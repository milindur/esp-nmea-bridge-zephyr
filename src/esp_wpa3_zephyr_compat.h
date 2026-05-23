/*
 * Compatibility shims for ESP-IDF WPA3 SoftAP code paths that still refer to
 * FreeRTOS handle typedefs while built in Zephyr's ESP Wi-Fi port.
 */
#ifndef ESP_WPA3_ZEPHYR_COMPAT_H_
#define ESP_WPA3_ZEPHYR_COMPAT_H_

#include <stdint.h>

#ifndef pdPASS
#define pdPASS 1
#endif

#ifndef pdTRUE
#define pdTRUE 1
#endif

#ifndef portMAX_DELAY
#define portMAX_DELAY UINT32_MAX
#endif

typedef void *TaskHandle_t;
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;

#endif /* ESP_WPA3_ZEPHYR_COMPAT_H_ */
