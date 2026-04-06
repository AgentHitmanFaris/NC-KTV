/**
 * @file test_ui_clutter.cpp
 * @brief Bug condition exploration tests for UI/UX clutter fix
 *
 * These tests MUST FAIL on unfixed code — failure confirms the clutter bugs exist.
 * DO NOT attempt to fix the test or the code when it fails.
 *
 * Validates: Requirements 1.1, 1.2, 1.5, 1.6, 1.8
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QString>
#include <QList>
#include <QRegularExpression>

// GUI headers
#include "editor/editor_mode.h"
#include "wizard/wizard_mode.h"
#include "../core/project/ncktv_project.hpp"
#include "../core/config/config_manager.h"

// ─── Helpers ──────────────────────────────────────────────────────────────────

/**
 * Extract all border-radius values (in px) from a QSS string.
 * Returns a list of integer pixel values found.
 */
static QList<int> extractBorderRadiusPx(const QString& styleSheet) {
    QList<int> values;
    // Match patterns like "border-radius: 6px" or "border-radius:10px"
    QRegularExpression re(R"(border-radius\s*:\s*(\d+)px)", QRegularExpression::CaseInsensitiveOption);
    auto it = re.globalMatch(styleSheet);
    while (it.hasNext()) {
        auto match = it.next();
        values.append(match.captured(1).toInt());
    }
    return values;
}

// ─── Test Fixture ─────────────────────────────────────────────────────────────

class UIClutterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // ConfigManager can be null — EditorMode/WizardMode handle nullptr gracefully
        m_config = nullptr;

        // Create a minimal project for EditorMode
        m_project = std::make_shared<ncktv::core::Project>();
        m_project->project_name = "TestProject";

        // Instantiate the widgets under test
        m_editor = new ncktv::EditorMode(m_project, m_config);
        m_wizard = new ncktv::WizardMode(m_config);
    }

    void TearDown() override {
        delete m_editor;
        delete m_wizard;
    }

    ncktv::ConfigManager* m_config = nullptr;
    std::shared_ptr<ncktv::core::Project> m_project;
    ncktv::EditorMode* m_editor = nullptr;
    ncktv::WizardMode* m_wizard = nullptr;
};

// ─── Test 1: No embedded QMainWindow in EditorMode ────────────────────────────
/**
 * Validates: Requirement 1.2
 * Bug: EditorMode::setupUi() creates a QMainWindow* rightContainer as a child widget.
 * Expected (clean): No child of EditorMode should be a QMainWindow instance.
 *
 * EXPECTED TO FAIL on unfixed code — confirms embedded QMainWindow bug exists.
 */
TEST_F(UIClutterTest, EditorMode_NoEmbeddedQMainWindow) {
    auto embeddedWindows = m_editor->findChildren<QMainWindow*>();

    // Document what was found
    if (!embeddedWindows.isEmpty()) {
        qWarning("COUNTEREXAMPLE: EditorMode contains %d embedded QMainWindow child(ren).",
                 embeddedWindows.size());
        for (auto* w : embeddedWindows) {
            qWarning("  - QMainWindow at %p, objectName='%s'",
                     static_cast<void*>(w),
                     qPrintable(w->objectName()));
        }
    }

    EXPECT_TRUE(embeddedWindows.isEmpty())
        << "EditorMode contains " << embeddedWindows.size()
        << " embedded QMainWindow child(ren). "
        << "This causes dock widget chrome and layout instability. "
        << "Fix: replace with QSplitter or plain layout.";
}

// ─── Test 2: Exactly one header-height widget in EditorMode ──────────────────
/**
 * Validates: Requirement 1.1
 * Bug: EditorMode has both an appHeader (64px) inside the workspace AND the sidebar
 *      project card area that visually competes as a second header zone.
 * Expected (clean): Exactly one widget with a fixed height in the 48–72px header range.
 *
 * EXPECTED TO FAIL on unfixed code — confirms duplicate header bars bug exists.
 */
TEST_F(UIClutterTest, EditorMode_ExactlyOneHeaderHeightWidget) {
    // Collect all QWidget children with a fixed height in the typical header range (48–72px)
    QList<QWidget*> headerHeightWidgets;
    const int kHeaderMin = 48;
    const int kHeaderMax = 72;

    for (auto* w : m_editor->findChildren<QWidget*>()) {
        int fh = w->minimumHeight();
        // Check both minimumHeight == maximumHeight (fixed) and fixedHeight pattern
        if (w->minimumHeight() == w->maximumHeight() &&
            w->minimumHeight() >= kHeaderMin &&
            w->minimumHeight() <= kHeaderMax) {
            headerHeightWidgets.append(w);
        }
    }

    // Document what was found
    if (headerHeightWidgets.size() != 1) {
        qWarning("COUNTEREXAMPLE: EditorMode has %d header-height widgets (expected exactly 1).",
                 headerHeightWidgets.size());
        for (auto* w : headerHeightWidgets) {
            qWarning("  - %s height=%d objectName='%s'",
                     w->metaObject()->className(),
                     w->minimumHeight(),
                     qPrintable(w->objectName()));
        }
    }

    EXPECT_EQ(headerHeightWidgets.size(), 1)
        << "EditorMode has " << headerHeightWidgets.size()
        << " header-height widgets in range [" << kHeaderMin << "," << kHeaderMax << "px]. "
        << "Expected exactly 1. Duplicate header bars create visual noise. "
        << "Fix: consolidate to a single unified top bar.";
}

// ─── Test 3: No QPushButton with border-radius inconsistent with global QSS ──
/**
 * Validates: Requirement 1.5
 * Bug: Inline setStyleSheet() calls use border-radius values of 4px, 6px, 10px, 20px
 *      while the global dark_theme.qss defines 8px for QPushButton.
 * Expected (clean): No QPushButton should have an inline styleSheet() containing
 *                   border-radius values other than 8px (the global QSS standard).
 *
 * EXPECTED TO FAIL on unfixed code — confirms inline style inconsistency bug exists.
 */
TEST_F(UIClutterTest, NoPushButton_InconsistentBorderRadius) {
    // The global QSS standard for QPushButton border-radius is 8px
    const int kGlobalButtonBorderRadius = 8;

    QStringList violations;

    auto checkWidget = [&](QWidget* root, const QString& widgetName) {
        for (auto* btn : root->findChildren<QPushButton*>()) {
            QString ss = btn->styleSheet();
            if (ss.isEmpty()) continue;

            QList<int> radii = extractBorderRadiusPx(ss);
            for (int r : radii) {
                if (r != kGlobalButtonBorderRadius) {
                    violations.append(
                        QString("%1 QPushButton '%2' has border-radius:%3px (expected %4px). "
                                "StyleSheet snippet: %5")
                        .arg(widgetName)
                        .arg(btn->text().left(30))
                        .arg(r)
                        .arg(kGlobalButtonBorderRadius)
                        .arg(ss.left(120))
                    );
                }
            }
        }
    };

    checkWidget(m_editor, "EditorMode");
    checkWidget(m_wizard, "WizardMode");

    // Document counterexamples
    if (!violations.isEmpty()) {
        qWarning("COUNTEREXAMPLE: Found %d QPushButton(s) with inconsistent border-radius:",
                 violations.size());
        for (const auto& v : violations) {
            qWarning("  - %s", qPrintable(v));
        }
    }

    EXPECT_TRUE(violations.isEmpty())
        << "Found " << violations.size()
        << " QPushButton(s) with border-radius values inconsistent with global QSS (8px):\n"
        << violations.join("\n").toStdString();
}

// ─── Test 4: Sidebar audio monitor has content or does not exist ──────────────
/**
 * Validates: Requirement 1.8
 * Bug: The sidebar audio monitor is a 70px dark QWidget with zero child content —
 *      a visual dead zone that looks unfinished.
 * Expected (clean): The audio monitor widget either has visible child widgets
 *                   (e.g., a QLabel, WaveformWidget) or does not exist at all.
 *
 * EXPECTED TO FAIL on unfixed code — confirms empty audio monitor placeholder bug.
 */
TEST_F(UIClutterTest, EditorMode_AudioMonitor_HasContentOrAbsent) {
    // The audio monitor is a QWidget with fixedHeight(70) in the sidebar.
    // We identify it by its fixed height of 70px.
    QWidget* audioMonitor = nullptr;
    for (auto* w : m_editor->findChildren<QWidget*>()) {
        if (w->minimumHeight() == w->maximumHeight() && w->minimumHeight() == 70) {
            audioMonitor = w;
            break;
        }
    }

    if (audioMonitor == nullptr) {
        // Widget doesn't exist — this is the acceptable "removed" state
        SUCCEED() << "Audio monitor placeholder does not exist (acceptable clean state).";
        return;
    }

    // Widget exists — it must have at least one visible child widget
    auto children = audioMonitor->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);

    if (children.isEmpty()) {
        qWarning("COUNTEREXAMPLE: Audio monitor widget exists (height=70px) but has NO child widgets. "
                 "This is an empty visual dead zone.");
    }

    EXPECT_FALSE(children.isEmpty())
        << "The sidebar audio monitor widget (height=70px) exists but has no child content. "
        << "This creates an empty visual dead zone. "
        << "Fix: either remove it or populate it with a waveform/time display.";
}

// ─── Test 5: WizardMode hero section has no stat card widgets ─────────────────
/**
 * Validates: Requirement 1.6
 * Bug: WizardMode hero section shows three stat cards with labels "LATENCY",
 *      "RESOLUTION", "FFMPEG" — non-actionable visual noise.
 * Expected (clean): No QLabel children of WizardMode should contain these stat labels.
 *
 * EXPECTED TO FAIL on unfixed code — confirms non-actionable stat cards bug exists.
 */
TEST_F(UIClutterTest, WizardMode_HeroSection_NoStatCards) {
    const QStringList kStatCardLabels = {"LATENCY", "RESOLUTION", "FFMPEG"};

    QStringList foundStatLabels;
    for (auto* lbl : m_wizard->findChildren<QLabel*>()) {
        QString text = lbl->text().toUpper();
        for (const auto& statLabel : kStatCardLabels) {
            if (text.contains(statLabel)) {
                foundStatLabels.append(
                    QString("QLabel text='%1'").arg(lbl->text())
                );
                break;
            }
        }
    }

    // Document counterexamples
    if (!foundStatLabels.isEmpty()) {
        qWarning("COUNTEREXAMPLE: WizardMode hero section contains %d stat card label(s):",
                 foundStatLabels.size());
        for (const auto& s : foundStatLabels) {
            qWarning("  - %s", qPrintable(s));
        }
    }

    EXPECT_TRUE(foundStatLabels.isEmpty())
        << "WizardMode hero section contains " << foundStatLabels.size()
        << " non-actionable stat card label(s): "
        << foundStatLabels.join(", ").toStdString()
        << ". These provide no user value and add visual noise. "
        << "Fix: remove the statsWidget and its createStatCard() calls.";
}
