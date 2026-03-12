/*
 * NC-KTV GUI — Theme Manager Dialog Implementation
 */

#include "theme_manager_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>

namespace ncktv {

ThemeManagerDialog::ThemeManagerDialog(ThemeManager* manager, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle("Theme Manager");
    setMinimumSize(560, 420);
    setModal(true);
    setupUi();
    applyTheme();
    refreshList();
}

void ThemeManagerDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    auto* titleLabel = new QLabel("🎨  Theme Manager", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Split: Theme List | Preview ──────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: Theme list
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_themeList = new QListWidget(leftWidget);
    leftLayout->addWidget(m_themeList, 1);

    auto* listBtnLayout = new QHBoxLayout();
    m_installBtn = new QPushButton("⬇ Install...", leftWidget);
    m_uninstallBtn = new QPushButton("✕ Remove", leftWidget);
    listBtnLayout->addWidget(m_installBtn);
    listBtnLayout->addWidget(m_uninstallBtn);
    leftLayout->addLayout(listBtnLayout);

    splitter->addWidget(leftWidget);

    // Right: Preview
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(12, 0, 0, 0);

    m_previewLabel = new QLabel("Select a theme to preview", rightWidget);
    m_previewLabel->setStyleSheet("font-size:14px; color:#888; border:1px solid #3f3f46; border-radius:8px; padding:24px; background:#2d2d30; min-height:200px;");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setWordWrap(true);
    rightLayout->addWidget(m_previewLabel, 1);

    m_applyBtn = new QPushButton("Apply Theme", rightWidget);
    m_applyBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:10px 20px; border-radius:4px;");
    rightLayout->addWidget(m_applyBtn);

    splitter->addWidget(rightWidget);
    splitter->setSizes({240, 320});

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
    connect(m_applyBtn, &QPushButton::clicked, this, &ThemeManagerDialog::onApply);
    connect(m_installBtn, &QPushButton::clicked, this, &ThemeManagerDialog::onInstall);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &ThemeManagerDialog::onUninstall);
    connect(m_themeList, &QListWidget::currentRowChanged, this, [this](int) { onSelectionChanged(); });
}

void ThemeManagerDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QListWidget { background:#2d2d30; color:#d4d4d4; border:1px solid #3f3f46; border-radius:4px; }
        QListWidget::item { padding:10px; border-bottom:1px solid #3f3f46; }
        QListWidget::item:selected { background:#4f46e5; color:white; }
        QListWidget::item:hover { background:#333; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QLabel { color:#ccc; border:none; }
        QSplitter::handle { background:#111; }
    )");
}

void ThemeManagerDialog::refreshList() {
    m_themeList->clear();
    if (!m_manager) return;

    QStringList themes = m_manager->availableThemes();
    QString current = m_manager->currentTheme();

    for (const auto& t : themes) {
        QString label = t;
        if (t == current) label += "  ✓ Active";
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, t);
        if (t == current) item->setForeground(QColor("#4ade80"));
        m_themeList->addItem(item);
    }

    if (themes.isEmpty()) {
        m_themeList->addItem("builtin-dark  ✓ Active");
        m_themeList->addItem("builtin-light");
    }
}

void ThemeManagerDialog::onSelectionChanged() {
    auto* item = m_themeList->currentItem();
    if (!item) return;
    QString themeId = item->data(Qt::UserRole).toString();

    if (themeId.isEmpty()) themeId = item->text().split(" ").first();

    m_previewLabel->setText(
        QString("<b>%1</b><br><br>Click \"Apply Theme\" to activate this theme.<br><br>"
                "<i style='color:#888;'>Theme will be applied to the entire application.</i>")
            .arg(themeId));
}

void ThemeManagerDialog::onApply() {
    auto* item = m_themeList->currentItem();
    if (!item) return;
    QString themeId = item->data(Qt::UserRole).toString();
    if (themeId.isEmpty()) themeId = item->text().split(" ").first();

    if (m_manager) {
        m_manager->applyTheme(themeId);
        refreshList();
        QMessageBox::information(this, "Theme Applied",
            "Theme \"" + themeId + "\" has been applied.");
    }
}

void ThemeManagerDialog::onInstall() {
    QString path = QFileDialog::getOpenFileName(this, "Install Theme Package",
        "", "NC-KTV Theme (*.ncktheme);;All Files (*)");
    if (path.isEmpty()) return;

    if (m_manager && m_manager->installThemePackage(path)) {
        refreshList();
        QMessageBox::information(this, "Installed", "Theme installed successfully.");
    } else {
        QMessageBox::warning(this, "Install Failed", "Could not install the theme package.");
    }
}

void ThemeManagerDialog::onUninstall() {
    auto* item = m_themeList->currentItem();
    if (!item) return;
    QString themeId = item->data(Qt::UserRole).toString();
    if (themeId.isEmpty()) themeId = item->text().split(" ").first();

    if (themeId.startsWith("builtin")) {
        QMessageBox::warning(this, "Cannot Remove", "Built-in themes cannot be removed.");
        return;
    }

    if (QMessageBox::question(this, "Remove Theme",
            "Remove theme \"" + themeId + "\"?") != QMessageBox::Yes) return;

    if (m_manager) m_manager->uninstallTheme(themeId);
    refreshList();
}

} // namespace ncktv
