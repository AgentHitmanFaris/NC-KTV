/*
 * NC-KTV GUI — Shortcuts Dialog Implementation
 */

#include "shortcuts_dialog.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>

namespace ncktv {

ShortcutsDialog::ShortcutsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Keyboard Shortcuts");
    setMinimumSize(480, 500);
    setModal(true);
    setupUi();
    applyTheme();
}

void ShortcutsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    auto* titleLabel = new QLabel("⌨  Keyboard Shortcuts", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    layout->addWidget(titleLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({"Shortcut", "Action"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    // Populate shortcuts
    QVector<QPair<QString, QString>> shortcuts = {
        {"Space",       "Set line start time (tap-to-sync)"},
        {"↑ / ↓",      "Navigate between subtitle lines"},
        {"J",           "Rewind 5 seconds"},
        {"K",           "Toggle play / pause"},
        {"L",           "Forward 5 seconds"},
        {"Ctrl+S",      "Save project"},
        {"Ctrl+Shift+S","Save project as..."},
        {"Ctrl+Z",      "Undo"},
        {"Ctrl+Y",      "Redo"},
        {"Ctrl+N",      "New project"},
        {"Ctrl+O",      "Open project"},
        {"Ctrl+E",      "Export"},
        {"Ctrl+I",      "Import subtitles"},
        {"Ctrl+B",      "Split clip at playhead"},
        {"Delete",      "Delete selected subtitle/clip"},
        {"F1",          "Show this shortcuts dialog"},
        {"Ctrl+Q",      "Quit application"},
    };

    m_table->setRowCount(shortcuts.size());
    for (int i = 0; i < shortcuts.size(); ++i) {
        auto* keyItem = new QTableWidgetItem(shortcuts[i].first);
        keyItem->setForeground(QColor("#4f46e5"));
        keyItem->setFont(QFont("Consolas", 11, QFont::Bold));
        m_table->setItem(i, 0, keyItem);
        m_table->setItem(i, 1, new QTableWidgetItem(shortcuts[i].second));
    }

    layout->addWidget(m_table, 1);

    auto* closeBtn = new QPushButton("Close", this);
    closeBtn->setFixedWidth(100);
    closeBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ShortcutsDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QTableWidget { background:#2d2d30; color:#d4d4d4; gridline-color:#3f3f46; border:1px solid #3f3f46; border-radius:4px; }
        QTableWidget::item { padding:6px; }
        QHeaderView::section { background:#252526; color:#ccc; border:1px solid #3f3f46; padding:6px; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QLabel { color:#ccc; border:none; }
    )");
}

} // namespace ncktv
