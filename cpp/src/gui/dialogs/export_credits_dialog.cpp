/*
 * NC-KTV GUI — Export Credits Dialog Implementation
 */

#include "export_credits_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>

namespace ncktv {

ExportCreditsDialog::ExportCreditsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Intro / Credits");
    setMinimumSize(480, 400);
    setModal(true);
    setupUi();
    applyTheme();
}

void ExportCreditsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    auto* titleLabel = new QLabel("🎬  Intro / Credits", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Song Info Group ──────────────────────────────────────────────────
    auto* infoGroup = new QGroupBox("Song Information", this);
    auto* infoForm = new QFormLayout(infoGroup);
    infoForm->setSpacing(10);
    infoForm->setContentsMargins(16, 20, 16, 12);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText("Song title...");
    infoForm->addRow("Title:", m_titleEdit);

    m_artistEdit = new QLineEdit(this);
    m_artistEdit->setPlaceholderText("Artist name...");
    infoForm->addRow("Artist:", m_artistEdit);

    mainLayout->addWidget(infoGroup);

    // ── Credits Group ────────────────────────────────────────────────────
    auto* creditsGroup = new QGroupBox("Custom Credits", this);
    auto* creditsLayout = new QVBoxLayout(creditsGroup);
    creditsLayout->setContentsMargins(16, 20, 16, 12);

    m_creditsEdit = new QTextEdit(this);
    m_creditsEdit->setPlaceholderText("Additional credits text (shown during intro)...");
    m_creditsEdit->setMaximumHeight(100);
    creditsLayout->addWidget(m_creditsEdit);

    mainLayout->addWidget(creditsGroup);

    // ── Timing Group ─────────────────────────────────────────────────────
    auto* timingGroup = new QGroupBox("Timing", this);
    auto* timingForm = new QFormLayout(timingGroup);
    timingForm->setSpacing(10);
    timingForm->setContentsMargins(16, 20, 16, 12);

    m_countdownCheck = new QCheckBox("Show \"3, 2, 1, GO\" countdown", this);
    m_countdownCheck->setChecked(true);
    timingForm->addRow("", m_countdownCheck);

    m_durationSpin = new QSpinBox(this);
    m_durationSpin->setRange(2, 30);
    m_durationSpin->setValue(5);
    m_durationSpin->setSuffix(" seconds");
    timingForm->addRow("Intro Duration:", m_durationSpin);

    mainLayout->addWidget(timingGroup);

    // ── Buttons ──────────────────────────────────────────────────────────
    mainLayout->addStretch();
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setFixedWidth(100);
    btnLayout->addWidget(m_cancelBtn);
    m_okBtn = new QPushButton("OK", this);
    m_okBtn->setFixedWidth(100);
    m_okBtn->setDefault(true);
    m_okBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    btnLayout->addWidget(m_okBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ExportCreditsDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QGroupBox { border:1px solid #3f3f46; border-radius:6px; padding-top:16px; margin-top:8px; font-weight:bold; color:#ccc; }
        QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; }
        QLineEdit, QSpinBox { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px 8px; color:#fff; }
        QTextEdit { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px; color:#d4d4d4; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QCheckBox { color:#ccc; }
        QLabel { color:#ccc; border:none; }
    )");
}

QString ExportCreditsDialog::titleText()         const { return m_titleEdit->text(); }
QString ExportCreditsDialog::artistText()        const { return m_artistEdit->text(); }
QString ExportCreditsDialog::customCreditsText() const { return m_creditsEdit->toPlainText(); }
bool    ExportCreditsDialog::showCountdown()     const { return m_countdownCheck->isChecked(); }
double  ExportCreditsDialog::introDuration()     const { return m_durationSpin->value(); }

} // namespace ncktv
