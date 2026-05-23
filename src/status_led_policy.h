#ifndef STATUS_LED_POLICY_H_
#define STATUS_LED_POLICY_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STATUS_LED_DISCONNECTED_PERIOD_MS 2000U
#define STATUS_LED_DISCONNECTED_ON_MS 1000U
#define STATUS_LED_DISCONNECTED_RED 16U

struct status_led_rgb {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/**
 * Render the disconnected NMEA connection state base pattern.
 *
 * The disconnected base state is a dim red slow blink. This policy function is
 * independent from Zephyr devices so it can be unit tested on a host.
 */
struct status_led_rgb status_led_policy_render_disconnected(uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif /* STATUS_LED_POLICY_H_ */
