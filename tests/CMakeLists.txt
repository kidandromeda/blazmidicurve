# -----------------------------------------------------------------------------
# tests/CMakeLists.txt
#
# Autor:      blazman
# Licencia:   MIT
# Versión:    2.0.0
# Fecha:      2026-11-15
#
# Configuración de pruebas unitarias y de integración.
# -----------------------------------------------------------------------------

find_package(PkgConfig REQUIRED)
pkg_check_modules(LILV IMPORTED_TARGET lilv-0)

# ---- Unit tests (compile with core sources) ----
set(UNIT_TESTS
    TestCurve
    TestInterpolator
    TestMidiProcessor
)

foreach(test_name ${UNIT_TESTS})
    add_executable(${test_name} ${test_name}.cpp
        ../src/core/Curve.cpp
        ../src/core/Interpolator.cpp)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${LV2_INCLUDE_DIRS})
    target_link_libraries(${test_name} PRIVATE m)
    target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# ---- Integration test (loads the real plugin via lilv) ----
if(LILV_FOUND)
    add_executable(TestIntegration TestIntegration.cpp)
    target_include_directories(TestIntegration PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${LV2_INCLUDE_DIRS}
        ${LILV_INCLUDE_DIRS})
    target_link_libraries(TestIntegration PRIVATE
        ${LILV_LINK_LIBRARIES}
        m)
    target_compile_options(TestIntegration PRIVATE -Wall -Wextra -Wpedantic)

    # Create a clean LV2 bundle directory for testing
    set(TEST_LV2_DIR "${CMAKE_BINARY_DIR}/test_lv2_bundles")
    add_custom_command(TARGET TestIntegration POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/blazmidicurve.lv2"
            "${TEST_LV2_DIR}/blazmidicurve.lv2")
    add_test(NAME TestIntegration
        COMMAND ${CMAKE_COMMAND} -E env
            LV2_PATH=${TEST_LV2_DIR}
            $<TARGET_FILE:TestIntegration>)
endif()
