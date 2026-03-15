/*
 * NC-KTV GUI — Import Dialog Implementation
 */

#include "import_dialog.h"
#include "lyrics_search_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QGroupBox>

namespace ncktv {

ImportDialog::ImportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Import Subtitles");
    setMinimumSize(560, 440);
    setModal(true);
    setupUi();
    applyTheme();
}

void ImportDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    // ── Title ────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel("📁  Import Subtitles / Lyrics", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    mainLayout->addWidget(titleLabel);

    auto* descLabel = new QLabel("Select a subtitle or lyrics file to import. Supports LRC, SRT, TXT, VTT, ASS, and Whisper JSON.", this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color:#888; font-size:12px; border:none;");
    mainLayout->addWidget(descLabel);

    // ── File selector ────────────────────────────────────────────────────
    auto* fileGroup = new QGroupBox("Source File", this);
    auto* fileLayout = new QVBoxLayout(fileGroup);

    auto* browseLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText("Select a subtitle file...");
    m_pathEdit->setReadOnly(true);
    m_browseBtn = new QPushButton("Browse...", this);
    m_browseBtn->setFixedWidth(90);
    m_searchBtn = new QPushButton("🔍 Search Online", this);
    m_searchBtn->setFixedWidth(120);
    m_searchBtn->setStyleSheet("background: #1e293b; color: #94a3b8; font-weight: bold; border-color: #334155;");
    
    browseLayout->addWidget(m_pathEdit, 1);
    browseLayout->addWidget(m_browseBtn);
    browseLayout->addWidget(m_searchBtn);
    fileLayout->addLayout(browseLayout);

    auto* infoLayout = new QHBoxLayout();
    m_formatLabel = new QLabel("Format: —", this);
    m_formatLabel->setStyleSheet("color:#4f46e5; font-weight:bold; border:none;");
    m_lineCountLabel = new QLabel("", this);
    m_lineCountLabel->setStyleSheet("color:#888; border:none;");
    infoLayout->addWidget(m_formatLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_lineCountLabel);
    fileLayout->addLayout(infoLayout);

    mainLayout->addWidget(fileGroup);

    // ── Preview ──────────────────────────────────────────────────────────
    auto* previewGroup = new QGroupBox("Preview", this);
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewText = new QTextEdit(this);
    m_previewText->setReadOnly(true);
    m_previewText->setPlaceholderText("File preview will appear here...");
    m_previewText->setMaximumHeight(160);
    previewLayout->addWidget(m_previewText);
    mainLayout->addWidget(previewGroup, 1);

    // ── Buttons ──────────────────────────────────────────────────────────
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setFixedWidth(100);
    buttonLayout->addWidget(m_cancelBtn);

    m_importBtn = new QPushButton("Import", this);
    m_importBtn->setFixedWidth(120);
    m_importBtn->setDefault(true);
    m_importBtn->setEnabled(false);
    m_importBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    buttonLayout->addWidget(m_importBtn);

    mainLayout->addLayout(buttonLayout);

    // ── Connections ──────────────────────────────────────────────────────
    connect(m_browseBtn, &QPushButton::clicked, this, &ImportDialog::browseFile);
    connect(m_searchBtn, &QPushButton::clicked, this, &ImportDialog::searchOnline);
    connect(m_importBtn, &QPushButton::clicked, this, &ImportDialog::onAccepted);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ImportDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QGroupBox { border:1px solid #3f3f46; border-radius:6px; padding-top:16px; margin-top:8px; font-weight:bold; color:#ccc; }
        QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; }
        QLineEdit { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px 8px; color:#fff; }
        QTextEdit { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px; color:#d4d4d4; font-family:"Consolas","Courier New",monospace; font-size:12px; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QPushButton:disabled { background:#2a2a2a; color:#555; }
        QLabel { color:#ccc; border:none; }
    )");
}

void ImportDialog::browseFile() {
    QString filter = "Subtitle Files (*.lrc *.srt *.txt *.json *.vtt *.ass *.ssa *.ttml);;Lyric (*.lrc);;SubRip (*.srt);;Text (*.txt);;Whisper JSON (*.json);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, "Select Subtitle File", "", filter);
    if (!path.isEmpty()) {
        onFileSelected(path);
    }
}

void ImportDialog::searchOnline() {
    LyricsSearchDialog dialog(this);
    
    // If we have a filename already, try to use it as initial search
    if (!m_pathEdit->text().isEmpty()) {
        QFileInfo fi(m_pathEdit->text());
        dialog.setInitialSearch(fi.baseName());
    }

    if (dialog.exec() == QDialog::Accepted) {
        QString content = dialog.syncedLyrics();
        if (content.isEmpty()) content = dialog.plainLyrics();

        if (content.isEmpty()) return;

        // Save to temporary file
        QString tempPath = QDir::tempPath() + "/ncktv_online_lyrics.lrc";
        QFile file(tempPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(content.toUtf8());
            file.close();
            onFileSelected(tempPath);
            
            // Mark it as online
            m_formatLabel->setText("Format: LRC (Online via LRCLIB)");
        }
    }
}

void ImportDialog::onFileSelected(const QString& path) {
    m_pathEdit->setText(path);
    detectFormat(path);

    // Load preview (first 30 lines)
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.readAll());
        QStringList lines = content.split('\n');
        int totalLines = lines.size();
        m_lineCountLabel->setText(QString("%1 lines").arg(totalLines));

        // Show first 30 lines
        QStringList preview = lines.mid(0, 30);
        m_previewText->setPlainText(preview.join('\n'));
        if (totalLines > 30) {
            m_previewText->append(QString("\n... (%1 more lines)").arg(totalLines - 30));
        }
        m_importBtn->setEnabled(true);
    } else {
        m_previewText->setPlainText("(Could not read file)");
        m_importBtn->setEnabled(false);
    }
}

void ImportDialog::detectFormat(const QString& path) {
    QFileInfo fi(path);
    QString ext = fi.suffix().toLower();

    if (ext == "lrc")       m_detectedFormat = "LRC";
    else if (ext == "srt")  m_detectedFormat = "SRT";
    else if (ext == "vtt")  m_detectedFormat = "VTT";
    else if (ext == "ass")  m_detectedFormat = "ASS";
    else if (ext == "ssa")  m_detectedFormat = "SSA";
    else if (ext == "ttml") m_detectedFormat = "TTML";
    else if (ext == "json") m_detectedFormat = "Whisper JSON";
    else if (ext == "txt")  m_detectedFormat = "Plain Text";
    else                    m_detectedFormat = "Unknown";

    m_formatLabel->setText("Format: " + m_detectedFormat);
}

void ImportDialog::onAccepted() {
    if (m_pathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "No File", "Please select a file to import.");
        return;
    }
    accept();
}

QString ImportDialog::filePath()       const { return m_pathEdit->text(); }
QString ImportDialog::detectedFormat() const { return m_detectedFormat; }

} // namespace ncktv
