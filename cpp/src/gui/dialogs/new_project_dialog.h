#pragma once
/*
 * NC-KTV GUI — New Project Dialog
 * File picker, project name, initial settings
 */

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>

namespace ncktv {

class NewProjectDialog : public QDialog {
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget* parent = nullptr);

    QString projectName() const;
    QString sourceFile() const;
    QString uvrModel() const;
    bool    useGpu() const;

private slots:
    void browseSourceFile();
    void onAccepted();

private:
    void setupUi();
    void applyTheme();

    QLineEdit*   m_nameEdit = nullptr;
    QLineEdit*   m_sourceEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QComboBox*   m_modelCombo = nullptr;
    QCheckBox*   m_gpuCheck = nullptr;
    QPushButton* m_createBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

} // namespace ncktv
