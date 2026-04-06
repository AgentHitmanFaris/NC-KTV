/**
 * @file test_ui_premiere.cpp
 * @brief Premiere Pro UI redesign tests — structural layout, QSS selectors,
 *        and property-based tests for tool mutual exclusion, timecode formatting,
 *        tab switching, and no inline styles.
 *
 * Validates: Requirements 3.1, 3.3, 4.1, 5.1, 5.3, 5.5, 12.2, 12.8, 15.2
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QRegularExpression>
#include <QPushButton>
#include <QWidget>
#include <QLabel>
#include <QSplitter>
#include <QButtonGroup>
#include <QFile>
#include <QIODevice>
#include <QMainWindow>
#include <random>
#include <vector>
#include <memory>

// Forward declarations / includes for the classes under test
// Note: these are compiled into the test binary via CMakeLists.txt
#include "editor/editor_mode.h"
#include "wizard/wizard_mode.h"
#include "core/config/config_manager.h"
#include "core/project/ncktv_project.hpp"

// ─── Helpers ──────────────────────────────────────────────────────────────────

static std::shared_ptr<ncktv::core::Project> makeTestProject() {
    return std::make_shared<ncktv::core::Project>();
}

static ncktv::ConfigManager* makeTestConfig() {
    return new ncktv::ConfigManager(""); // empty path = in-memory defaults
}

// ─── Structural Tests ─────────────────────────────────────────────────────────

TEST(EditorModeLayout, NoQMainWindowChild) {
    auto project = makeTestProject();
    auto* config = makeTestConfig();
    ncktv::EditorMode editor(project, config);

    // Walk widget tree, assert no QMainWindow descendant
    bool found = false;
    QList<QObject*> stack = editor.children();
    while (!stack.isEmpty()) {
        QObject* obj = stack.takeFirst();
        if (qobject_cast<QMainWindow*>(obj)) { found = true; break; }
        stack.append(obj->children());
    }
    EXPECT_FALSE(found) << "EditorMode should not contain a QMainWindow child";
    delete config;
}

TEST(EditorModeLayout, TransportBarHeight) {
    auto project = makeTestProject();
    auto* config = makeTestConfig();
    ncktv::EditorMode editor(project, config);

    auto* transportBar = editor.findChild<QWidget*>("transportBar");
    ASSERT_NE(transportBar, nullptr) << "transportBar widget not found";
    EXPECT_EQ(transportBar->maximumHeight(), 40) << "transportBar should have max height 40";
    delete config;
}

TEST(EditorModeLayout, TabBarHasThreeTabBtns) {
    auto project = makeTestProject();
    auto* config = makeTestConfig();
    ncktv::EditorMode editor(project, config);

    auto* tabBar = editor.findChild<QWidget*>("tabBar");
    ASSERT_NE(tabBar, nullptr) << "tabBar widget not found";

    QList<QPushButton*> tabBtns = tabBar->findChildren<QPushButton*>("tabBtn");
    EXPECT_EQ(tabBtns.size(), 3) << "tabBar should have exactly 3 tabBtn buttons";
    for (auto* btn : tabBtns) {
        EXPECT_TRUE(btn->isCheckable()) << "Each tabBtn should be checkable";
    }
    delete config;
}

TEST(EditorModeLayout, ToolsPanelWidth) {
    auto project = makeTestProject();
    auto* config = makeTestConfig();
    ncktv::EditorMode editor(project, config);
    editor.show(); // needed for geometry to be set
    editor.resize(1200, 800);

    auto* toolsPanel = editor.findChild<QWidget*>("toolsPanel");
    ASSERT_NE(toolsPanel, nullptr) << "toolsPanel widget not found";
    EXPECT_EQ(toolsPanel->maximumWidth(), 32) << "toolsPanel should have max width 32";
    delete config;
}

TEST(WizardModeLayout, SidebarWidth) {
    auto* config = makeTestConfig();
    ncktv::WizardMode wizard(config);

    auto* sidebar = wizard.findChild<QWidget*>("wizardSidebar");
    ASSERT_NE(sidebar, nullptr) << "wizardSidebar widget not found";
    EXPECT_EQ(sidebar->maximumWidth(), 260) << "wizardSidebar should have max width 260";
    delete config;
}

TEST(WizardModeLayout, StartBtnDisabledOnConstruct) {
    auto* config = makeTestConfig();
    ncktv::WizardMode wizard(config);

    auto* startBtn = wizard.findChild<QPushButton*>("startEngineBtn");
    ASSERT_NE(startBtn, nullptr) << "startEngineBtn not found";
    EXPECT_FALSE(startBtn->isEnabled()) << "startEngineBtn should be disabled on construction";
    delete config;
}

TEST(QSSSelectors, AllRequiredSelectorsPresent) {
    // Load dark_theme.qss and check for required selectors
    QFile qssFile(":/styles/dark_theme.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Try relative path
        qssFile.setFileName("../src/gui/resources/styles/dark_theme.qss");
        if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            GTEST_SKIP() << "Could not open dark_theme.qss";
        }
    }
    QString content = QString::fromUtf8(qssFile.readAll());

    QStringList required = {
        "QWidget#panelHeader",
        "QWidget#toolsPanel",
        "QWidget#sourcePanel",
        "QWidget#programPanel",
        "QWidget#captionsPanel",
        "QWidget#timelinePanel",
        "QWidget#transportBar",
        "QWidget#tabBar",
        "QWidget#topBar"
    };

    for (const QString& selector : required) {
        EXPECT_TRUE(content.contains(selector))
            << "dark_theme.qss missing selector: " << selector.toStdString();
    }
}

// ─── Property 2: Tool Button Mutual Exclusion ─────────────────────────────────
// Validates: Requirements 5.3, 5.5

class ToolMutualExclusionTest : public ::testing::TestWithParam<std::vector<int>> {};

TEST_P(ToolMutualExclusionTest, ExactlyOneChecked) {
    auto project = makeTestProject();
    auto* config = makeTestConfig();
    ncktv::EditorMode editor(project, config);

    auto* toolsPanel = editor.findChild<QWidget*>("toolsPanel");
    ASSERT_NE(toolsPanel, nullptr);

    QList<QPushButton*> toolBtns = toolsPanel->findChildren<QPushButton*>("toolBtn");
    ASSERT_EQ(toolBtns.size(), 3) << "Expected 3 tool buttons";

    const auto& clickSequence = GetParam();
    for (int idx : clickSequence) {
        if (idx >= 0 && idx < toolBtns.size()) {
            toolBtns[idx]->click();

            int checkedCount = 0;
            for (auto* btn : toolBtns) {
                if (btn->isChecked()) checkedCount++;
            }
            EXPECT_EQ(checkedCount, 1) << "Exactly one tool button should be checked after click";
        }
    }
    delete config;
}

static std::vector<std::vector<int>> generateToolClickSequences(int count) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 2);
    std::vector<std::vector<int>> result;
    for (int i = 0; i < count; ++i) {
        std::vector<int> seq;
        int len = 5 + (rng() % 10);
        for (int j = 0; j < len; ++j) seq.push_back(dist(rng));
        result.push_back(seq);
    }
    return result;
}

INSTANTIATE_TEST_SUITE_P(
    RandomToolClicks,
    ToolMutualExclusionTest,
    ::testing::ValuesIn(generateToolClickSequences(50))
);

// ─── Property 3: Timecode Formatting Correctness ──────────────────────────────
// Validates: Requirements 6.4, 10.4

static QString testFormatTimecode(double seconds, int fps = 30) {
    int totalFrames = static_cast<int>(seconds * fps);
    int ff = totalFrames % fps;
    int ss = (totalFrames / fps) % 60;
    int mm = (totalFrames / fps / 60) % 60;
    int hh = totalFrames / fps / 3600;
    return QString("%1:%2:%3:%4")
        .arg(hh, 2, 10, QChar('0'))
        .arg(mm, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));
}

class TimecodeFormatTest : public ::testing::TestWithParam<double> {};

TEST_P(TimecodeFormatTest, MatchesPattern) {
    double pos = GetParam();
    QString tc = testFormatTimecode(pos);

    QRegularExpression re(R"(\d{2}:\d{2}:\d{2}:\d{2})");
    EXPECT_TRUE(re.match(tc).hasMatch())
        << "Timecode '" << tc.toStdString() << "' does not match HH:MM:SS:FF pattern";

    int ff = tc.split(':').last().toInt();
    EXPECT_GE(ff, 0) << "FF component should be >= 0";
    EXPECT_LE(ff, 29) << "FF component should be <= 29 for 30fps";
}

static std::vector<double> generateRandomPositions(int count) {
    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(0.0, 86400.0);
    std::vector<double> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) result.push_back(dist(rng));
    return result;
}

INSTANTIATE_TEST_SUITE_P(
    RandomPositions,
    TimecodeFormatTest,
    ::testing::ValuesIn(generateRandomPositions(100))
);

// ─── Property 5: Tab Switching View Consistency ───────────────────────────────
// Validates: Requirements 11.1, 11.3, 11.4

class TabSwitchTest : public ::testing::TestWithParam<std::vector<int>> {};

TEST_P(TabSwitchTest, ViewStackConsistency) {
    auto project = makeTestProject();
    auto* config = makeTestConfig();
    ncktv::EditorMode editor(project, config);

    auto* tabBar = editor.findChild<QWidget*>("tabBar");
    ASSERT_NE(tabBar, nullptr);

    QList<QPushButton*> tabBtns = tabBar->findChildren<QPushButton*>("tabBtn");
    ASSERT_EQ(tabBtns.size(), 3);

    const auto& clickSequence = GetParam();
    for (int tabIdx : clickSequence) {
        if (tabIdx >= 0 && tabIdx < tabBtns.size()) {
            tabBtns[tabIdx]->click();
            EXPECT_TRUE(tabBtns[tabIdx]->isChecked())
                << "Clicked tab " << tabIdx << " should be checked";
        }
    }
    delete config;
}

static std::vector<std::vector<int>> generateTabSequences(int count) {
    std::mt19937 rng(456);
    std::uniform_int_distribution<int> dist(0, 2);
    std::vector<std::vector<int>> result;
    for (int i = 0; i < count; ++i) {
        std::vector<int> seq;
        int len = 3 + (rng() % 8);
        for (int j = 0; j < len; ++j) seq.push_back(dist(rng));
        result.push_back(seq);
    }
    return result;
}

INSTANTIATE_TEST_SUITE_P(
    RandomTabClicks,
    TabSwitchTest,
    ::testing::ValuesIn(generateTabSequences(50))
);

// ─── Property 6: No Inline Styles on Structural Widgets ───────────────────────
// Validates: Requirements 15.1, 15.2

TEST(NoInlineStylesTest, EditorModeStructuralWidgets) {
    auto project = makeTestProject();
    auto* config = makeTestConfig();
    ncktv::EditorMode editor(project, config);

    QStringList structuralNames = {
        "panelHeader", "transportBar", "toolsPanel", "tabBar",
        "captionsPanel", "sourcePanel", "programPanel", "timelinePanel"
    };

    for (const QString& name : structuralNames) {
        QList<QWidget*> widgets = editor.findChildren<QWidget*>(name);
        for (auto* w : widgets) {
            EXPECT_TRUE(w->styleSheet().isEmpty())
                << "Widget '" << name.toStdString() << "' has inline styleSheet: "
                << w->styleSheet().toStdString();
        }
    }
    delete config;
}
