# ─── NC-KTV Unit Tests ───────────────────────────────────────────────────────

enable_testing()

# ── Google Test via FetchContent (no vcpkg needed) ────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
# Prevent overriding our project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(TEST_SOURCES
    test_lyrics_data.cpp
    test_timeline_data.cpp
    test_audio_clock.cpp
    test_parsers.cpp
    test_config.cpp
    test_effect_compositor.cpp
)

add_executable(ncktv_tests ${TEST_SOURCES})

target_link_libraries(ncktv_tests
    PRIVATE
        ncktv_core
        Qt6::Core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(ncktv_tests)

# ─── NC-KTV GUI Tests (widget tree / UI clutter exploration) ─────────────────
# Requires QApplication — compiles the GUI sources directly (no main.cpp).
# NCKTV_GUI_SOURCES_NO_MAIN and NCKTV_GUI_INCLUDE_DIR are exported by src/gui/CMakeLists.txt.

# Qt6::Test provides QSignalSpy and QTest utilities
find_package(Qt6 QUIET COMPONENTS Test)

# Prepend the GUI source directory so relative paths resolve correctly
set(GUI_SOURCES_ABSOLUTE "")
foreach(src ${NCKTV_GUI_SOURCES_NO_MAIN})
    if(IS_ABSOLUTE "${src}")
        list(APPEND GUI_SOURCES_ABSOLUTE "${src}")
    else()
        list(APPEND GUI_SOURCES_ABSOLUTE "${NCKTV_GUI_INCLUDE_DIR}/${src}")
    endif()
endforeach()

add_executable(ncktv_gui_tests
    test_ui_clutter.cpp
    test_ui_preservation.cpp
    test_ui_premiere.cpp
    test_ui_clutter_main.cpp
    ${GUI_SOURCES_ABSOLUTE}
)

# Disable AUTOMOC compiler predefines regeneration to avoid MinGW PATH issue
# in CMake's autogen subprocess on Windows.
set_target_properties(ncktv_gui_tests PROPERTIES
    AUTOMOC_COMPILER_PREDEFINES OFF
)

target_include_directories(ncktv_gui_tests PRIVATE
    ${NCKTV_GUI_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ncktv_gui_tests
    PRIVATE
        ncktv_core
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        Qt6::Network
        GTest::gtest
)

# Link Qt6::Test if available (provides QSignalSpy, QTest::mouseClick)
if(TARGET Qt6::Test)
    target_link_libraries(ncktv_gui_tests PRIVATE Qt6::Test)
else()
    # Fallback: add the QtTest include path manually so QSignalSpy header is found
    target_include_directories(ncktv_gui_tests PRIVATE
        "${Qt6_DIR}/../../../include/QtTest"
    )
    # Find and link the QtTest library directly
    find_library(QT6_TEST_LIB NAMES Qt6Test libQt6Test
        HINTS "${Qt6_DIR}/../../../lib" "${Qt6_DIR}/../../../bin"
    )
    if(QT6_TEST_LIB)
        target_link_libraries(ncktv_gui_tests PRIVATE "${QT6_TEST_LIB}")
    endif()
endif()

if(WIN32 AND NOT MSVC AND TARGET onnxruntime_implib)
    add_dependencies(ncktv_gui_tests onnxruntime_implib)
endif()

gtest_discover_tests(ncktv_gui_tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    DISCOVERY_MODE PRE_TEST
)
