#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
namespace ncktv {
class ModelManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelManagerWidget(QWidget* parent = nullptr);
signals:
    void modelInstalled(const QString& modelName);
    void modelUninstalled(const QString& modelName);
private slots:
    void onInstall();
    void onUninstall();
    void refreshModelList();
private:
    void setupUi();
    void applyTheme();
    QListWidget*  m_modelList = nullptr;
    QPushButton*  m_installBtn = nullptr;
    QPushButton*  m_uninstallBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel*       m_statusLabel = nullptr;
};
} // namespace ncktv
