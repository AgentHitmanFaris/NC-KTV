#pragma once
#include <QWidget>
#include "../../core/config/config_manager.h"
#include "project/project.h"

QT_BEGIN_NAMESPACE
namespace Ui { class WizardMode; }
QT_END_NAMESPACE

namespace ncktv {

class WizardMode : public QWidget {
    Q_OBJECT

public:
    explicit WizardMode(ConfigManager* config, QWidget* parent = nullptr);
    ~WizardMode() override;
    void reset();

signals:
    void projectReady(Project* project);
    void requestOpen();
    void requestNew();
    void requestPreferences();

private slots:
    void onBrowseFile();
    void onStartClicked();
    void onSeparationFinished(const QString& instrumentalPath, const QString& vocalsPath);
    void onSeparationError(const QString& error);
    void onTranscriptionFinished(const QString& resultJson);
    void onTranscriptionError(const QString& error);

private:
    void setupUi();
    bool eventFilter(QObject* obj, QEvent* event) override;

    Ui::WizardMode* ui;
    ConfigManager* m_config = nullptr;

    QString m_selectedFile;
    QString m_onlineLyrics;
    Project* m_project = nullptr;
};

} // namespace ncktv
