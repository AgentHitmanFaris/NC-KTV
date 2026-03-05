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
