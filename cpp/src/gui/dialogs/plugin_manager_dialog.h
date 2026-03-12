#pragma once
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include "plugins/plugin_manager.h"
namespace ncktv {
class PluginManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PluginManagerDialog(PluginManager* manager, QWidget* parent = nullptr);
private slots:
    void onInstall();
    void onToggleEnabled();
    void onUninstall();
    void onSelectionChanged();
    void refreshList();
private:
    void setupUi();
    void applyTheme();
    PluginManager* m_manager = nullptr;
    QListWidget*   m_pluginList = nullptr;
    QLabel*        m_detailName = nullptr;
    QLabel*        m_detailAuthor = nullptr;
    QLabel*        m_detailVersion = nullptr;
    QTextEdit*     m_detailDesc = nullptr;
    QPushButton*   m_enableBtn = nullptr;
    QPushButton*   m_installBtn = nullptr;
    QPushButton*   m_uninstallBtn = nullptr;
};
} // namespace ncktv
