/*
 * NC-KTV GUI — Plugin Manager Dialog Implementation
 */

#include "plugin_manager_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

namespace ncktv {

PluginManagerDialog::PluginManagerDialog(PluginManager* manager, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle("Plugin Manager");
    setMinimumSize(640, 460);
    setModal(true);
    setupUi();
    applyTheme();
    refreshList();
}

void PluginManagerDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    auto* titleLabel = new QLabel("🔌  Plugin Manager", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Split: List | Detail ─────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: Plugin list
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_pluginList = new QListWidget(leftWidget);
    leftLayout->addWidget(m_pluginList, 1);

    auto* listBtnLayout = new QHBoxLayout();
    m_installBtn = new QPushButton("⬇ Install...", leftWidget);
    m_uninstallBtn = new QPushButton("✕ Remove", leftWidget);
    listBtnLayout->addWidget(m_installBtn);
    listBtnLayout->addWidget(m_uninstallBtn);
    leftLayout->addLayout(listBtnLayout);

    splitter->addWidget(leftWidget);

    // Right: Detail panel
    auto* detailWidget = new QWidget(splitter);
    auto* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(12, 0, 0, 0);

    m_detailName = new QLabel("Select a plugin", detailWidget);
    m_detailName->setStyleSheet("font-size:16px; font-weight:bold; color:#fff; border:none;");
    detailLayout->addWidget(m_detailName);

    m_detailAuthor = new QLabel("", detailWidget);
    m_detailAuthor->setStyleSheet("color:#888; border:none;");
    detailLayout->addWidget(m_detailAuthor);

    m_detailVersion = new QLabel("", detailWidget);
    m_detailVersion->setStyleSheet("color:#888; border:none;");
    detailLayout->addWidget(m_detailVersion);

    m_detailDesc = new QTextEdit(detailWidget);
    m_detailDesc->setReadOnly(true);
    m_detailDesc->setMaximumHeight(120);
    detailLayout->addWidget(m_detailDesc);

    m_enableBtn = new QPushButton("Enable", detailWidget);
    m_enableBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    detailLayout->addWidget(m_enableBtn);

    detailLayout->addStretch();

    splitter->addWidget(detailWidget);
    splitter->setSizes({280, 360});

    mainLayout->addWidget(splitter, 1);

    // ── Close ────────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* closeBtn = new QPushButton("Close", this);
    closeBtn->setFixedWidth(100);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    // ── Connections ──────────────────────────────────────────────────────
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_installBtn, &QPushButton::clicked, this, &PluginManagerDialog::onInstall);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &PluginManagerDialog::onUninstall);
    connect(m_enableBtn, &QPushButton::clicked, this, &PluginManagerDialog::onToggleEnabled);
    connect(m_pluginList, &QListWidget::currentRowChanged, this, [this](int) { onSelectionChanged(); });
}

void PluginManagerDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QListWidget { background:#2d2d30; color:#d4d4d4; border:1px solid #3f3f46; border-radius:4px; }
        QListWidget::item { padding:8px; border-bottom:1px solid #3f3f46; }
        QListWidget::item:selected { background:#4f46e5; color:white; }
        QListWidget::item:hover { background:#333; }
        QTextEdit { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:8px; color:#ccc; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QLabel { color:#ccc; border:none; }
        QSplitter::handle { background:#111; }
    )");
}

void PluginManagerDialog::refreshList() {
    m_pluginList->clear();
    if (!m_manager) return;

    auto plugins = m_manager->getDiscoveredPlugins();
    for (const auto& p : plugins) {
        QString label = p.name;
        auto* loaded = m_manager->getPlugin(p.id);
        if (loaded && loaded->isEnabled()) label += "  ✓";
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, p.id);
        m_pluginList->addItem(item);
    }

    if (plugins.isEmpty()) {
        m_pluginList->addItem("No plugins discovered.");
    }
}

void PluginManagerDialog::onSelectionChanged() {
    if (!m_manager) return;
    auto* item = m_pluginList->currentItem();
    if (!item) return;

    QString pluginId = item->data(Qt::UserRole).toString();
    auto plugins = m_manager->getDiscoveredPlugins();

    for (const auto& p : plugins) {
        if (p.id == pluginId) {
            m_detailName->setText(p.name);
            m_detailAuthor->setText("Author: " + p.author);
            m_detailVersion->setText("Version: " + p.version);
            m_detailDesc->setPlainText(p.description);

            auto* loaded = m_manager->getPlugin(p.id);
            bool enabled = loaded && loaded->isEnabled();
            m_enableBtn->setText(enabled ? "Disable" : "Enable");
            return;
        }
    }
}

void PluginManagerDialog::onInstall() {
    QString path = QFileDialog::getOpenFileName(this, "Install Plugin Package",
        "", "NC-KTV Plugin (*.nckplugin);;All Files (*)");
    if (path.isEmpty()) return;

    if (m_manager && m_manager->installPluginPackage(path)) {
        refreshList();
        QMessageBox::information(this, "Installed", "Plugin installed successfully.");
    } else {
        QMessageBox::warning(this, "Install Failed", "Could not install the plugin package.");
    }
}

void PluginManagerDialog::onToggleEnabled() {
    if (!m_manager) return;
    auto* item = m_pluginList->currentItem();
    if (!item) return;
    QString pluginId = item->data(Qt::UserRole).toString();

    auto* loaded = m_manager->getPlugin(pluginId);
    if (loaded && loaded->isEnabled()) {
        m_manager->disablePlugin(pluginId);
    } else {
        m_manager->enablePlugin(pluginId);
    }
    refreshList();
    onSelectionChanged();
}

void PluginManagerDialog::onUninstall() {
    auto* item = m_pluginList->currentItem();
    if (!item) return;
    QString pluginId = item->data(Qt::UserRole).toString();

    if (QMessageBox::question(this, "Remove Plugin",
            "Remove plugin \"" + pluginId + "\"?") != QMessageBox::Yes) return;

    if (m_manager) m_manager->uninstallPlugin(pluginId);
    refreshList();
}

} // namespace ncktv
