#include "tcp_signalk_client.h"

#include "nmea_bridge.h"
#include "wifi_manager.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

LOG_MODULE_REGISTER(tcp_signalk_client, LOG_LEVEL_INF);

static bool started;

static int connect_gateway(struct net_in_addr *gw)
{
	int fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0) {
		LOG_ERR("uplink socket failed: errno=%d", errno);
		return -errno;
	}

	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(CONFIG_ESP_SERIAL_BRIDGE_TCP_PORT);
	addr.sin_addr = *gw;

	char gw_buf[NET_IPV4_ADDR_LEN];
	net_addr_ntop(AF_INET, gw, gw_buf, sizeof(gw_buf));
	LOG_INF("SignalK uplink connecting to %s:%d", gw_buf,
		CONFIG_ESP_SERIAL_BRIDGE_TCP_PORT);

	if (zsock_connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		int ret = -errno;
		LOG_WRN("SignalK connect failed: errno=%d", errno);
		(void)zsock_close(fd);
		return ret;
	}

	int flags = zsock_fcntl(fd, ZVFS_F_GETFL, 0);
	(void)zsock_fcntl(fd, ZVFS_F_SETFL, flags | ZVFS_O_NONBLOCK);

	LOG_INF("SignalK uplink connected");
	return fd;
}

static void uplink_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	uint32_t backoff_s = 1;

	for (;;) {
		struct net_in_addr gw;

		while (!wifi_manager_get_sta_gateway(&gw)) {
			k_sleep(K_SECONDS(2));
		}

		int fd = connect_gateway(&gw);
		if (fd < 0) {
			k_sleep(K_SECONDS(backoff_s));
			backoff_s = MIN(backoff_s * 2, 30U);
			continue;
		}

		backoff_s = 1;
		int sink_id = nmea_bridge_sink_register("signalk-uplink");
		if (sink_id < 0) {
			LOG_ERR("Unable to register SignalK sink");
			(void)zsock_close(fd);
			k_sleep(K_SECONDS(5));
			continue;
		}

		struct nmea_frame frame;
		while (wifi_manager_sta_ready()) {
			if (nmea_bridge_sink_get(sink_id, &frame, K_SECONDS(1)) != 0) {
				continue;
			}

			ssize_t sent = zsock_send(fd, frame.data, frame.len, 0);
			if (sent != frame.len) {
				LOG_WRN("SignalK send failed/partial: sent=%zd len=%u errno=%d",
					sent, frame.len, errno);
				break;
			}
		}

		nmea_bridge_sink_unregister(sink_id);
		(void)zsock_close(fd);
		LOG_INF("SignalK uplink disconnected; reconnecting");
		k_sleep(K_SECONDS(backoff_s));
		backoff_s = MIN(backoff_s * 2, 30U);
	}
}

static struct k_thread signalk_uplink_thread;
K_THREAD_STACK_DEFINE(signalk_uplink_stack, 4096);

int tcp_signalk_client_start(void)
{
	if (!IS_ENABLED(CONFIG_ESP_SERIAL_BRIDGE_SIGNALK_UPLINK_ENABLE)) {
		LOG_INF("SignalK uplink disabled");
		return 0;
	}

	if (!started) {
		started = true;
		k_thread_create(&signalk_uplink_thread, signalk_uplink_stack,
				K_THREAD_STACK_SIZEOF(signalk_uplink_stack), uplink_thread,
				NULL, NULL, NULL, 7, 0, K_NO_WAIT);
	}

	return 0;
}
