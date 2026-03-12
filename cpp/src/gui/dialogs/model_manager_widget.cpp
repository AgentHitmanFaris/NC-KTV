/*
 * NC-KTV GUI — Model Manager Widget Implementation
 */

#include "model_manager_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QMessageBox>

namespace ncktv {

ModelManagerWidget::ModelManagerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    applyTheme();
    refreshModelList();
}

void ModelManagerWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(0, 0, 0, 0);

    m_modelList = new QListWidget(this);
    layout->addWidget(m_modelList, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedHeight(16);
    m_progressBar->setTextVisible(false);
    layout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("", this);
    m_statusLabel->setStyleSheet("color:#888; font-size:11px; border:none;");
    layout->addWidget(m_statusLabel);

    auto* btnLayout = new QHBoxLayout();
    m_installBtn = new QPushButton("⬇ Download Model...", this);
    m_uninstallBtn = new QPushButton("✕ Remove", this);
    btnLayout->addWidget(m_installBtn);
    btnLayout->addWidget(m_uninstallBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(m_installBtn, &QPushButton::clicked, this, &ModelManagerWidget::onInstall);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &ModelManagerWidget::onUninstall);
}

void ModelManagerWidget::applyTheme() {
    setStyleSheet(R"(
        QListWidget { background:#2d2d30; color:#d4d4d4; border:1px solid #3f3f46; border-radius:4px; }
        QListWidget::item { padding:8px; border-bottom:1px solid #3f3f46; }
        QListWidget::item:selected { background:#4f46e5; color:white; }
        QListWidget::item:hover { background:#333; }
        QProgressBar { border:1px solid #555; border-radius:4px; background:#222; }
        QProgressBar::chunk { background-color:#4f46e5; border-radius:2px; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
    )");
}

void ModelManagerWidget::refreshModelList() {
    m_modelList->clear();

    // Scan for available models
    struct ModelInfo {
        QString name;
        QString size;
        bool installed;
    };

    QVector<ModelInfo> models = {
        {"tiny",     "~39 MB",   false},
        {"base",     "~74 MB",   false},
        {"small",    "~244 MB",  false},
        {"medium",   "~769 MB",  false},
        {"large-v2", "~1.5 GB",  false},
        {"large-v3", "~1.5 GB",  false},
    };

    // Check which models exist locally
    QDir modelDir("models/whisper");
    for (auto& m : models) {
        if (modelDir.exists("faster-whisper-" + m.name) || 
            modelDir.exists(m.name + ".pt")) {
            m.installed = true;
        }
    }

    for (const auto& m : models) {
        QString label = m.name + "  (" + m.size + ")";
        if (m.installed) label += "  ✓ Installed";
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, m.name);
        if (m.installed) {
            item->setForeground(QColor("#4ade80"));
        }
        m_modelList->addItem(item);
    }

    int installed = std::count_if(models.begin(), models.end(), [](const ModelInfo& m) { return m.installed; });
    m_statusLabel->setText(QString("%1 of %2 models installed").arg(installed).arg(models.size()));
}

void ModelManagerWidget::onInstall() {
    auto* item = m_modelList->currentItem();
    if (!item) {
        QMessageBox::information(this, "Select Model", "Please select a model to download.");
        return;
    }
    QString modelName = item->data(Qt::UserRole).toString();
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Downloading " + modelName + "...");

    // In a real implementation, kick off a QProcess / QNetworkReply to download
    // For now, show a placeholder message
    QMessageBox::information(this, "Download", 
        "Model download for '" + modelName + "' would start here.\n"
        "This requires an active internet connection to fetch the native C++ ggml binaries.");
    m_progressBar->setVisible(false);
    m_statusLabel->setText("Ready");
}

void ModelManagerWidget::onUninstall() {
    auto* item = m_modelList->currentItem();
    if (!item) return;
    QString modelName = item->data(Qt::UserRole).toString();

    if (QMessageBox::question(this, "Remove Model",
            "Remove the \"" + modelName + "\" model from disk?") != QMessageBox::Yes) {
        return;
    }

    // In a real implementation, delete the model directory
    QDir modelDir("models/whisper/faster-whisper-" + modelName);
    if (modelDir.exists()) {
        modelDir.removeRecursively();
    }

    refreshModelList();
    emit modelUninstalled(modelName);
}

} // namespace ncktv
