#include "status_led_policy.h"

struct status_led_rgb status_led_policy_render_disconnected(uint32_t elapsed_ms)
{
	uint32_t phase_ms = elapsed_ms % STATUS_LED_DISCONNECTED_PERIOD_MS;

	if (phase_ms >= STATUS_LED_DISCONNECTED_ON_MS) {
		return (struct status_led_rgb){ 0, 0, 0 };
	}

	return (struct status_led_rgb){ STATUS_LED_DISCONNECTED_RED, 0, 0 };
}
