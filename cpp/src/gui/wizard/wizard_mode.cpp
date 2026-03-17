#include "wizard_mode.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGraphicsDropShadowEffect>
#include "workers/vocal_separator_worker.h"
#include "dialogs/lyrics_search_dialog.h"
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QFrame>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>

namespace ncktv {

WizardMode::WizardMode(ConfigManager* config, QWidget* parent)
    : QWidget(parent)
    , m_config(config)
{
    setupUi();
}
void WizardMode::reset() {
    m_pages->setCurrentIndex(0);
    m_selectedFile.clear();
    m_onlineLyrics.clear();
    if (m_filePathEdit) m_filePathEdit->clear();
    if (m_startBtn) m_startBtn->setEnabled(false);
    if (m_dropTitle) m_dropTitle->setText("Upload Source");
    if (m_dropSubtitle) m_dropSubtitle->setText("Drag and drop MKV, MP4, or WAV files to begin rendering");
    if (m_dropZone) m_dropZone->setStyleSheet("border: 2px dashed rgba(255, 255, 255, 0.1); border-radius: 12px; background: rgba(255, 255, 255, 0.02);");
}

void WizardMode::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_pages = new QStackedWidget(this);
    mainLayout->addWidget(m_pages);

    createWelcomePage();
    createProcessingPage();
    
    m_pages->setCurrentIndex(0);
}

// ════════════════════════════════════════════════════════════════════════════
// PAGE 1: NC-KTV PRO DASHBOARD
// Matches the prototype's hero section + media pipeline card layout
// ════════════════════════════════════════════════════════════════════════════

void WizardMode::createWelcomePage() {
    auto* page = new QWidget();
    page->setStyleSheet("background-color: #0b101b;");
    auto* outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ── TOP NAV BAR ──────────────────────────────────────────────────────────
    auto* topNav = new QWidget(page);
    topNav->setFixedHeight(56);
    topNav->setStyleSheet(
        "background: rgba(11, 16, 27, 0.9);"
        "border-bottom: 1px solid rgba(255, 255, 255, 0.05);"
    );
    auto* navLayout = new QHBoxLayout(topNav);
    navLayout->setContentsMargins(24, 0, 24, 0);

    auto* brandLabel = new QLabel("NC-KTV", topNav);
    brandLabel->setStyleSheet(
        "font-family: 'Segoe UI', 'Outfit', sans-serif;"
        "font-size: 18px; font-weight: 700; color: #e2e8f0;"
        "letter-spacing: -0.5px; border: none; background: transparent;"
    );
    navLayout->addWidget(brandLabel);

    auto* versionLabel = new QLabel("PRO", topNav);
    versionLabel->setStyleSheet(
        "font-size: 10px; font-weight: 800; color: #3b82f6;"
        "margin-left: -4px; margin-top: 6px; border: none; background: transparent;"
    );
    navLayout->addWidget(versionLabel);
    
    // Custom Headers (File, Edit, etc)
    auto* menuStack = new QHBoxLayout();
    menuStack->setSpacing(20);
    menuStack->setContentsMargins(40, 0, 0, 0);
    
    auto makeMenuBtn = [&](const QString& text) {
        auto* btn = new QPushButton(text, topNav);
        btn->setStyleSheet("QPushButton { color: #94a3b8; font-size: 12px; font-weight: 500; background: transparent; border: none; padding: 4px; } QPushButton:hover { color: white; }");
        return btn;
    };
    
    auto* btnFile = makeMenuBtn("File");
    auto* btnEdit = makeMenuBtn("Edit");
    auto* btnProj = makeMenuBtn("Project");

    auto* mFile = new QMenu(this);
    mFile->setStyleSheet("QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid rgba(255,255,255,0.1); padding: 4px; } QMenu::item { padding: 8px 32px; border-radius: 4px; } QMenu::item:selected { background-color: #3b82f6; color: white; }");
    mFile->addAction("New Project...", this, [this]{ emit requestNew(); });
    mFile->addAction("Open Project...", this, [this]{ emit requestOpen(); });
    mFile->addSeparator();
    mFile->addAction("Preferences...", this, [this]{ emit requestPreferences(); });
    btnFile->setMenu(mFile);

    auto* mEdit = new QMenu(this);
    mEdit->setStyleSheet(mFile->styleSheet());
    mEdit->addAction("Undo")->setEnabled(false);
    mEdit->addAction("Redo")->setEnabled(false);
    btnEdit->setMenu(mEdit);

    auto* mProj = new QMenu(this);
    mProj->setStyleSheet(mFile->styleSheet());
    mProj->addAction("Project Settings...", this, [this]{ emit requestPreferences(); });
    btnProj->setMenu(mProj);

    menuStack->addWidget(btnFile);
    menuStack->addWidget(btnEdit);
    menuStack->addWidget(btnProj);
    navLayout->addLayout(menuStack);

    navLayout->addStretch();

    auto* consoleBtn = new QPushButton("Console", topNav);
    consoleBtn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3b82f6, stop:1 #8b5cf6);"
        "  color: white; border: none; padding: 8px 20px;"
        "  border-radius: 6px; font-size: 12px; font-weight: 600;"
        "}"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #60a5fa, stop:1 #a78bfa); }"
    );
    connect(consoleBtn, &QPushButton::clicked, this, [this](){
        QString logPath = QCoreApplication::applicationDirPath() + "/debug.log";
        if (!QFile::exists(logPath)) logPath = QDir::currentPath() + "/debug.log";
        QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
    });
    navLayout->addWidget(consoleBtn);
    outerLayout->addWidget(topNav);

    // ── MAIN CONTENT: HERO + PIPELINE CARD ───────────────────────────────────
    auto* contentWidget = new QWidget(page);
    contentWidget->setStyleSheet("background: transparent;");
    auto* contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(80, 40, 80, 40);
    contentLayout->setSpacing(60);

    // ===== LEFT: HERO SECTION =====
    auto* heroWidget = new QWidget(contentWidget);
    heroWidget->setStyleSheet("background: transparent;");
    auto* heroLayout = new QVBoxLayout(heroWidget);
    heroLayout->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    heroLayout->setSpacing(16);

    // Status Badge
    auto* statusBadge = new QLabel("SYSTEM ONLINE: V1.0.4-BETA", heroWidget);
    statusBadge->setStyleSheet(
        "background: rgba(59, 130, 246, 0.1);"
        "border: 1px solid rgba(59, 130, 246, 0.2);"
        "color: #3b82f6; font-size: 11px; font-weight: 700;"
        "padding: 6px 14px; border-radius: 14px;"
    );
    statusBadge->setFixedWidth(statusBadge->sizeHint().width() + 28);
    heroLayout->addWidget(statusBadge);

    // Hero Title
    m_heroTitle = new QLabel("NC-KTV Pro", heroWidget);
    m_heroTitle->setStyleSheet(
        "font-family: 'Segoe UI', 'Outfit', sans-serif;"
        "font-size: 52px; font-weight: 700; color: #e2e8f0;"
        "letter-spacing: -2px; background: transparent; border: none;"
    );
    heroLayout->addWidget(m_heroTitle);

    auto* heroDesc = new QLabel(
        "High-performance, developer-focused media engine\n"
        "for automated KTV processing and real-time visualization.", heroWidget);
    heroDesc->setStyleSheet(
        "color: #94a3b8; font-size: 14px; line-height: 1.6;"
        "background: transparent; border: none;"
    );
    heroLayout->addWidget(heroDesc);
    heroLayout->addSpacing(16);

    // Stats Grid
    auto* statsWidget = new QWidget(heroWidget);
    statsWidget->setStyleSheet("background: transparent;");
    auto* statsLayout = new QHBoxLayout(statsWidget);
    statsLayout->setSpacing(16);
    statsLayout->setContentsMargins(0, 0, 0, 0);

    auto createStatCard = [](const QString& value, const QString& label, QWidget* parent) -> QWidget* {
        auto* card = new QWidget(parent);
        card->setMinimumWidth(130);
        card->setStyleSheet(
            "background: rgba(18, 24, 39, 0.7);"
            "border: 1px solid rgba(255, 255, 255, 0.05);"
            "border-radius: 14px;"
        );
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        auto* valLbl = new QLabel(value, card);
        valLbl->setStyleSheet(
            "font-family: 'Segoe UI', 'Outfit', sans-serif;"
            "font-size: 24px; font-weight: 700; color: #e2e8f0;"
            "background: transparent; border: none;"
        );
        cardLayout->addWidget(valLbl);
        auto* lbLbl = new QLabel(label, card);
        lbLbl->setStyleSheet(
            "font-size: 10px; font-weight: 700; color: #94a3b8;"
            "letter-spacing: 1px; background: transparent; border: none;"
        );
        cardLayout->addWidget(lbLbl);
        return card;
    };

    statsLayout->addWidget(createStatCard("0ms", "LATENCY", statsWidget));
    statsLayout->addWidget(createStatCard("4K", "RESOLUTION", statsWidget));
    statsLayout->addWidget(createStatCard("FFMPEG", "NATIVE CORE", statsWidget));

    heroLayout->addWidget(statsWidget);
    contentLayout->addWidget(heroWidget, 1);

    // ===== RIGHT: MEDIA PIPELINE CARD =====
    auto* pipelineCard = new QWidget(contentWidget);
    pipelineCard->setFixedWidth(400);
    pipelineCard->setStyleSheet(
        "background: rgba(18, 24, 39, 0.8);"
        "border: 1px solid rgba(255, 255, 255, 0.1);"
        "border-radius: 20px;"
    );
    auto* pipeLayout = new QVBoxLayout(pipelineCard);
    pipeLayout->setContentsMargins(24, 24, 24, 24);
    pipeLayout->setSpacing(16);

    // Card Header
    auto* cardHeader = new QLabel("Media Pipeline", pipelineCard);
    cardHeader->setStyleSheet(
        "font-size: 16px; font-weight: 700; color: #e2e8f0;"
        "background: transparent; border: none;"
    );
    pipeLayout->addWidget(cardHeader);

    // Drop Zone
    m_dropZone = new QWidget(pipelineCard);
    m_dropZone->setStyleSheet(
        "border: 2px dashed rgba(255, 255, 255, 0.1);"
        "border-radius: 12px;"
        "background: rgba(255, 255, 255, 0.02);"
    );
    auto* dropLayout = new QVBoxLayout(m_dropZone);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setContentsMargins(16, 24, 16, 24);

    auto* uploadIcon = new QLabel("↑", m_dropZone);
    uploadIcon->setStyleSheet(
        "font-size: 22px; color: #3b82f6;"
        "background: rgba(59, 130, 246, 0.1);"
        "border-radius: 10px; border: none;"
        "padding: 8px;"
    );
    uploadIcon->setFixedSize(44, 44);
    uploadIcon->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(uploadIcon, 0, Qt::AlignCenter);

    m_dropTitle = new QLabel("Upload Source", m_dropZone);
    m_dropTitle->setStyleSheet(
        "font-weight: 600; font-size: 14px; color: #e2e8f0;"
        "background: transparent; border: none;"
    );
    m_dropTitle->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(m_dropTitle);

    m_dropSubtitle = new QLabel("Drag and drop MKV, MP4, or WAV files to begin rendering", m_dropZone);
    m_dropSubtitle->setStyleSheet(
        "font-size: 11px; color: #94a3b8;"
        "background: transparent; border: none;"
    );
    m_dropSubtitle->setAlignment(Qt::AlignCenter);
    m_dropSubtitle->setWordWrap(true);
    dropLayout->addWidget(m_dropSubtitle);
    dropLayout->addSpacing(8);

    // Browse + Search Buttons
    auto* browseRow = new QHBoxLayout();
    browseRow->setAlignment(Qt::AlignCenter);
    browseRow->setSpacing(8);
    
    auto* browseBtn = new QPushButton("Browse Files", m_dropZone);
    browseBtn->setStyleSheet(
        "QPushButton {"
        "  background: #1a2333; border: 1px solid rgba(255, 255, 255, 0.08);"
        "  color: #e2e8f0; padding: 8px 16px; border-radius: 6px;"
        "  font-size: 12px; font-weight: 600;"
        "}"
        "QPushButton:hover { border-color: #3b82f6; background: rgba(59, 130, 246, 0.1); }"
    );
    connect(browseBtn, &QPushButton::clicked, this, &WizardMode::onBrowseFile);
    browseRow->addWidget(browseBtn);
    
    auto* searchBtn = new QPushButton("🔍 Search Lyrics", m_dropZone);
    searchBtn->setStyleSheet(
        "QPushButton {"
        "  background: rgba(59, 130, 246, 0.1); border: 1px solid rgba(59, 130, 246, 0.2);"
        "  color: #3b82f6; padding: 8px 16px; border-radius: 6px;"
        "  font-size: 12px; font-weight: 600;"
        "}"
        "QPushButton:hover { background: rgba(59, 130, 246, 0.2); }"
    );
    connect(searchBtn, &QPushButton::clicked, this, [this](){
        LyricsSearchDialog dialog(this);
        if (!m_filePathEdit->text().isEmpty()) {
            QFileInfo fi(m_filePathEdit->text());
            dialog.setInitialSearch(fi.baseName());
        }
        if (dialog.exec() == QDialog::Accepted) {
             m_onlineLyrics = dialog.syncedLyrics();
             if (m_onlineLyrics.isEmpty()) m_onlineLyrics = dialog.plainLyrics();
             
             if (!m_onlineLyrics.isEmpty()) {
                 QMessageBox::information(this, "Lyrics Found", "Synced lyrics found online! These will be used instead of AI transcription.");
                 m_transcribeCheck->setChecked(false);
             }
        }
    });
    browseRow->addWidget(searchBtn);
    dropLayout->addLayout(browseRow);

    pipeLayout->addWidget(m_dropZone);

    // Hidden file path edit (still needed for logic)
    m_filePathEdit = new QLineEdit(pipelineCard);
    m_filePathEdit->setVisible(false);
    pipeLayout->addWidget(m_filePathEdit);

    // Settings Section
    auto* settingsLabel = new QLabel("PROCESSING PROFILE", pipelineCard);
    settingsLabel->setStyleSheet(
        "font-size: 10px; font-weight: 700; color: #94a3b8;"
        "letter-spacing: 0.5px; background: transparent; border: none;"
    );
    pipeLayout->addWidget(settingsLabel);

    // Transcription Toggle + Language
    auto* optionsRow = new QHBoxLayout();

    m_transcribeCheck = new QCheckBox("Auto-generate Subs", pipelineCard);
    m_transcribeCheck->setChecked(true);
    m_transcribeCheck->setStyleSheet(
        "QCheckBox { color: #94a3b8; font-size: 12px; background: transparent; border: none; }"
        "QCheckBox::indicator { width: 28px; height: 16px; border-radius: 8px; }"
        "QCheckBox::indicator:unchecked { background: #1a2333; border: 1px solid rgba(255,255,255,0.08); }"
        "QCheckBox::indicator:checked { background: #3b82f6; border: 1px solid #3b82f6; }"
    );
    optionsRow->addWidget(m_transcribeCheck);

    m_langCombo = new QComboBox(pipelineCard);
    m_langCombo->addItems({"Auto", "English (en)", "Malay (ms)", "Indonesian (id)", "Japanese (ja)", "Korean (ko)", "Chinese (zh)"});
    m_langCombo->setStyleSheet(
        "QComboBox { background: #1a2333; color: #e2e8f0; border: 1px solid rgba(255,255,255,0.08);"
        "  border-radius: 6px; padding: 4px 8px; font-size: 11px; }"
    );
    optionsRow->addWidget(m_langCombo);
    pipeLayout->addLayout(optionsRow);

    // UVR Model Selector
    m_modelCombo = new QComboBox(pipelineCard);
    m_modelCombo->setStyleSheet(
        "QComboBox { background: #1a2333; color: #e2e8f0; border: 1px solid rgba(255,255,255,0.08);"
        "  border-radius: 6px; padding: 6px 10px; font-size: 12px; }"
    );

    QStringList modelDirs = {
        QCoreApplication::applicationDirPath() + "/models/uvr",
        QDir::currentPath() + "/models/uvr"
    };
    QStringList found;
    for (const QString& dirPath : modelDirs) {
        QDir dir(dirPath);
        if (dir.exists()) {
            const auto entries = dir.entryList({"*.onnx", "*.pth"}, QDir::Files);
            for (const QString& f : entries) {
                if (!found.contains(f)) found << f;
            }
        }
    }
    if (found.isEmpty()) {
        found << "UVR_MDXNET_KARA_2.onnx" << "UVR-MDX-NET-Inst_HQ_3.onnx";
    }
    m_modelCombo->addItems(found);

    if (m_config) {
        QString defUvr = m_config->get<QString>("ai.uvr_model", "UVR_MDXNET_KARA_2.onnx");
        int idx = m_modelCombo->findText(defUvr);
        if (idx != -1) m_modelCombo->setCurrentIndex(idx);
        else m_modelCombo->setCurrentText(defUvr);
    }

    pipeLayout->addWidget(m_modelCombo);

    if (m_config) {
        QString defLang = m_config->get<QString>("ai.language", "Auto");
        int idx = m_langCombo->findText(defLang, Qt::MatchContains);
        if (idx != -1) m_langCombo->setCurrentIndex(idx);
    }

    // START ENGINE BUTTON
    m_startBtn = new QPushButton("🚀  Start Engine", pipelineCard);
    m_startBtn->setEnabled(false);
    m_startBtn->setMinimumHeight(48);
    m_startBtn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3b82f6, stop:1 #8b5cf6);"
        "  color: white; border: none; border-radius: 10px;"
        "  font-weight: 700; font-size: 15px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #60a5fa, stop:1 #a78bfa);"
        "}"
        "QPushButton:disabled {"
        "  background: #1e293b; color: #475569;"
        "}"
    );
    connect(m_startBtn, &QPushButton::clicked, this, &WizardMode::onStartClicked);
    pipeLayout->addWidget(m_startBtn);

    contentLayout->addWidget(pipelineCard);
    outerLayout->addWidget(contentWidget, 1);

    // ── BOTTOM STATUS BAR ────────────────────────────────────────────────────
    auto* bottomBar = new QWidget(page);
    bottomBar->setFixedHeight(36);
    bottomBar->setStyleSheet(
        "background: #0b101b;"
        "border-top: 1px solid rgba(255, 255, 255, 0.05);"
    );
    auto* barLayout = new QHBoxLayout(bottomBar);
    barLayout->setContentsMargins(24, 0, 24, 0);

    auto* statusItem = new QLabel("● Worker Cluster: active", bottomBar);
    statusItem->setStyleSheet("color: #10b981; font-size: 11px; font-weight: 600; background: transparent; border: none;");
    barLayout->addWidget(statusItem);

    auto* memItem = new QLabel("MEM: — / —", bottomBar);
    memItem->setStyleSheet("color: #94a3b8; font-size: 11px; background: transparent; border: none;");
    barLayout->addWidget(memItem);

    barLayout->addStretch();
    auto* copyright = new QLabel("© 2026 NC-KTV Systems.", bottomBar);
    copyright->setStyleSheet("color: #475569; font-size: 11px; background: transparent; border: none;");
    barLayout->addWidget(copyright);

    outerLayout->addWidget(bottomBar);

    m_pages->addWidget(page);
}

// ════════════════════════════════════════════════════════════════════════════
// PAGE 2: NC-KTV PRO PROCESSOR
// Matches the prototype's processing/loading screen design
// ════════════════════════════════════════════════════════════════════════════

void WizardMode::createProcessingPage() {
    auto* page = new QWidget();
    page->setStyleSheet("background-color: #0b101b;");
    auto* outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Top Nav
    auto* topNav = new QWidget(page);
    topNav->setFixedHeight(56);
    topNav->setStyleSheet(
        "background: rgba(11, 16, 27, 0.9);"
        "border-bottom: 1px solid rgba(255, 255, 255, 0.05);"
    );
    auto* navLayout = new QHBoxLayout(topNav);
    navLayout->setContentsMargins(24, 0, 24, 0);
    auto* brandLabel = new QLabel("NC-KTV Processor", topNav);
    brandLabel->setStyleSheet(
        "font-family: 'Segoe UI', 'Outfit', sans-serif;"
        "font-size: 16px; font-weight: 700; color: #e2e8f0;"
        "background: transparent; border: none;"
    );
    navLayout->addWidget(brandLabel);
    auto* engineLabel = new QLabel("ADVANCED ENGINE", topNav);
    engineLabel->setStyleSheet(
        "font-size: 10px; font-weight: 700; color: #3b82f6;"
        "margin-left: 4px; margin-top: 3px; background: transparent; border: none;"
    );
    navLayout->addWidget(engineLabel);
    navLayout->addStretch();
    outerLayout->addWidget(topNav);

    // Central Processing Card
    auto* centerWidget = new QWidget(page);
    auto* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(120, 40, 120, 40);
    centerLayout->setAlignment(Qt::AlignCenter);

    // Main Progress Card
    auto* progressCard = new QWidget(centerWidget);
    progressCard->setStyleSheet(
        "background: rgba(255, 255, 255, 0.03);"
        "border: 1px solid rgba(255, 255, 255, 0.08);"
        "border-radius: 16px;"
    );
    auto* pcLayout = new QVBoxLayout(progressCard);
    pcLayout->setContentsMargins(32, 28, 32, 28);
    pcLayout->setSpacing(16);

    // Active Status Indicator
    auto* statusRow = new QHBoxLayout();
    auto* dot = new QLabel("●", progressCard);
    dot->setStyleSheet("color: #3b82f6; font-size: 10px; background: transparent; border: none;");
    statusRow->addWidget(dot);
    auto* activeLabel = new QLabel("ACTIVE PROCESSING", progressCard);
    activeLabel->setStyleSheet(
        "color: #3b82f6; font-size: 11px; font-weight: 700;"
        "letter-spacing: 1px; background: transparent; border: none;"
    );
    statusRow->addWidget(activeLabel);
    statusRow->addStretch();
    pcLayout->addLayout(statusRow);

    // Task Title + Percentage
    auto* infoRow = new QHBoxLayout();
    auto* textBlock = new QVBoxLayout();

    m_taskStatusLabel = new QLabel("Optimizing Media...", progressCard);
    m_taskStatusLabel->setStyleSheet(
        "font-family: 'Segoe UI', 'Outfit', sans-serif;"
        "font-size: 28px; font-weight: 700; color: #e2e8f0;"
        "background: transparent; border: none;"
    );
    textBlock->addWidget(m_taskStatusLabel);

    m_statusLabel = new QLabel("High Fidelity Synthesis in progress", progressCard);
    m_statusLabel->setStyleSheet(
        "color: #94a3b8; font-size: 13px;"
        "background: transparent; border: none;"
    );
    textBlock->addWidget(m_statusLabel);
    infoRow->addLayout(textBlock, 1);

    // Percentage Display
    auto* percentBlock = new QVBoxLayout();
    percentBlock->setAlignment(Qt::AlignRight);
    m_percentLabel = new QLabel("0%", progressCard);
    m_percentLabel->setStyleSheet(
        "font-family: 'Segoe UI', 'Outfit', sans-serif;"
        "font-size: 42px; font-weight: 700; color: #3b82f6;"
        "background: transparent; border: none;"
    );
    m_percentLabel->setAlignment(Qt::AlignRight);
    percentBlock->addWidget(m_percentLabel);
    auto* compLabel = new QLabel("COMPLETED", progressCard);
    compLabel->setStyleSheet(
        "color: #94a3b8; font-size: 10px; font-weight: 700;"
        "background: transparent; border: none;"
    );
    compLabel->setAlignment(Qt::AlignRight);
    percentBlock->addWidget(compLabel);
    infoRow->addLayout(percentBlock);
    pcLayout->addLayout(infoRow);

    // Progress Bar
    m_progressBar = new QProgressBar(progressCard);
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedHeight(10);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  border: none; border-radius: 5px;"
        "  background: rgba(255, 255, 255, 0.05);"
        "}"
        "QProgressBar::chunk {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #8b5cf6);"
        "  border-radius: 5px;"
        "}"
    );
    pcLayout->addWidget(m_progressBar);

    centerLayout->addWidget(progressCard);
    outerLayout->addWidget(centerWidget, 1);

    // Bottom status
    auto* bottomBar = new QWidget(page);
    bottomBar->setFixedHeight(36);
    bottomBar->setStyleSheet(
        "background: #0b101b;"
        "border-top: 1px solid rgba(255, 255, 255, 0.05);"
    );
    auto* barLayout = new QHBoxLayout(bottomBar);
    barLayout->setContentsMargins(24, 0, 24, 0);
    auto* buildLabel = new QLabel("© 2026 NC-KTV SYSTEMS. BUILD 2.0.42-STABLE", bottomBar);
    buildLabel->setStyleSheet("color: #475569; font-size: 11px; background: transparent; border: none;");
    barLayout->addStretch();
    barLayout->addWidget(buildLabel);
    outerLayout->addWidget(bottomBar);

    m_pages->addWidget(page);
}

// ════════════════════════════════════════════════════════════════════════════
// INTERACTIVITY (unchanged logic, adapted for new UI members)
// ════════════════════════════════════════════════════════════════════════════

void WizardMode::onBrowseFile() {
    QString path = QFileDialog::getOpenFileName(this, "Select Media", "", "Media Files (*.mp4 *.mkv *.mp3 *.wav *.flac);;All Files (*)");
    if (!path.isEmpty()) {
        m_filePathEdit->setText(path);
        m_selectedFile = path;
        m_onlineLyrics.clear();
        m_startBtn->setEnabled(true);

        // Update drop zone to show selected file
        QFileInfo fi(path);
        m_dropTitle->setText("Selected File:");
        m_dropSubtitle->setText(fi.fileName());
        m_dropZone->setStyleSheet(
            "border: 2px dashed rgba(16, 185, 129, 0.3);"
            "border-radius: 12px;"
            "background: rgba(16, 185, 129, 0.05);"
        );
    }
}

void WizardMode::onStartClicked() {
    m_pages->setCurrentIndex(1);
    
    m_project = new Project(m_selectedFile);
    
    m_taskStatusLabel->setText("Separating Audio...");
    m_statusLabel->setText("Extracting vocals and instruments from source media");
    m_progressBar->setValue(10);
    m_percentLabel->setText("10%");
    
    // Start vocal separation worker in a background thread
    auto* thread = new QThread(this);
    auto* worker = new VocalSeparatorWorker();
    worker->moveToThread(thread);
    
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    connect(worker, &VocalSeparatorWorker::separationComplete, this, &WizardMode::onSeparationFinished);
    connect(worker, &VocalSeparatorWorker::error, this, &WizardMode::onSeparationError);
    connect(worker, &VocalSeparatorWorker::progress, this, [this](int val, const QString& msg){
        m_progressBar->setValue(val);
        m_percentLabel->setText(QString("%1%").arg(val));
        m_statusLabel->setText(msg);
    });

    connect(worker, &VocalSeparatorWorker::separationComplete, thread, &QThread::quit);
    connect(worker, &VocalSeparatorWorker::error, thread, &QThread::quit);

    thread->start();
    
    QString selectedModel = m_modelCombo ? m_modelCombo->currentText() : "UVR_MDXNET_KARA_2.onnx";
    if (selectedModel.isEmpty()) selectedModel = "UVR_MDXNET_KARA_2.onnx";

    QMetaObject::invokeMethod(worker, "startSeparation", Qt::QueuedConnection,
                              Q_ARG(QString, m_selectedFile),
                              Q_ARG(QString, selectedModel),
                              Q_ARG(QString, "output"));
}

void WizardMode::onSeparationFinished(const QString& instrumentalPath, const QString& vocalsPath) {
    m_progressBar->setValue(100);
    m_percentLabel->setText("100%");
    m_taskStatusLabel->setText("Processing Complete");
    m_statusLabel->setText("Loading project...");
    
    m_project->instrumentalPath = instrumentalPath;
    m_project->vocalsPath = vocalsPath;
    m_project->originalAudioPath = m_selectedFile;
    
    double duration = 180.0;
    
    auto& track = m_project->timeline.addTrack("inst_track", TrackType::Audio, "Instrumental");
    
    Clip instClip;
    instClip.clipId = "inst_clip_0";
    instClip.trackId = "inst_track";
    instClip.sourceFile = instrumentalPath;
    instClip.startTime = 0.0;
    instClip.duration = duration;
    
    track.addClip(instClip);

    qDebug() << "Separation Finished! Inst:" << instrumentalPath << "Vocals:" << vocalsPath << "Duration:" << duration;
    
    if (!m_onlineLyrics.isEmpty()) {
        m_project->lyrics.importFromLrc(m_onlineLyrics);
        emit projectReady(m_project);
    } else if (m_transcribeCheck->isChecked() && !vocalsPath.isEmpty()) {
        m_progressBar->setValue(0);
        m_percentLabel->setText("0%");
        m_taskStatusLabel->setText("Transcribing Vocals...");
        m_statusLabel->setText("AI lyrics transcription in progress");
        
        auto* tThread = new QThread(this);
        auto* tWorker = new TranscriptionWorker();
        tWorker->moveToThread(tThread);
        
        connect(tThread, &QThread::finished, tWorker, &QObject::deleteLater);
        connect(tThread, &QThread::finished, tThread, &QObject::deleteLater);

        connect(tWorker, &TranscriptionWorker::transcriptionComplete, this, &WizardMode::onTranscriptionFinished);
        connect(tWorker, &TranscriptionWorker::error, this, &WizardMode::onTranscriptionError);
        connect(tWorker, &TranscriptionWorker::progressUpdated, this, [this](const QString& msg){
            m_progressBar->setValue(50);
            m_percentLabel->setText("50%");
            m_statusLabel->setText(msg);
        });
        
        connect(tWorker, &TranscriptionWorker::transcriptionComplete, tThread, &QThread::quit);
        connect(tWorker, &TranscriptionWorker::error, tThread, &QThread::quit);

        QString langStr = m_langCombo->currentText();
        QString langCode = "auto";
        if (langStr.contains("(")) {
            langCode = langStr.split("(").last().replace(")", "").trimmed();
        }
        
        tThread->start();
        
        QString whisperModel = m_config ? m_config->get<QString>("ai.whisper_model", "medium") : "medium";
        
        QMetaObject::invokeMethod(tWorker, "startTranscription", Qt::QueuedConnection,
                                  Q_ARG(QString, vocalsPath),
                                  Q_ARG(QString, whisperModel),
                                  Q_ARG(QString, langCode));
    } else {
        emit projectReady(m_project);
    }
}

void WizardMode::onTranscriptionFinished(const QString& resultJson) {
    m_progressBar->setValue(100);
    m_percentLabel->setText("100%");
    m_taskStatusLabel->setText("Complete!");
    m_statusLabel->setText("Launching Editor...");
    
    m_project->lyrics.importFromWhisperJson(resultJson);
    
    emit projectReady(m_project);
}

void WizardMode::onTranscriptionError(const QString& error) {
    QMessageBox::warning(this, "Transcription Failed", "Could not transcribe vocals:\n" + error + "\n\nProceeding without lyrics.");
    emit projectReady(m_project);
}

void WizardMode::onSeparationError(const QString& error) {
    QMessageBox::critical(this, "Separation Error", "Failed to separate audio stems:\n" + error);
    m_pages->setCurrentIndex(0);
}

} // namespace ncktv
