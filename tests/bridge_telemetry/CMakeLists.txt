cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bridge_telemetry_test)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_compile_definitions(app PRIVATE CONFIG_ESP_NMEA_BRIDGE_NMEA_FRAME_MAX_LEN=128)
target_sources(app PRIVATE
  src/main.c
  ../../src/bridge_telemetry.c
)
