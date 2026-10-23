# SPDX-License-Identifier: Apache-2.0

# Build a locally patched copy; west-managed dependencies stay untouched.
set(mcan_source "${ZEPHYR_BASE}/drivers/can/can_mcan.c")
set(mcan_generator "${CMAKE_CURRENT_LIST_DIR}/../scripts/mcan_tx_fifo.py")
set(mcan_fifo_source "${CMAKE_CURRENT_BINARY_DIR}/can_mcan_tx_fifo.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${mcan_source}" "${mcan_generator}")
execute_process(
  COMMAND "${PYTHON_EXECUTABLE}" "${mcan_generator}"
    "${mcan_source}" "${mcan_fifo_source}"
  RESULT_VARIABLE mcan_result
  ERROR_VARIABLE mcan_error)
if(NOT mcan_result EQUAL 0)
  message(FATAL_ERROR "Could not prepare local M_CAN FIFO driver: ${mcan_error}")
endif()

get_target_property(can_sources drivers__can SOURCES)
set(mcan_replaced FALSE)
foreach(source IN LISTS can_sources)
  get_filename_component(source_name "${source}" NAME)
  if(source_name STREQUAL "can_mcan.c")
    list(REMOVE_ITEM can_sources "${source}")
    set(mcan_replaced TRUE)
  endif()
endforeach()
if(NOT mcan_replaced)
  message(FATAL_ERROR "Could not find Zephyr M_CAN driver to replace")
endif()
set_property(TARGET drivers__can PROPERTY SOURCES "${can_sources}")
target_sources(drivers__can PRIVATE "${mcan_fifo_source}")
