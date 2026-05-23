#include "wifi_manager.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(wifi_manager, LOG_LEVEL_INF);

#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"

#define WIFI_EVENT_MASK (NET_EVENT_WIFI_CONNECT_RESULT | \
			 NET_EVENT_WIFI_DISCONNECT_RESULT | \
			 NET_EVENT_WIFI_AP_ENABLE_RESULT | \
			 NET_EVENT_WIFI_AP_DISABLE_RESULT | \
			 NET_EVENT_WIFI_AP_STA_CONNECTED | \
			 NET_EVENT_WIFI_AP_STA_DISCONNECTED | \
			 NET_EVENT_IPV4_ADDR_ADD)

static struct net_if *ap_iface;
static struct net_if *sta_iface;
static struct net_mgmt_event_callback wifi_cb;
static bool sta_connected;
static bool sta_has_addr;

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		sta_connected = true;
		LOG_INF("STA connected to %s", CONFIG_ESP_SERIAL_BRIDGE_STA_SSID);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		sta_connected = false;
		sta_has_addr = false;
		LOG_INF("STA disconnected from %s", CONFIG_ESP_SERIAL_BRIDGE_STA_SSID);
		break;
	case NET_EVENT_IPV4_ADDR_ADD:
		if (iface == sta_iface) {
			struct net_in_addr gw = net_if_ipv4_get_gw(sta_iface);
			char gw_buf[NET_IPV4_ADDR_LEN];

			sta_has_addr = true;
			net_addr_ntop(AF_INET, &gw, gw_buf, sizeof(gw_buf));
			LOG_INF("STA IPv4 ready; gateway %s", gw_buf);
		}
		break;
	case NET_EVENT_WIFI_AP_ENABLE_RESULT:
		LOG_INF("SoftAP enabled");
		break;
	case NET_EVENT_WIFI_AP_DISABLE_RESULT:
		LOG_INF("SoftAP disabled");
		break;
	case NET_EVENT_WIFI_AP_STA_CONNECTED: {
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;
		LOG_INF("AP client joined: " MACSTR, sta_info->mac[0], sta_info->mac[1],
			sta_info->mac[2], sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;
		LOG_INF("AP client left: " MACSTR, sta_info->mac[0], sta_info->mac[1],
			sta_info->mac[2], sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	default:
		break;
	}
}

static int start_dhcp_server(void)
{
	static struct net_in_addr addr;
	static struct net_in_addr netmask;
	struct net_in_addr pool_start;

	if (net_addr_pton(AF_INET, CONFIG_ESP_SERIAL_BRIDGE_AP_IP_ADDRESS, &addr) != 0) {
		LOG_ERR("Invalid AP IP: %s", CONFIG_ESP_SERIAL_BRIDGE_AP_IP_ADDRESS);
		return -EINVAL;
	}

	if (net_addr_pton(AF_INET, CONFIG_ESP_SERIAL_BRIDGE_AP_NETMASK, &netmask) != 0) {
		LOG_ERR("Invalid AP netmask: %s", CONFIG_ESP_SERIAL_BRIDGE_AP_NETMASK);
		return -EINVAL;
	}

	net_if_ipv4_set_gw(ap_iface, &addr);
	if (net_if_ipv4_addr_add(ap_iface, &addr, NET_ADDR_MANUAL, 0) == NULL) {
		LOG_WRN("Unable to add AP IPv4 address; it may already be configured");
	}

	if (!net_if_ipv4_set_netmask_by_addr(ap_iface, &addr, &netmask)) {
		LOG_ERR("Unable to set AP netmask");
		return -EIO;
	}

	pool_start = addr;
	pool_start.s4_addr[3] += 10;

	int ret = net_dhcpv4_server_start(ap_iface, &pool_start);
	if (ret != 0) {
		LOG_ERR("DHCPv4 server start failed: %d", ret);
		return ret;
	}

	LOG_INF("DHCPv4 server started on %s", CONFIG_ESP_SERIAL_BRIDGE_AP_IP_ADDRESS);
	return 0;
}

static int enable_ap(void)
{
	if (!ap_iface) {
		return -ENODEV;
	}

	static struct wifi_connect_req_params ap_config;

	memset(&ap_config, 0, sizeof(ap_config));
	ap_config.ssid = (const uint8_t *)CONFIG_ESP_SERIAL_BRIDGE_AP_SSID;
	ap_config.ssid_length = strlen(CONFIG_ESP_SERIAL_BRIDGE_AP_SSID);
	ap_config.psk = (const uint8_t *)CONFIG_ESP_SERIAL_BRIDGE_AP_PSK;
	ap_config.psk_length = strlen(CONFIG_ESP_SERIAL_BRIDGE_AP_PSK);
	ap_config.channel = WIFI_CHANNEL_ANY;
	ap_config.band = WIFI_FREQ_BAND_2_4_GHZ;
	ap_config.security = ap_config.psk_length == 0 ? WIFI_SECURITY_TYPE_NONE : WIFI_SECURITY_TYPE_PSK;

	int ret = start_dhcp_server();
	if (ret != 0) {
		return ret;
	}

	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, ap_iface, &ap_config, sizeof(ap_config));
	if (ret != 0) {
		LOG_ERR("SoftAP enable failed: %d", ret);
		return ret;
	}

	LOG_INF("SoftAP enable requested: ssid=%s security=%s", CONFIG_ESP_SERIAL_BRIDGE_AP_SSID,
		ap_config.security == WIFI_SECURITY_TYPE_NONE ? "open" : "WPA2-PSK");
	return 0;
}

static int connect_sta(void)
{
	if (!sta_iface) {
		return -ENODEV;
	}

	if (strlen(CONFIG_ESP_SERIAL_BRIDGE_STA_SSID) == 0) {
		LOG_WRN("STA SSID empty; STA connect skipped");
		return 0;
	}

	struct wifi_connect_req_params sta_config = { 0 };

	sta_config.ssid = (const uint8_t *)CONFIG_ESP_SERIAL_BRIDGE_STA_SSID;
	sta_config.ssid_length = strlen(CONFIG_ESP_SERIAL_BRIDGE_STA_SSID);
	sta_config.psk = (const uint8_t *)CONFIG_ESP_SERIAL_BRIDGE_STA_PSK;
	sta_config.psk_length = strlen(CONFIG_ESP_SERIAL_BRIDGE_STA_PSK);
	sta_config.security = sta_config.psk_length == 0 ? WIFI_SECURITY_TYPE_NONE : WIFI_SECURITY_TYPE_PSK;
	sta_config.channel = WIFI_CHANNEL_ANY;
	sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config, sizeof(sta_config));
	if (ret != 0) {
		LOG_ERR("STA connect request failed: %d", ret);
		return ret;
	}

	LOG_INF("STA connect requested: ssid=%s", CONFIG_ESP_SERIAL_BRIDGE_STA_SSID);
	return 0;
}

int wifi_manager_start(void)
{
	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler, WIFI_EVENT_MASK);
	net_mgmt_add_event_callback(&wifi_cb);

	ap_iface = net_if_get_wifi_sap();
	sta_iface = net_if_get_wifi_sta();

	LOG_INF("Wi-Fi interfaces: ap=%p sta=%p", ap_iface, sta_iface);

	int ret = 0;

	if (IS_ENABLED(CONFIG_ESP_SERIAL_BRIDGE_AP_ENABLE)) {
		ret = enable_ap();
		if (ret != 0) {
			LOG_ERR("AP startup failed: %d", ret);
		}
	}

	if (IS_ENABLED(CONFIG_ESP_SERIAL_BRIDGE_STA_ENABLE)) {
		int sta_ret = connect_sta();
		if (sta_ret != 0 && ret == 0) {
			ret = sta_ret;
		}
	}

	return ret;
}

struct net_if *wifi_manager_sta_iface(void)
{
	return sta_iface;
}

bool wifi_manager_sta_ready(void)
{
	return sta_connected && sta_has_addr;
}

bool wifi_manager_get_sta_gateway(struct net_in_addr *gw)
{
	if (!sta_iface || gw == NULL || !wifi_manager_sta_ready()) {
		return false;
	}

	*gw = net_if_ipv4_get_gw(sta_iface);
	return !net_ipv4_is_addr_unspecified(gw);
}
