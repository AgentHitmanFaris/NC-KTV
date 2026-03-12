#pragma once
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include "themes/theme_manager.h"
namespace ncktv {
class ThemeManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemeManagerDialog(ThemeManager* manager, QWidget* parent = nullptr);
private slots:
    void onApply();
    void onInstall();
    void onUninstall();
    void onSelectionChanged();
    void refreshList();
private:
    void setupUi();
    void applyTheme();
    ThemeManager* m_manager = nullptr;
    QListWidget*  m_themeList = nullptr;
    QLabel*       m_previewLabel = nullptr;
    QPushButton*  m_applyBtn = nullptr;
    QPushButton*  m_installBtn = nullptr;
    QPushButton*  m_uninstallBtn = nullptr;
};
} // namespace ncktv
