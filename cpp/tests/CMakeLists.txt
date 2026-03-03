# ─── NC-KTV Unit Tests ───────────────────────────────────────────────────────

enable_testing()

set(TEST_SOURCES
    test_lyrics_data.cpp
    test_timeline_data.cpp
    test_audio_clock.cpp
    test_parsers.cpp
    test_config.cpp
    test_effect_compositor.cpp
)

add_executable(ncktv_tests ${TEST_SOURCES})

find_package(GTest REQUIRED)

target_link_libraries(ncktv_tests
    PRIVATE
        ncktv_core
        Qt6::Core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(ncktv_tests)
