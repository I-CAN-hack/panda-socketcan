# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.13.1)
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../deps/cannectivity/app)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(panda_socketcan LANGUAGES C)

# Reuse the upstream USB protocol, LEDs and timestamp implementation.
set(cannectivity_app ${CMAKE_CURRENT_SOURCE_DIR}/../deps/cannectivity/app)
target_include_directories(app PRIVATE ${cannectivity_app}/src)
target_sources(app PRIVATE main.c ${cannectivity_app}/src/usb.c)
target_sources_ifdef(CONFIG_CANNECTIVITY_LED app PRIVATE ${cannectivity_app}/src/led.c)
target_sources_ifdef(CONFIG_CANNECTIVITY_TIMESTAMP_COUNTER app PRIVATE
  ${cannectivity_app}/src/timestamp-counter.c)
if(CONFIG_BOARD_PANDA_BLACK OR CONFIG_BOARD_PANDA_RED)
  target_sources(app PRIVATE harness.c)
endif()
