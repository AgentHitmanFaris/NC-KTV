#pragma once
/*
 * NC-KTV GUI — Video Options Dialog
 * Resolution, codec, bitrate, and FPS settings
 */

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

namespace ncktv {

class VideoOptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit VideoOptionsDialog(QWidget* parent = nullptr);

    int     width() const;
    int     height() const;
    int     fps() const;
    int     bitrateMbps() const;
    QString codec() const;
    bool    hwAccel() const;

private slots:
    void onResolutionChanged(int index);

private:
    void setupUi();
    void applyTheme();

    QComboBox* m_resolutionCombo = nullptr;
    QComboBox* m_codecCombo = nullptr;
    QSpinBox*  m_fpsSpinBox = nullptr;
    QSlider*   m_bitrateSlider = nullptr;
    QLabel*    m_bitrateLabel = nullptr;
    QSpinBox*  m_widthSpin = nullptr;
    QSpinBox*  m_heightSpin = nullptr;
    QCheckBox* m_hwAccelCheck = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

} // namespace ncktv
