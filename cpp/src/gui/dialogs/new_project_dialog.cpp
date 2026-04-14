/*
 * NC-KTV GUI — New Project Dialog Implementation
 */

#include "new_project_dialog.h"
#include "ui_new_project_dialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

namespace ncktv {

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::NewProjectDialog)
{
    ui->setupUi(this);
    setModal(true);

    connect(ui->m_browseBtn, &QPushButton::clicked, this, &NewProjectDialog::browseSourceFile);
    connect(ui->m_createBtn, &QPushButton::clicked, this, &NewProjectDialog::onAccepted);
    connect(ui->m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

NewProjectDialog::~NewProjectDialog() {
    delete ui;
}

void NewProjectDialog::browseSourceFile() {
    QString filter = "Media Files (*.mp3 *.wav *.flac *.ogg *.aac *.mp4 *.mkv *.avi *.webm);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, "Select Source File", "", filter);
    if (!path.isEmpty()) {
        ui->m_sourceEdit->setText(path);
        // Auto-fill project name from filename
        if (ui->m_nameEdit->text().isEmpty()) {
            QFileInfo fi(path);
            ui->m_nameEdit->setText(fi.completeBaseName());
        }
    }
}

void NewProjectDialog::onAccepted() {
    if (ui->m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing Name", "Please enter a project name.");
        ui->m_nameEdit->setFocus();
        return;
    }
    if (ui->m_sourceEdit->text().isEmpty()) {
        QMessageBox::warning(this, "No Source File", "Please select a source audio or video file.");
        return;
    }
    accept();
}

QString NewProjectDialog::projectName() const { return ui->m_nameEdit->text().trimmed(); }
QString NewProjectDialog::sourceFile()   const { return ui->m_sourceEdit->text(); }
QString NewProjectDialog::uvrModel()     const { return ui->m_modelCombo->currentText(); }
bool    NewProjectDialog::useGpu()       const { return ui->m_gpuCheck->isChecked(); }

} // namespace ncktv
