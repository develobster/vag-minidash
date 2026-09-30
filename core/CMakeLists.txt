cmake_minimum_required(VERSION 3.16)
project(minidash_core C)
set(CMAKE_C_STANDARD 11)
# Same physical sources used by ESP-IDF; no duplicated target source trees.
set(MD_CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../firmware/main)
add_library(minidash-core STATIC
    ${MD_CORE_DIR}/ui_core.c ${MD_CORE_DIR}/ui_theme.c
    ${MD_CORE_DIR}/ui_i18n.c ${MD_CORE_DIR}/ui_model.c
    ${MD_CORE_DIR}/ui_platform.c ${MD_CORE_DIR}/ui_configuration.c
    ${MD_CORE_DIR}/ui_touch_map.c ${MD_CORE_DIR}/display_serial_protocol.c)
target_include_directories(minidash-core PUBLIC ${MD_CORE_DIR})
if(NOT MSVC)
    target_compile_options(minidash-core PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(minidash-core PUBLIC m)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(minidash-core-tests tests.c ${MD_CORE_DIR}/ui_platform_selftest.c)
    target_link_libraries(minidash-core-tests PRIVATE minidash-core)
    add_test(NAME portable-core COMMAND minidash-core-tests)
    add_executable(minidash-backlight-tests test_backlight.c ${MD_CORE_DIR}/backlight_controller.c)
    target_include_directories(minidash-backlight-tests PRIVATE backlight_stubs ${MD_CORE_DIR})
    add_test(NAME backlight-driver-contract COMMAND minidash-backlight-tests)
    set(MD_JSON_SOURCE_DIR "" CACHE PATH "Optional cJSON source directory for editor contract tests")
    if(MD_JSON_SOURCE_DIR)
        add_executable(minidash-json-tests test_json.c
            ${MD_CORE_DIR}/ui_configuration_json.c ${MD_JSON_SOURCE_DIR}/cJSON.c)
        target_include_directories(minidash-json-tests PRIVATE ${MD_JSON_SOURCE_DIR})
        target_link_libraries(minidash-json-tests PRIVATE minidash-core)
        add_test(NAME configuration-json COMMAND minidash-json-tests)
    endif()
endif()
