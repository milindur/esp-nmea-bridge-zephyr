#ifndef TCP_NMEA_CLIENT_H_
#define TCP_NMEA_CLIENT_H_

#ifdef CONFIG_ESP_NMEA_BRIDGE_TCP_NMEA_CLIENT_ENABLE
int tcp_nmea_client_start(void);
#else
static inline int tcp_nmea_client_start(void)
{
	return 0;
}
#endif

#endif /* TCP_NMEA_CLIENT_H_ */
