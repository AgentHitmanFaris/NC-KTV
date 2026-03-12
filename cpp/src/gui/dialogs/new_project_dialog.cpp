/*
 * NC-KTV GUI — New Project Dialog Implementation
 */

#include "new_project_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>

namespace ncktv {

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("New Project");
    setMinimumSize(520, 340);
    setModal(true);
    setupUi();
    applyTheme();
}

void NewProjectDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    // ── Title ────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel("✦  Create New Project", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Project Details Group ────────────────────────────────────────────
    auto* detailsGroup = new QGroupBox("Project Details", this);
    auto* formLayout = new QFormLayout(detailsGroup);
    formLayout->setSpacing(10);
    formLayout->setContentsMargins(16, 20, 16, 12);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("My Karaoke Project");
    formLayout->addRow("Project Name:", m_nameEdit);

    // Source file picker
    auto* sourceLayout = new QHBoxLayout();
    m_sourceEdit = new QLineEdit(this);
    m_sourceEdit->setPlaceholderText("Select audio/video file...");
    m_sourceEdit->setReadOnly(true);
    m_browseBtn = new QPushButton("Browse...", this);
    m_browseBtn->setFixedWidth(90);
    sourceLayout->addWidget(m_sourceEdit, 1);
    sourceLayout->addWidget(m_browseBtn);
    formLayout->addRow("Source File:", sourceLayout);

    mainLayout->addWidget(detailsGroup);

    // ── Processing Options Group ─────────────────────────────────────────
    auto* optionsGroup = new QGroupBox("Processing Options", this);
    auto* optionsForm = new QFormLayout(optionsGroup);
    optionsForm->setSpacing(10);
    optionsForm->setContentsMargins(16, 20, 16, 12);

    m_modelCombo = new QComboBox(this);
    m_modelCombo->addItems({
        "UVR_MDXNET_KARA_2.onnx",
        "6_HP-Karaoke-UVR.pth",
        "UVR-MDX-NET-Inst_HQ_3.onnx"
    });
    optionsForm->addRow("Separation Model:", m_modelCombo);

    m_gpuCheck = new QCheckBox("Use GPU acceleration (if available)", this);
    m_gpuCheck->setChecked(true);
    optionsForm->addRow("", m_gpuCheck);

    mainLayout->addWidget(optionsGroup);

    // ── Buttons ──────────────────────────────────────────────────────────
    mainLayout->addStretch();

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setFixedWidth(100);
    buttonLayout->addWidget(m_cancelBtn);

    m_createBtn = new QPushButton("Create Project", this);
    m_createBtn->setFixedWidth(140);
    m_createBtn->setDefault(true);
    m_createBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    buttonLayout->addWidget(m_createBtn);

    mainLayout->addLayout(buttonLayout);

    // ── Connections ──────────────────────────────────────────────────────
    connect(m_browseBtn, &QPushButton::clicked, this, &NewProjectDialog::browseSourceFile);
    connect(m_createBtn, &QPushButton::clicked, this, &NewProjectDialog::onAccepted);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void NewProjectDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QGroupBox { border:1px solid #3f3f46; border-radius:6px; padding-top:16px; margin-top:8px; font-weight:bold; color:#ccc; }
        QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; }
        QLineEdit { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px 8px; color:#fff; }
        QLineEdit:focus { border:1px solid #4f46e5; }
        QComboBox { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px 8px; color:#fff; }
        QComboBox::drop-down { border:none; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QCheckBox { color:#ccc; }
        QCheckBox::indicator { width:16px; height:16px; }
        QLabel { color:#ccc; border:none; }
    )");
}

void NewProjectDialog::browseSourceFile() {
    QString filter = "Media Files (*.mp3 *.wav *.flac *.ogg *.aac *.mp4 *.mkv *.avi *.webm);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, "Select Source File", "", filter);
    if (!path.isEmpty()) {
        m_sourceEdit->setText(path);
        // Auto-fill project name from filename
        if (m_nameEdit->text().isEmpty()) {
            QFileInfo fi(path);
            m_nameEdit->setText(fi.completeBaseName());
        }
    }
}

void NewProjectDialog::onAccepted() {
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing Name", "Please enter a project name.");
        m_nameEdit->setFocus();
        return;
    }
    if (m_sourceEdit->text().isEmpty()) {
        QMessageBox::warning(this, "No Source File", "Please select a source audio or video file.");
        return;
    }
    accept();
}

QString NewProjectDialog::projectName() const { return m_nameEdit->text().trimmed(); }
QString NewProjectDialog::sourceFile()   const { return m_sourceEdit->text(); }
QString NewProjectDialog::uvrModel()     const { return m_modelCombo->currentText(); }
bool    NewProjectDialog::useGpu()       const { return m_gpuCheck->isChecked(); }

} // namespace ncktv
