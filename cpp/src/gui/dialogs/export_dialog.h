#pragma once
/*
 * NC-KTV GUI — Export Dialog
 * Export format, quality, output path, and preset selection
 */

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>

namespace ncktv {

class ExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(QWidget* parent = nullptr);

    // Result accessors
    QString outputPath() const;
    QString format() const;
    QString preset() const;
    QString lyricsStyle() const;
    QString audioSource() const;
    int     videoWidth() const;
    int     videoHeight() const;
    bool    burnSubtitles() const;

private slots:
    void browseOutput();
    void onPresetChanged(int index);
    void onAccepted();

private:
    void setupUi();
    void applyTheme();

    QComboBox*   m_presetCombo = nullptr;
    QComboBox*   m_formatCombo = nullptr;
    QComboBox*   m_audioSourceCombo = nullptr;
    QComboBox*   m_lyricsStyleCombo = nullptr;
    QComboBox*   m_resolutionCombo = nullptr;
    QLineEdit*   m_outputEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QCheckBox*   m_burnSubsCheck = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

} // namespace ncktv
