#ifndef NMEA_BRIDGE_H_
#define NMEA_BRIDGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

struct nmea_frame {
	uint16_t len;
	uint8_t data[CONFIG_ESP_NMEA_BRIDGE_NMEA_FRAME_MAX_LEN];
};

struct nmea_bridge_stats {
	uint32_t frames_in;
	uint32_t ingest_dropped_oldest;
	uint32_t sink_dropped_oldest;
	uint32_t publish_no_sinks;
	uint32_t publish_invalid;
	uint32_t publish_oversize;
};

struct nmea_sink_stats {
	uint32_t queued;
	uint32_t dropped_oldest;
	uint32_t consumed;
};

struct nmea_bridge_sink {
	int id;
};

void nmea_bridge_init(void);
int nmea_bridge_publish_frame(const uint8_t *data, size_t len);
int nmea_bridge_sink_open(struct nmea_bridge_sink *sink, const char *name);
void nmea_bridge_sink_close(struct nmea_bridge_sink *sink);
int nmea_bridge_sink_take(struct nmea_bridge_sink *sink, struct nmea_frame *frame,
				  k_timeout_t timeout);
void nmea_bridge_get_stats(struct nmea_bridge_stats *stats);
void nmea_bridge_get_sink_stats(const struct nmea_bridge_sink *sink,
				       struct nmea_sink_stats *stats);

#endif /* NMEA_BRIDGE_H_ */
