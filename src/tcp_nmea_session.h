#ifndef TCP_NMEA_SESSION_H_
#define TCP_NMEA_SESSION_H_

enum tcp_nmea_session_result {
	TCP_NMEA_SESSION_ENDED_PEER_CLOSED,
	TCP_NMEA_SESSION_ENDED_SEND_FAILED,
	TCP_NMEA_SESSION_ENDED_SINK_UNAVAILABLE,
	TCP_NMEA_SESSION_ENDED_RX_FAILED,
};

enum tcp_nmea_session_result tcp_nmea_session_run(int fd, const char *sink_name);

#endif /* TCP_NMEA_SESSION_H_ */
