/**
 * @file test_ui_clutter_main.cpp
 * @brief Custom GTest main that initialises QApplication before running GUI tests.
 *
 * Qt widgets require a QApplication instance to exist before any QWidget is
 * constructed. This replaces the default GTest main so we can create
 * QApplication first, then hand control to GTest.
 */

#include <gtest/gtest.h>
#include <QApplication>

int main(int argc, char* argv[]) {
    // QApplication must be created before any QWidget
    QApplication app(argc, argv);

    // Use offscreen platform if no display is available (CI / headless)
    // Can also be set via QT_QPA_PLATFORM=offscreen environment variable.

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
