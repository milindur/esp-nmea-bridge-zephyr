#include <zephyr/ztest.h>

#include "status_led_policy.h"

ZTEST(status_led_policy, test_disconnected_is_dim_red_during_on_phase)
{
	struct status_led_rgb rgb = status_led_policy_render_disconnected(0U);

	zassert_equal(rgb.r, STATUS_LED_DISCONNECTED_RED);
	zassert_equal(rgb.g, 0U);
	zassert_equal(rgb.b, 0U);
}

ZTEST(status_led_policy, test_disconnected_is_off_during_off_phase)
{
	struct status_led_rgb rgb =
		status_led_policy_render_disconnected(STATUS_LED_DISCONNECTED_ON_MS);

	zassert_equal(rgb.r, 0U);
	zassert_equal(rgb.g, 0U);
	zassert_equal(rgb.b, 0U);
}

ZTEST(status_led_policy, test_disconnected_repeats_after_period)
{
	struct status_led_rgb rgb =
		status_led_policy_render_disconnected(STATUS_LED_DISCONNECTED_PERIOD_MS);

	zassert_equal(rgb.r, STATUS_LED_DISCONNECTED_RED);
	zassert_equal(rgb.g, 0U);
	zassert_equal(rgb.b, 0U);
}

ZTEST_SUITE(status_led_policy, NULL, NULL, NULL, NULL, NULL);
