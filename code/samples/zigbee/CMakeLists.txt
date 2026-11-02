cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("b-parasite Zigbee sample")

include_directories(src)

get_filename_component(PRST_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(PRST_IDENTITY_SCRIPT "${PRST_SOURCE_ROOT}/code/scripts/firmware_identity.py")
set(PRST_IDENTITY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PRST_GENERATED_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/zephyr/.config")
file(GLOB_RECURSE PRST_IDENTITY_INPUTS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${PRST_SOURCE_ROOT}/code/prstlib/src/*.c"
  "${PRST_SOURCE_ROOT}/code/prstlib/src/*.h"
  "${PRST_SOURCE_ROOT}/code/prstlib/include/*.c"
  "${PRST_SOURCE_ROOT}/code/prstlib/include/*.h"
  "${PRST_SOURCE_ROOT}/code/prstlib/boards/*"
  "${PRST_SOURCE_ROOT}/code/prstlib/dts/*"
  "${PRST_SOURCE_ROOT}/code/prstlib/zephyr/*"
)
file(GLOB PRST_IDENTITY_CONFIG_INPUTS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Kconfig*"
  "${CMAKE_CURRENT_SOURCE_DIR}/prj*.conf"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.overlay"
  "${PRST_SOURCE_ROOT}/code/prstlib/Kconfig*"
  "${PRST_SOURCE_ROOT}/code/prstlib/prj*.conf"
  "${PRST_SOURCE_ROOT}/code/prstlib/*.overlay"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${PRST_IDENTITY_INPUTS} ${PRST_IDENTITY_CONFIG_INPUTS}
  "${PRST_SOURCE_ROOT}/code/prstlib/CMakeLists.txt"
  "${PRST_GENERATED_CONFIG}" "${PRST_IDENTITY_SCRIPT}"
)
execute_process(
  COMMAND ${PYTHON_EXECUTABLE} "${PRST_IDENTITY_SCRIPT}"
    --source-root "${PRST_SOURCE_ROOT}"
    --config "${PRST_GENERATED_CONFIG}"
    --output-dir "${PRST_IDENTITY_DIR}"
    --metadata "zephyr=${KERNEL_VERSION_STRING}"
    --metadata "zephyr_sdk=${SDK_VERSION}"
    --metadata "compiler=${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
  RESULT_VARIABLE PRST_IDENTITY_RESULT
  OUTPUT_VARIABLE PRST_IDENTITY
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT PRST_IDENTITY_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to generate firmware build identity")
endif()
message(STATUS "b-parasite firmware identity: ${PRST_IDENTITY}")
target_include_directories(app PRIVATE "${PRST_IDENTITY_DIR}")

target_sources(app PRIVATE
  src/main.c
  src/boot_loop_guard.c
  src/debug_counters.c
  src/double_reset_detector.c
  src/factory_reset.c
  src/flash_fs.c
  src/prst_zb_attrs.c
  src/prst_zb_soil_moisture_defs.c
  src/prst_zb_soil_calibration.c
  src/restart_handler.c
  src/watchdog.c
)

target_link_libraries(app PUBLIC prstlib)
