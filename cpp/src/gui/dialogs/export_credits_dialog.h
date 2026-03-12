#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
namespace ncktv {
class ExportCreditsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExportCreditsDialog(QWidget* parent = nullptr);
    QString titleText() const;
    QString artistText() const;
    QString customCreditsText() const;
    bool    showCountdown() const;
    double  introDuration() const;
private:
    void setupUi();
    void applyTheme();
    QLineEdit* m_titleEdit = nullptr;
    QLineEdit* m_artistEdit = nullptr;
    QTextEdit* m_creditsEdit = nullptr;
    QCheckBox* m_countdownCheck = nullptr;
    QSpinBox*  m_durationSpin = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};
} // namespace ncktv
