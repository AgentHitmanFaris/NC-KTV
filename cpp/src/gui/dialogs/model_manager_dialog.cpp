/*
 * NC-KTV GUI — Model Manager Dialog Implementation
 */

#include "model_manager_dialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

namespace ncktv {

ModelManagerDialog::ModelManagerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Model Manager");
    setMinimumSize(500, 420);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(24, 20, 24, 20);

    auto* titleLabel = new QLabel("🤖  Whisper Model Manager", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    layout->addWidget(titleLabel);

    auto* descLabel = new QLabel("Manage AI transcription models. Larger models are more accurate but use more resources.", this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color:#888; font-size:12px; border:none;");
    layout->addWidget(descLabel);

    m_widget = new ModelManagerWidget(this);
    layout->addWidget(m_widget, 1);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* closeBtn = new QPushButton("Close", this);
    closeBtn->setFixedWidth(100);
    closeBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QLabel { color:#ccc; border:none; }
    )");
}

} // namespace ncktv
