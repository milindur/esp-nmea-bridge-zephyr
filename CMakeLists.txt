cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(esp_serial_bridge)

# Work around ESP-IDF WPA3 SoftAP sources that still reference FreeRTOS handle
# typedefs when compiled through Zephyr's ESP Wi-Fi port.
zephyr_compile_definitions(
  TaskHandle_t=void*
  QueueHandle_t=void*
  SemaphoreHandle_t=void*
  pdPASS=1
  pdTRUE=1
  portMAX_DELAY=0xffffffffU
)

target_sources(app PRIVATE
  src/main.c
  src/nmea_bridge.c
  src/uart_nmea.c
  src/wifi_manager.c
  src/tcp_nmea_server.c
  src/tcp_signalk_client.c
)
