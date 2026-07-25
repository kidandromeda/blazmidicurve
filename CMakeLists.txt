cmake_minimum_required(VERSION 3.16)
project(BlazMidiCurve VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 IMPORTED_TARGET lv2)
pkg_check_modules(X11 IMPORTED_TARGET x11)
pkg_check_modules(CAIRO IMPORTED_TARGET cairo)

if(NOT LV2_FOUND)
    message(FATAL_ERROR "lv2-dev not found (apt install lv2-dev)")
endif()
if(NOT X11_FOUND)
    message(FATAL_ERROR "libx11-dev not found (apt install libx11-dev)")
endif()
if(NOT CAIRO_FOUND)
    message(FATAL_ERROR "libcairo2-dev not found (apt install libcairo2-dev)")
endif()

add_library(midicurve_lv2 SHARED
    src/lv2/MidiCurvePlugin.cpp
    src/lv2/MidiCurveUI.cpp
    src/lv2/FileDialog.cpp
    src/core/Curve.cpp
    src/core/Interpolator.cpp
)

target_include_directories(midicurve_lv2 PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

target_link_libraries(midicurve_lv2 PRIVATE
    ${LV2_LINK_LIBRARIES}
    ${X11_LINK_LIBRARIES}
    ${CAIRO_LINK_LIBRARIES}
)

set_target_properties(midicurve_lv2 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/blazmidicurve.lv2"
    OUTPUT_NAME "midicurve_lv2"
    PREFIX ""
)

file(COPY ttl/manifest.ttl DESTINATION "${CMAKE_BINARY_DIR}/blazmidicurve.lv2")

# Asegurar que el TTL se actualiza en cada build (file(COPY) solo corre en configure)
add_custom_command(TARGET midicurve_lv2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/ttl/manifest.ttl"
        "${CMAKE_BINARY_DIR}/blazmidicurve.lv2/manifest.ttl"
)

add_custom_target(install-user
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/blazmidicurve.lv2"
        "$ENV{HOME}/.lv2/blazmidicurve.lv2"
)
