#include "nmea_bridge.h"

#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(nmea_bridge, LOG_LEVEL_INF);

#define MAX_SINKS (CONFIG_ESP_SERIAL_BRIDGE_TCP_NMEA_SERVER_MAX_CLIENTS + 1)

K_MSGQ_DEFINE(ingest_msgq, sizeof(struct nmea_frame),
	     CONFIG_ESP_SERIAL_BRIDGE_INGEST_QUEUE_DEPTH, 4);

struct sink_slot {
	bool active;
	const char *name;
	struct k_msgq msgq;
	char buffer[CONFIG_ESP_SERIAL_BRIDGE_SINK_QUEUE_DEPTH * sizeof(struct nmea_frame)];
	struct nmea_sink_stats stats;
};

static struct sink_slot sinks[MAX_SINKS];
static struct nmea_bridge_stats bridge_stats;
static struct k_mutex sinks_lock;

static void put_drop_oldest(struct k_msgq *msgq, const struct nmea_frame *frame,
			    uint32_t *drop_counter)
{
	struct nmea_frame old;

	if (k_msgq_put(msgq, frame, K_NO_WAIT) == 0) {
		return;
	}

	(void)k_msgq_get(msgq, &old, K_NO_WAIT);
	(*drop_counter)++;

	if (k_msgq_put(msgq, frame, K_NO_WAIT) != 0) {
		(*drop_counter)++;
	}
}

static void dispatcher_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	struct nmea_frame frame;

	for (;;) {
		k_msgq_get(&ingest_msgq, &frame, K_FOREVER);

		k_mutex_lock(&sinks_lock, K_FOREVER);
		bool any = false;

		for (int i = 0; i < ARRAY_SIZE(sinks); i++) {
			if (!sinks[i].active) {
				continue;
			}

			uint32_t before = sinks[i].stats.dropped_oldest;
			put_drop_oldest(&sinks[i].msgq, &frame, &sinks[i].stats.dropped_oldest);
			if (sinks[i].stats.dropped_oldest != before) {
				bridge_stats.sink_dropped_oldest +=
					sinks[i].stats.dropped_oldest - before;
			}
			sinks[i].stats.queued++;
			any = true;
		}

		if (!any) {
			bridge_stats.publish_no_sinks++;
		}
		k_mutex_unlock(&sinks_lock);
	}
}

K_THREAD_DEFINE(nmea_dispatcher_tid, 2048, dispatcher_thread, NULL, NULL, NULL,
	       5, 0, 0);

void nmea_bridge_init(void)
{
	k_mutex_init(&sinks_lock);

	for (int i = 0; i < ARRAY_SIZE(sinks); i++) {
		k_msgq_init(&sinks[i].msgq, sinks[i].buffer, sizeof(struct nmea_frame),
			    CONFIG_ESP_SERIAL_BRIDGE_SINK_QUEUE_DEPTH);
	}

	LOG_INF("NMEA bridge ready: frame_len=%d ingest_depth=%d sink_depth=%d sinks=%d",
		CONFIG_ESP_SERIAL_BRIDGE_NMEA_FRAME_MAX_LEN,
		CONFIG_ESP_SERIAL_BRIDGE_INGEST_QUEUE_DEPTH,
		CONFIG_ESP_SERIAL_BRIDGE_SINK_QUEUE_DEPTH, MAX_SINKS);
}

int nmea_bridge_sink_register(const char *name)
{
	k_mutex_lock(&sinks_lock, K_FOREVER);

	for (int i = 0; i < ARRAY_SIZE(sinks); i++) {
		if (!sinks[i].active) {
			sinks[i].active = true;
			sinks[i].name = name;
			memset(&sinks[i].stats, 0, sizeof(sinks[i].stats));
			k_msgq_purge(&sinks[i].msgq);
			k_mutex_unlock(&sinks_lock);
			LOG_INF("NMEA sink registered: %s id=%d", name ? name : "?", i);
			return i;
		}
	}

	k_mutex_unlock(&sinks_lock);
	return -ENOMEM;
}

void nmea_bridge_sink_unregister(int sink_id)
{
	if (sink_id < 0 || sink_id >= ARRAY_SIZE(sinks)) {
		return;
	}

	k_mutex_lock(&sinks_lock, K_FOREVER);
	sinks[sink_id].active = false;
	sinks[sink_id].name = NULL;
	k_msgq_purge(&sinks[sink_id].msgq);
	k_mutex_unlock(&sinks_lock);
	LOG_INF("NMEA sink unregistered: id=%d", sink_id);
}

int nmea_bridge_sink_get(int sink_id, struct nmea_frame *frame, k_timeout_t timeout)
{
	if (sink_id < 0 || sink_id >= ARRAY_SIZE(sinks) || frame == NULL) {
		return -EINVAL;
	}

	int ret = k_msgq_get(&sinks[sink_id].msgq, frame, timeout);
	if (ret == 0) {
		sinks[sink_id].stats.consumed++;
	}

	return ret;
}

void nmea_bridge_publish(const uint8_t *data, size_t len)
{
	struct nmea_frame frame;

	if (data == NULL || len == 0) {
		return;
	}

	frame.len = MIN(len, sizeof(frame.data));
	memcpy(frame.data, data, frame.len);

	bridge_stats.frames_in++;
	put_drop_oldest(&ingest_msgq, &frame, &bridge_stats.ingest_dropped_oldest);
}

void nmea_bridge_get_stats(struct nmea_bridge_stats *stats)
{
	if (stats != NULL) {
		*stats = bridge_stats;
	}
}

void nmea_bridge_get_sink_stats(int sink_id, struct nmea_sink_stats *stats)
{
	if (stats == NULL || sink_id < 0 || sink_id >= ARRAY_SIZE(sinks)) {
		return;
	}

	*stats = sinks[sink_id].stats;
}
