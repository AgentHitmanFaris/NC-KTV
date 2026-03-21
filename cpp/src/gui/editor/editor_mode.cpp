#include "editor_mode.h"
#include "../../core/parsers/subtitle_parser.h"
#include "../../core/lyrics/lyrics_data.h"
#include <QDebug>
#include <QShortcut>
#include <QLabel>
#include <cmath>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QEvent>
#include <QProgressBar>
#include <QTimer>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QTableWidget>
#include <QMainWindow>
#include <QDockWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QMenu>
#include <QLineEdit>
#include <QComboBox>
#include <QSplitter>
#include <QCheckBox>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextEdit>
#include "../workers/transcription_worker.h"
#include "../workers/export_worker.h"
#include "../workers/waveform_worker.h"
#include <QThread>
#include "../dialogs/word_editor.h"
#include "../dialogs/export_dialog.h"
#include "../dialogs/preferences_dialog.h"
#include "../dialogs/import_dialog.h"
#include <QClipboard>
#include <QApplication>
#include <QProgressDialog>
#include <QProcess>
#include <QDir>
#include "../../core/parsers/ass_generator.h"

namespace ncktv {
static QString formatTimeMMSS(double seconds) {
    int mins = static_cast<int>(seconds) / 60; double secs = std::fmod(seconds, 60.0);
    return QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 5, 'f', 2, QChar('0'));
}


class ProcessingOverlay : public QWidget {
public:
    QProgressBar* bar;
    QLabel* msg;
    QLabel* iconLbl;
    ProcessingOverlay(QWidget* parent) : QWidget(parent) {
        setGeometry(parent->rect());
        setStyleSheet("ProcessingOverlay { background: rgba(0, 0, 0, 200); }");
        auto* layout = new QVBoxLayout(this);
        layout->setAlignment(Qt::AlignCenter);
        auto* popup = new QWidget(this);
        popup->setObjectName("PopupContainer");
        popup->setFixedSize(450, 200);
        popup->setStyleSheet("QWidget#PopupContainer { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0f172a, stop:1 #1e293b); border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 16px; }");
        auto* popupLayout = new QVBoxLayout(popup);
        popupLayout->setAlignment(Qt::AlignCenter);
        popupLayout->setContentsMargins(30, 25, 30, 25);
        popupLayout->setSpacing(15);
        iconLbl = new QLabel("✨", popup);
        iconLbl->setStyleSheet("font-size: 32px; background: transparent; border: none;");
        iconLbl->setAlignment(Qt::AlignCenter);
        popupLayout->addWidget(iconLbl);
        msg = new QLabel("Initializing AI transcription...", popup);
        msg->setStyleSheet("color: #a9b1d6; font-size: 15px; font-weight: bold; border: none; background: transparent; font-family: 'Segoe UI', Arial;");
        msg->setAlignment(Qt::AlignCenter);
        popupLayout->addWidget(msg);
        bar = new QProgressBar(popup);
        bar->setRange(0, 0); 
        bar->setFixedHeight(10);
        bar->setTextVisible(false);
        bar->setStyleSheet("QProgressBar { border: none; border-radius: 5px; background: #16161e; } QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #7aa2f7, stop:1 #bb9af7); border-radius: 5px; }");
        popupLayout->addWidget(bar);
        layout->addWidget(popup);
        setCursor(Qt::BusyCursor);
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
    }
    protected:
    void mousePressEvent(QMouseEvent*) override {}
};

EditorMode::EditorMode(std::shared_ptr<core::Project> project, ConfigManager* config, QWidget* parent)
    : QWidget(parent)
    , m_project(std::move(project))
    , m_config(config)
{
    m_undoManager = new UndoManager(this);
    m_timingHandler = new TimingOffsetHandler(this);
    setupUi();
    setupToolBar();
    setupConnections();
    applyTheme();
    syncViewState();
}

void EditorMode::setupUi() {
    m_mainHLayout = new QHBoxLayout(this);
    m_mainHLayout->setContentsMargins(0, 0, 0, 0);
    m_mainHLayout->setSpacing(0);
    
    // ------------------------------------------
    // 1. LEFT SIDEBAR
    // ------------------------------------------
    m_sidebar = new QWidget(this);
    m_sidebar->setFixedWidth(280);
    m_sidebar->setStyleSheet("background: #111827; border-right: 1px solid rgba(255,255,255,0.05);");
    auto* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(20, 24, 20, 24);
    sidebarLayout->setSpacing(24);
    
    // Project Card
    auto* projectCard = new QWidget(m_sidebar);
    projectCard->setStyleSheet("QWidget { background: rgba(31, 41, 55, 0.5); border: 1px solid rgba(255,255,255,0.08); border-radius: 12px; }");
    auto* pcLayout = new QHBoxLayout(projectCard);
    pcLayout->setContentsMargins(16, 16, 16, 16);
    pcLayout->setSpacing(12);
    auto* projIcon = new QLabel("🎵", projectCard);
    projIcon->setFixedSize(40, 40);
    projIcon->setAlignment(Qt::AlignCenter);
    projIcon->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3b82f6, stop:1 #8b5cf6); border-radius: 10px; font-size: 18px; border: none;");
    pcLayout->addWidget(projIcon);
    auto* projDetails = new QVBoxLayout();
    
    QString pTitle = "Untitled";
    QString pSub = "(PRO_V1)";
    QString pFile = "No file loaded";

    if (m_project) {
        if (m_project->source_file.has_value()) {
            pTitle = QString::fromStdString(m_project->source_file->stem().string());
            pFile = QString::fromStdString(m_project->source_file->filename().string());
        }
        if (!m_project->project_name.empty() && m_project->project_name != "Untitled") {
            pTitle = QString::fromStdString(m_project->project_name);
        }
    }
    
    auto* projNameLabel = new QLabel(pTitle, projectCard);
    projNameLabel->setStyleSheet("font-size: 14px; font-weight: 700; color: white; border: none; background: transparent;");
    projDetails->addWidget(projNameLabel);
    auto* projSubLabel = new QLabel(pSub, projectCard);
    projSubLabel->setStyleSheet("font-size: 10px; color: #64748b; border: none; background: transparent; font-weight: 600;");
    projDetails->addWidget(projSubLabel);
    auto* projFileLabel = new QLabel(pFile, projectCard);
    projFileLabel->setStyleSheet("font-size: 11px; color: #94a3b8; border: none; background: transparent; font-weight: 600;");
    projDetails->addWidget(projFileLabel);
    pcLayout->addLayout(projDetails, 1);
    sidebarLayout->addWidget(projectCard);
    
    // AUDIO MONITOR
    auto* audioMonitorLabel = new QLabel("AUDIO MONITOR", m_sidebar);
    audioMonitorLabel->setStyleSheet("font-size: 10px; font-weight: 800; color: #94a3b8; letter-spacing: 1.5px; border: none; background: transparent;");
    sidebarLayout->addWidget(audioMonitorLabel);
    auto* audioMonWg = new QWidget(m_sidebar);
    audioMonWg->setStyleSheet("background: #0f172a; border-radius: 8px; border: 1px solid rgba(255,255,255,0.03);");
    audioMonWg->setFixedHeight(70);
    sidebarLayout->addWidget(audioMonWg);
    
    // EDITOR MODES
    auto* modesLabel = new QLabel("EDITOR MODES", m_sidebar);
    modesLabel->setStyleSheet("font-size: 10px; font-weight: 800; color: #94a3b8; letter-spacing: 1.5px; border: none; background: transparent;");
    sidebarLayout->addWidget(modesLabel);
    auto* modesLayout = new QVBoxLayout();
    modesLayout->setSpacing(8);
    auto makeSidebarBtn = [](const QString& text) {
        auto* btn = new QPushButton(text);
        btn->setCheckable(true);
        btn->setStyleSheet("QPushButton { text-align: left; padding: 12px 16px; font-size: 13px; font-weight: 600; color: #94a3b8; background: transparent; border: none; border-radius: 8px; } QPushButton:hover { background: rgba(255,255,255,0.03); color: #e2e8f0; } QPushButton:checked { background: rgba(59, 130, 246, 0.15); color: #60a5fa; border: 1px solid rgba(59,130,246,0.3); }");
        return btn;
    };
    m_modeLyricsBtn = makeSidebarBtn("Lyrics Editor");
    m_modeTimingBtn = makeSidebarBtn("Timing Sync");
    m_modeRenderBtn = makeSidebarBtn("Video Render");
    m_modeLyricsBtn->setChecked(true);
    modesLayout->addWidget(m_modeLyricsBtn);
    modesLayout->addWidget(m_modeTimingBtn);
    modesLayout->addWidget(m_modeRenderBtn);
    sidebarLayout->addLayout(modesLayout);
    sidebarLayout->addStretch(1);

    // Console/Debug button
    m_consoleBtn = new QPushButton("Debug Log", m_sidebar);
    m_consoleBtn->setStyleSheet("QPushButton { text-align: left; padding: 12px 16px; font-size: 12px; font-weight: 600; color: #64748b; background: transparent; border: none; border-radius: 8px; } QPushButton:hover { background: rgba(255,255,255,0.03); color: #94a3b8; }");
    sidebarLayout->addWidget(m_consoleBtn);
    
    m_saveProjectBtn = new QPushButton("Save Project", m_sidebar);
    m_saveProjectBtn->setStyleSheet("QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #8b5cf6, stop:1 #6d28d9); color: white; border-radius: 8px; padding: 12px; font-size: 13px; font-weight: bold; border: none; } QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #a78bfa, stop:1 #7c3aed); }");
    sidebarLayout->addWidget(m_saveProjectBtn);
    m_mainHLayout->addWidget(m_sidebar);
    
    // ------------------------------------------
    // 2. RIGHT WORKSPACE (Now dockable via QMainWindow)
    // ------------------------------------------
    auto* rightContainer = new QMainWindow(this);
    rightContainer->setWindowFlags(Qt::Widget);
    rightContainer->setStyleSheet("QMainWindow { background: #0f172a; } QDockWidget { color: white; background: #0f172a; border: 1px solid rgba(255,255,255,0.05); } QDockWidget::title { background: #1e293b; padding: 6px; font-weight: bold; }");
    rightContainer->setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks);

    auto* centralWidget = new QWidget(rightContainer);
    m_workspaceLayout = new QVBoxLayout(centralWidget);
    m_workspaceLayout->setContentsMargins(0, 0, 0, 0); m_workspaceLayout->setSpacing(0);
    rightContainer->setCentralWidget(centralWidget);
    
    // Header Bar
    auto* appHeader = new QWidget(centralWidget);
    appHeader->setFixedHeight(64);
    appHeader->setStyleSheet("background: rgba(11, 16, 27, 0.5); border-bottom: 1px solid rgba(255,255,255,0.05);");
    auto* headerLayout = new QHBoxLayout(appHeader);
    headerLayout->setContentsMargins(24, 0, 24, 0);
    auto* headerTitle = new QLabel("NC-KTV PRO", appHeader);
    headerTitle->setStyleSheet("color: white; font-weight: 900; font-size: 16px; margin-right: 20px; font-style: italic;");
    headerLayout->addWidget(headerTitle);
    auto* menuLayout = new QHBoxLayout();
    menuLayout->setSpacing(20);
    auto makeMenuBtn = [&](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setStyleSheet("QPushButton { color: #94a3b8; font-size: 12px; font-weight: 500; background: transparent; border: none; padding: 4px; } QPushButton:hover { color: white; }");
        return btn;
    };
    auto* btnFile = makeMenuBtn("File");
    auto* btnEdit = makeMenuBtn("Edit");
    auto* btnProj = makeMenuBtn("Project");
    auto* btnExpo = makeMenuBtn("Export");
    
    // Setup popups for these buttons
    auto* mFile = new QMenu(this);
    mFile->setStyleSheet("QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid rgba(255,255,255,0.1); padding: 4px; } QMenu::item { padding: 8px 32px; border-radius: 4px; } QMenu::item:selected { background-color: #3b82f6; color: white; }");
    mFile->addAction("New Project...", this, [this]{ emit requestNew(); }); 
    mFile->addAction("Open Project...", this, [this]{ emit requestOpen(); });
    mFile->addSeparator();
    mFile->addAction("Import Lyrics (JSON/LRC)...", this, &EditorMode::onImportSubtitleClicked);
    mFile->addSeparator();
    mFile->addAction("Save Project", this, [this]{ emit requestSave(); });
    mFile->addAction("Save Project As...", this, [this]{ emit requestSaveAs(); });
    mFile->addSeparator();
    mFile->addAction("Preferences...", this, [this]{ emit requestPreferences(); });
    btnFile->setMenu(mFile);

    auto* mEdit = new QMenu(this);
    mEdit->setStyleSheet(mFile->styleSheet());
    mEdit->addAction("Undo", m_undoManager, &UndoManager::undo);
    mEdit->addAction("Redo", m_undoManager, &UndoManager::redo);
    btnEdit->setMenu(mEdit);

    auto* mProj = new QMenu(this);
    mProj->setStyleSheet(mFile->styleSheet());
    mProj->addAction("AI Transcription (Whisper)...", this, &EditorMode::onAutoWhisperClicked);
    mProj->addSeparator();
    mProj->addAction("Project Settings...", this, [this]{ emit requestPreferences(); });
    btnProj->setMenu(mProj);

    auto* mExpo = new QMenu(this);
    mExpo->setStyleSheet(mFile->styleSheet());
    mExpo->addAction("Export Video...", this, &EditorMode::onExportClicked);
    mExpo->addAction("Export Settings...", this, &EditorMode::onExportSettingsClicked);
    btnExpo->setMenu(mExpo);

    menuLayout->addWidget(btnFile); menuLayout->addWidget(btnEdit); menuLayout->addWidget(btnProj); menuLayout->addWidget(btnExpo);
    headerLayout->addLayout(menuLayout);
    headerLayout->addStretch();
    auto* syncedBadge = new QLabel("● SYNCED", appHeader);
    syncedBadge->setStyleSheet("color: #10b981; background: rgba(16,185,129,0.1); border-radius: 12px; padding: 4px 12px; font-size: 11px; font-weight: bold; border: 1px solid rgba(16,185,129,0.2);");
    headerLayout->addWidget(syncedBadge);
    m_workspaceLayout->addWidget(appHeader);
    
    // MAIN CONTENT STACK
    m_viewStack = new QStackedWidget(rightContainer);
    m_workspaceLayout->addWidget(m_viewStack, 1);
    
    // Create THE Table (Persistent)
    m_syncTable = new QTableWidget(0, 5);
    m_syncTable->setHorizontalHeaderLabels({"#", "START", "END", "CONTENT", "DUR."});
    m_syncTable->setShowGrid(false); m_syncTable->setAlternatingRowColors(true);
    m_syncTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_syncTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_syncTable->verticalHeader()->setVisible(false);
    m_syncTable->verticalHeader()->setDefaultSectionSize(48);
    auto* hh = m_syncTable->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::Fixed); m_syncTable->setColumnWidth(0, 40);
    hh->setSectionResizeMode(1, QHeaderView::Fixed); m_syncTable->setColumnWidth(1, 100);
    hh->setSectionResizeMode(2, QHeaderView::Fixed); m_syncTable->setColumnWidth(2, 100);
    hh->setSectionResizeMode(3, QHeaderView::Stretch);
    hh->setSectionResizeMode(4, QHeaderView::Fixed); m_syncTable->setColumnWidth(4, 80);
    m_syncTable->setStyleSheet("QTableWidget { background-color: #0f172a; border: 1px solid rgba(255,255,255,0.05); border-radius: 12px; outline: 0; } QTableWidget::item { padding: 4px 12px; border-bottom: 1px solid rgba(255,255,255,0.02); color: #e2e8f0; font-size: 14px; } QTableWidget::item:selected { background-color: rgba(59, 130, 246, 0.12); color: #ffffff; border-left: 3px solid #3b82f6; } QHeaderView::section { background-color: #1e293b; color: #94a3b8; font-weight: 800; font-size: 11px; border: none; padding: 10px; }");
    
    auto* gridHeader = new QWidget();
    auto* thLay = new QHBoxLayout(gridHeader); thLay->setContentsMargins(0,0,0,0);
    m_statusLabel = new QLabel("☲ Synchronization Queue", gridHeader);
    m_statusLabel->setStyleSheet("color: white; font-weight: bold; font-size: 16px;");
    thLay->addWidget(m_statusLabel); 
    auto* unsyncBadge = new QLabel("|  Unsynced (4)", gridHeader);
    unsyncBadge->setStyleSheet("color: #94a3b8; font-size: 13px; margin-left: 8px; font-weight: 500;");
    thLay->addWidget(unsyncBadge); thLay->addStretch();

    auto* magicBtn = new QPushButton("✨ AI AUTO-SYNC", gridHeader);
    magicBtn->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #ec4899, stop:1 #8b5cf6); color: white; border-radius: 6px; padding: 8px 16px; font-weight: bold; border: none; font-size: 11px; margin-right: 8px;");
    connect(magicBtn, &QPushButton::clicked, this, &EditorMode::onAutoWhisperClicked);
    thLay->addWidget(magicBtn);

    m_splitTokensBtn = new QPushButton("🧬 SPLIT TO TOKENS", gridHeader);
    m_splitTokensBtn->setStyleSheet("background: rgba(59,130,246,0.1); color: #3b82f6; border: 1px solid rgba(59,130,246,0.2); border-radius: 6px; padding: 8px 16px; font-weight: bold; font-size: 11px;");
    thLay->addWidget(m_splitTokensBtn);
    m_addSubtitleBtn = new QPushButton("+ ADD LINE", gridHeader);
    m_addSubtitleBtn->setStyleSheet("background: #3b82f6; color: white; border-radius: 6px; padding: 8px 16px; font-weight: bold; border: none; font-size: 11px;");
    thLay->addWidget(m_addSubtitleBtn);

    auto* gridContainer = new QWidget();
    auto* gcl = new QVBoxLayout(gridContainer); gcl->setContentsMargins(24,24,24,24); gcl->setSpacing(20);
    gcl->addWidget(gridHeader);
    gcl->addWidget(m_syncTable, 1);

    auto* syncDock = new QDockWidget("Synchronization Queue", rightContainer);
    syncDock->setWidget(gridContainer);
    syncDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    syncDock->setObjectName("syncDock");
    rightContainer->addDockWidget(Qt::BottomDockWidgetArea, syncDock);

    // - VIEW 0: LYRICS EDITOR
    m_lyricsView = new QWidget(m_viewStack);
    auto* lyrl = new QVBoxLayout(m_lyricsView); lyrl->setContentsMargins(0,0,0,0); lyrl->setSpacing(0);
    auto* lyrSubH = new QWidget(m_lyricsView);
    lyrSubH->setFixedHeight(50); lyrSubH->setStyleSheet("background: rgba(0,0,0,0.1); border-bottom: 1px solid rgba(255,255,255,0.03);");
    auto* lshl = new QHBoxLayout(lyrSubH);
    auto makeTabBtn = [&](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setCheckable(true); btn->setAutoExclusive(true);
        btn->setStyleSheet("QPushButton { padding: 8px 16px; font-size: 12px; font-weight: 600; color: #94a3b8; background: transparent; border: none; border-bottom: 2px solid transparent; border-radius: 0px; } QPushButton:hover { color: #e2e8f0; } QPushButton:checked { color: #3b82f6; border-bottom: 2px solid #3b82f6; }");
        return btn;
    };
    m_lyrSourceBtn = makeTabBtn("Source Lyrics"); m_lyrHistoryBtn = makeTabBtn("History");
    m_lyrSourceBtn->setChecked(true);
    lshl->addWidget(m_lyrSourceBtn); lshl->addWidget(m_lyrHistoryBtn); lshl->addStretch();
    lyrl->addWidget(lyrSubH);
    m_lyricsSubStack = new QStackedWidget(m_lyricsView);
    auto* sourcePage = new QWidget();
    auto* spL = new QVBoxLayout(sourcePage); spL->setContentsMargins(40,40,40,40);

    auto* aiToolsL = new QHBoxLayout();
    
    auto* geminiApiInput = new QLineEdit(sourcePage);
    geminiApiInput->setPlaceholderText("Gemini API Key (or set GEMINI_API_KEY env var)...");
    geminiApiInput->setEchoMode(QLineEdit::Password);
    geminiApiInput->setText(QString::fromLocal8Bit(qgetenv("GEMINI_API_KEY")));
    geminiApiInput->setStyleSheet("background: rgba(255,255,255,0.05); color: white; border-radius: 6px; padding: 10px; font-size: 13px; border: 1px solid rgba(255,255,255,0.2);");

    auto* geminiBtn = new QPushButton("✨ Transcribe with Gemini", sourcePage);
    geminiBtn->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1a73e8, stop:1 #4285f4); color: white; border-radius: 6px; padding: 10px 20px; font-weight: bold; font-size: 13px;");
    geminiBtn->setCursor(Qt::PointingHandCursor);
    connect(geminiBtn, &QPushButton::clicked, this, [this, geminiApiInput]() {
        if (!m_project || (!m_project->audio_file.has_value() && !m_project->instrumental_file.has_value())) {
            QMessageBox::warning(this, "No Audio", "Please load an audio file first.");
            return;
        }

        QString apiKey = geminiApiInput->text().trimmed();
        if (apiKey.isEmpty()) apiKey = QString::fromLocal8Bit(qgetenv("GEMINI_API_KEY"));
        if (apiKey.isEmpty()) {
            QMessageBox::warning(this, "No API Key",
                "Please enter your Gemini API key.\n\nGet one at: https://aistudio.google.com/app/apikey\n"
                "Or set the GEMINI_API_KEY environment variable.");
            return;
        }

        QString sourceAudio;
        if (m_project->audio_file.has_value() && !m_project->audio_file.value().empty()) {
            sourceAudio = QString::fromStdString(m_project->audio_file.value().string());
        } else {
            sourceAudio = QString::fromStdString(m_project->instrumental_file.value().string());
        }

        // Step 1: Compress audio to MP3 temp file for Gemini upload
        QString outPath = QDir::tempPath() + "/gemini_upload.mp3";
        auto* overlay = new ProcessingOverlay(this);
        overlay->msg->setText("Compressing audio for Gemini upload...");
        overlay->show();
        auto* exporter = new ExportWorker(this);

        connect(exporter, &ExportWorker::progress, this, [overlay](int p, const QString& m){
            overlay->bar->setValue(p); overlay->msg->setText(m);
        });
        connect(exporter, &ExportWorker::exportComplete, this, [this, overlay, exporter, outPath, apiKey](const QString&) {
            exporter->deleteLater();
            overlay->msg->setText("Sending to Gemini API...");
            overlay->bar->setRange(0, 0); // Indeterminate

            // Step 2: Call python_bridge gemini
            auto* proc = new QProcess(this);
            QString appDir = QCoreApplication::applicationDirPath();
            QString pythonPath;
            for (const auto& p : QStringList{QDir::cleanPath(appDir+"/python_embed/python.exe"),
                                              QDir::cleanPath(QDir::currentPath()+"/python_embed/python.exe"),
                                              "D:/Program Files/Python/python.exe", "python"}) {
                if (p == "python" || QFile::exists(p)) { pythonPath = p; break; }
            }
            QString bridgePath = QDir::cleanPath(appDir + "/python_bridge.py");
            if (!QFile::exists(bridgePath)) bridgePath = QDir::current().filePath("python_bridge.py");

            QStringList args = { bridgePath, "gemini", outPath, "--api-key", apiKey };
            QString* accumulated = new QString();
            connect(proc, &QProcess::readyReadStandardOutput, this, [proc, accumulated]() {
                accumulated->append(QString::fromUtf8(proc->readAllStandardOutput()));
            });
            connect(proc, &QProcess::readyReadStandardError, this, [overlay, proc]() {
                QString l = QString::fromUtf8(proc->readAllStandardError()).trimmed();
                if (!l.isEmpty()) overlay->msg->setText(l.split('\n').last());
            });
            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, overlay, proc, accumulated, outPath](int exitCode, QProcess::ExitStatus) {
                overlay->deleteLater();
                if (exitCode == 0 && !accumulated->isEmpty()) {
                    m_project->lyrics.importFromWhisperJson(accumulated->toStdString());
                    updateSubtitleList();
                    emit unsavedChangesChanged(true);
                    QMessageBox::information(this, "Gemini Done",
                        QString("Imported %1 lines successfully.").arg(m_project->lyrics.lines.size()));
                } else {
                    QMessageBox::critical(this, "Gemini Error",
                        accumulated->isEmpty() ? "Process failed (exit " + QString::number(exitCode) + ")" : *accumulated);
                }
                delete accumulated; proc->deleteLater(); QFile::remove(outPath);
            });
            connect(proc, &QProcess::errorOccurred, this, [this, overlay, proc, accumulated](QProcess::ProcessError) {
                overlay->deleteLater();
                QMessageBox::critical(this, "Process Error", "Failed to launch python_bridge: " + proc->errorString());
                delete accumulated; proc->deleteLater();
            });
            proc->start(pythonPath, args);
        });
        connect(exporter, &ExportWorker::error, this, [overlay, exporter](const QString& err) {
            overlay->deleteLater(); exporter->deleteLater();
            QMessageBox::critical(nullptr, "Audio Compression Failed", err);
        });
        exporter->startExport("", sourceAudio, outPath, "mp3", false);
    });
    
    auto* pasteBtn = new QPushButton("📋 Paste & Sync", sourcePage);
    pasteBtn->setStyleSheet("background: rgba(255,255,255,0.1); color: white; border-radius: 6px; padding: 10px 20px; font-weight: bold; font-size: 13px; border: 1px solid rgba(255,255,255,0.2);");
    pasteBtn->setCursor(Qt::PointingHandCursor);
    connect(pasteBtn, &QPushButton::clicked, this, [this]() {
        QString text = QApplication::clipboard()->text();
        if (text.isEmpty()) {
            QMessageBox::warning(this, "Empty Clipboard", "Clipboard is empty.");
            return;
        }
        if (m_project) {
            ncktv::LyricsData parsed = SubtitleParser::parsePlainText(text);
            m_project->lyrics.clear();
            for (const auto& line : parsed.lines) {
                std::vector<core::LyricsToken> coreTokens;
                for (const auto& w : line.words) {
                    coreTokens.emplace_back(w.word.toStdString(), static_cast<float>(w.startTime), static_cast<float>(w.endTime));
                }
                m_project->lyrics.add_line(line.text.toStdString(), static_cast<float>(line.startTime), static_cast<float>(line.endTime), coreTokens);
            }
            m_sourceLyricsEdit->setPlainText(text);
            updateSubtitleList();
            QMessageBox::information(this, "Import Complete", "AI transcription imported successfully!");
        }
    });

    aiToolsL->addWidget(geminiApiInput, 1);
    aiToolsL->addWidget(geminiBtn);
    aiToolsL->addWidget(pasteBtn);
    spL->addLayout(aiToolsL);

    m_sourceLyricsEdit = new QPlainTextEdit(sourcePage);
    m_sourceLyricsEdit->setReadOnly(false);
    m_sourceLyricsEdit->setPlaceholderText("No lyrics available... Paste AI transcription here or click Paste & Sync.");
    m_sourceLyricsEdit->setStyleSheet("QPlainTextEdit { background: transparent; border: none; color: white; font-size: 28px; font-weight: bold; line-height: 1.6; }");
    spL->addWidget(m_sourceLyricsEdit);
    m_lyricsSubStack->addWidget(sourcePage);
    auto* histPage = new QWidget(); (new QVBoxLayout(histPage))->addWidget(new QLabel("No history available.")); m_lyricsSubStack->addWidget(histPage);
    lyrl->addWidget(m_lyricsSubStack, 1);
    m_viewStack->addWidget(m_lyricsView);
    
    // - VIEW 1: TIMING SYNC
    m_timingView = new QWidget(m_viewStack);
    auto* timingL = new QVBoxLayout(m_timingView); timingL->setContentsMargins(0,0,0,0); timingL->setSpacing(0);
    auto* stageArea = new QWidget(m_timingView);
    auto* stageL = new QHBoxLayout(stageArea); stageL->setContentsMargins(24,24,24,24); stageL->setSpacing(24);
    m_previewWidget = new KaraokePreview(stageArea);
    m_previewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewWidget->setStyleSheet("background: #000;");
    stageL->addWidget(m_previewWidget, 3);
    auto* propPanel = new QWidget(); 
    propPanel->setFixedWidth(300); 
    propPanel->setStyleSheet("background: #1e293b; border-radius: 12px; border: 1px solid rgba(255,255,255,0.05);");
    auto* ppl = new QVBoxLayout(propPanel);
    auto* ppTitle = new QLabel("PROPERTIES", propPanel); 
    ppTitle->setStyleSheet("font-size: 10px; font-weight: 800; color: #94a3b8; letter-spacing: 1.5px; margin-bottom: 20px;");
    ppl->addWidget(ppTitle); ppl->addStretch();
    
    auto* propDock = new QDockWidget("Properties", rightContainer);
    propDock->setWidget(propPanel);
    propDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    propDock->setObjectName("propDock");
    rightContainer->addDockWidget(Qt::RightDockWidgetArea, propDock);

    timingL->addWidget(stageArea, 3);
    m_viewStack->addWidget(m_timingView);

    // - VIEW 2: VIDEO RENDER
    m_renderView = new QWidget(m_viewStack);
    auto* rvL = new QVBoxLayout(m_renderView); rvL->setContentsMargins(40,40,40,40); rvL->setSpacing(30);
    rvL->setAlignment(Qt::AlignTop);

    auto* rvTitle = new QLabel("Finalize & Export Video", m_renderView);
    rvTitle->setStyleSheet("font-size: 24px; font-weight: bold; color: white;");
    rvL->addWidget(rvTitle);

    auto* settingsBox = new QWidget(m_renderView);
    settingsBox->setStyleSheet("background: #1e293b; border-radius: 12px; border: 1px solid rgba(255,255,255,0.05);");
    auto* sbl = new QGridLayout(settingsBox); sbl->setContentsMargins(30,30,30,30); sbl->setSpacing(20);

    auto addSettingRow = [&](int row, QString label, QWidget* widget) {
        auto* lbl = new QLabel(label, settingsBox);
        lbl->setStyleSheet("color: #94a3b8; font-weight: 800; font-size: 11px; letter-spacing: 1px;");
        sbl->addWidget(lbl, row, 0);
        sbl->addWidget(widget, row, 1);
    };

    auto* fmtCombo = new QComboBox(settingsBox); fmtCombo->addItems({"MP4 (H.264 / AAC)", "MKV (H.265 / AAC)", "WebM (VP9 / Opus)"});
    addSettingRow(0, "EXPORT FORMAT", fmtCombo);

    auto* resCombo = new QComboBox(settingsBox); resCombo->addItems({"1920x1080 (Full HD)", "1280x720 (HD)", "3840x2160 (4K Ultra)"});
    addSettingRow(1, "RESOLUTION", resCombo);

    auto* trackCombo = new QComboBox(settingsBox); trackCombo->addItems({"Original Mix", "Instrumental (Vocal Removed)", "Vocals Only"});
    addSettingRow(2, "AUDIO SOURCE", trackCombo);

    auto* burnCheck = new QCheckBox("Burn Subtitles (High Quality ASS)", settingsBox);
    burnCheck->setChecked(true);
    burnCheck->setStyleSheet("QCheckBox { color: white; font-weight: 600; }");
    sbl->addWidget(burnCheck, 3, 1);

    rvL->addWidget(settingsBox);

    auto* exportBtn = new QPushButton("🚀 START EXPORT & RENDER", m_renderView);
    exportBtn->setFixedHeight(60);
    exportBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #8b5cf6, stop:1 #6d28d9); color: white; border-radius: 12px; font-size: 16px; font-weight: bold; border: none; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #a78bfa, stop:1 #7c3aed); }"
    );
    rvL->addWidget(exportBtn);
    
    // Connect the big button
    connect(exportBtn, &QPushButton::clicked, this, [this, fmtCombo, resCombo, trackCombo, burnCheck]() {
        // Collect settings
        QString fmt = fmtCombo->currentText();
        QString res = resCombo->currentText();
        QString track = trackCombo->currentText();
        bool burn = burnCheck->isChecked();

        // Trigger export logic
        // We'll use a file dialog to ask where to save
        QString filter = "Video Files (*.mp4)";
        if (fmt.contains("MKV")) filter = "Video Files (*.mkv)";
        else if (fmt.contains("WebM")) filter = "Video Files (*.webm)";

        QString defaultName = m_project ? QString::fromStdString(m_project->project_name) + "_render" : "output";
        QString outPath = QFileDialog::getSaveFileName(this, "Save Rendered Video", defaultName, filter);

        if (outPath.isEmpty()) return;

        if (burn) {
            AssStyle style; 
            QString assContent = AssGenerator::generate(m_project->lyrics, style);
            QString tempAssPath = QFileInfo(outPath).absolutePath() + "/temp_subs.ass";
            QFile assFile(tempAssPath);
            if (assFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                assFile.write(assContent.toUtf8());
                assFile.close();
            }
        }

        auto* worker = new ExportWorker(this);
        auto* overlay = new ProcessingOverlay(this);
        overlay->msg->setText("Initializing Renderer...");
        overlay->show();

        connect(worker, &ExportWorker::exportComplete, this, [overlay, outPath](const QString&){
            overlay->deleteLater();
            QMessageBox::information(nullptr, "Render Complete", "Video rendered successfully:\n" + outPath);
            QFile::remove(QFileInfo(outPath).absolutePath() + "/temp_subs.ass");
        });
        connect(worker, &ExportWorker::error, this, [overlay](const QString& e){
            overlay->deleteLater();
            QMessageBox::critical(nullptr, "Render Failed", e);
        });
        connect(worker, &ExportWorker::progress, this, [overlay](int p, const QString& m){
            overlay->bar->setValue(p); overlay->msg->setText(m);
        });

        QString vSrc = m_project->source_file.has_value() ? QString::fromStdString(m_project->source_file->string()) : "";
        QString aSrc = vSrc;
        if (track.contains("Instrumental") && m_project->instrumental_file.has_value()) aSrc = QString::fromStdString(m_project->instrumental_file->string());
        else if (track.contains("Vocals Only") && m_project->vocals_file.has_value()) aSrc = QString::fromStdString(m_project->vocals_file->string());
        
        int width = 1920, height = 1080;
        if (res.contains("3840")) { width = 3840; height = 2160; }
        else if (res.contains("1280")) { width = 1280; height = 720; }
        
        worker->startExport(vSrc, aSrc, outPath, fmt, burn, width, height);
    });

    rvL->addStretch();
    m_viewStack->addWidget(m_renderView);

    // Show/hide docks dynamically to keep them "live" in appropriate views
    // NOTE: Sync Grid tab removed — sync queue dock is only shown in Timing Sync view
    auto updateDockVisibility = [syncDock, propDock](int mainIdx, int /*subIdx*/ = -1) {
        if (mainIdx == 1) { // Timing Sync
            syncDock->show();
            propDock->show();
        } else {
            syncDock->hide();
            propDock->hide();
        }
    };
    
    // TRANSPORT BAR
    m_transportBar = new QWidget(rightContainer);
    m_transportBar->setFixedHeight(80);
    m_transportBar->setStyleSheet("background: #0b101b; border-top: 1px solid rgba(255,255,255,0.05);");
    auto* tLay = new QHBoxLayout(m_transportBar); tLay->setContentsMargins(30, 8, 30, 8); tLay->setSpacing(24);
    
    auto* playBtn = new QPushButton("▶", m_transportBar);
    playBtn->setFixedSize(48, 48);
    playBtn->setStyleSheet("background: white; color: black; border-radius: 24px; font-size: 18px; border: none; font-family: 'Segoe UI Emoji', sans-serif;");
    tLay->addWidget(playBtn);

    m_setStartBtn = new QPushButton("[\uf2bd Start", m_transportBar);
    m_setStartBtn->setToolTip("Set start time of selected line to current playback position");
    m_setStartBtn->setStyleSheet("QPushButton { background: rgba(59, 130, 246, 0.2); color: #93c5fd; border-radius: 6px; padding: 6px 12px; font-weight: bold; border: 1px solid rgba(59, 130, 246, 0.5); } QPushButton:hover { background: rgba(59, 130, 246, 0.4); }");
    tLay->addWidget(m_setStartBtn);

    m_setEndBtn = new QPushButton("]\uf2bd End", m_transportBar);
    m_setEndBtn->setToolTip("Set end time of selected line to current playback position");
    m_setEndBtn->setStyleSheet("QPushButton { background: rgba(239, 68, 68, 0.2); color: #fca5a5; border-radius: 6px; padding: 6px 12px; font-weight: bold; border: 1px solid rgba(239, 68, 68, 0.5); } QPushButton:hover { background: rgba(239, 68, 68, 0.4); }");
    tLay->addWidget(m_setEndBtn);

    connect(m_setStartBtn, &QPushButton::clicked, this, [this]() {
        if (!m_project || m_syncTable->selectedItems().isEmpty()) return;
        int row = m_syncTable->selectedItems().first()->row();
        if (row < 0 || row >= static_cast<int>(m_project->lyrics.lines.size())) return;

        double currTime = m_audioPlayer->player()->position() / 1000.0;
        m_project->lyrics.lines[row].start_time = currTime;
        m_syncTable->item(row, 1)->setText(formatTimeMMSS(currTime));
        double dur = m_project->lyrics.lines[row].end_time - currTime;
        m_syncTable->item(row, 4)->setText(QString("%1s").arg(dur, 0, 'f', 1));
        emit unsavedChangesChanged(true);
    });

    connect(m_setEndBtn, &QPushButton::clicked, this, [this]() {
        if (!m_project || m_syncTable->selectedItems().isEmpty()) return;
        int row = m_syncTable->selectedItems().first()->row();
        if (row < 0 || row >= static_cast<int>(m_project->lyrics.lines.size())) return;

        double currTime = m_audioPlayer->player()->position() / 1000.0;
        m_project->lyrics.lines[row].end_time = currTime;
        m_syncTable->item(row, 2)->setText(formatTimeMMSS(currTime));
        double dur = currTime - m_project->lyrics.lines[row].start_time;
        m_syncTable->item(row, 4)->setText(QString("%1s").arg(dur, 0, 'f', 1));
        emit unsavedChangesChanged(true);
    });

    // Track Selector in Transport Bar
    m_trackSelector = new QComboBox(m_transportBar);
    m_trackSelector->addItems({"Original Mix", "Instrumental", "Vocals Only"});
    m_trackSelector->setFixedWidth(140);
    m_trackSelector->setStyleSheet("QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; padding: 6px 10px; color: #94a3b8; font-weight: 600; font-size: 11px; } QComboBox::drop-down { border: none; }");
    tLay->addWidget(m_trackSelector);

    m_waveformWidget = new WaveformWidget(m_transportBar);
    m_waveformWidget->setFixedHeight(60);
    tLay->addWidget(m_waveformWidget, 1);
    
    m_workspaceLayout->addWidget(m_transportBar);
    m_mainHLayout->addWidget(rightContainer, 1);
    
    // Legacy / Hidden
    m_toolBar = new QToolBar(this); m_toolBar->hide();
    m_langCombo = new QComboBox(this); m_langCombo->setCurrentText("Auto"); m_langCombo->hide();
    m_whisperBtn = new QPushButton(this); m_whisperBtn->hide();
    m_precisionBtn = new QPushButton(this); m_precisionBtn->hide();
    m_importBtn = new QPushButton(this); m_importBtn->hide();
    m_subtitleInput = new QLineEdit(this); m_subtitleInput->hide();
    m_timelineWidget = new TimelineWidget(this); m_timelineWidget->hide();
    m_audioPlayer = new AudioPlayer(this); m_audioPlayer->hide();
    // m_trackSelector is now visible in transport bar

    // Setup basic connections for new UI
    connect(btnFile, &QPushButton::clicked, this, [this]{ emit requestOpen(); });
    connect(btnEdit, &QPushButton::clicked, this, [this]{ emit requestPreferences(); });
    connect(btnProj, &QPushButton::clicked, this, [this]{ emit requestSave(); });
    connect(btnExpo, &QPushButton::clicked, this, [this]{ emit requestExport(); });
    
    connect(m_consoleBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath() + "/debug.log"));
    });
    
    connect(m_modeLyricsBtn, &QPushButton::clicked, this, [this, updateDockVisibility]{
        m_viewStack->setCurrentIndex(0);
        m_modeTimingBtn->setChecked(false);
        m_modeRenderBtn->setChecked(false);
        updateDockVisibility(0, m_lyricsSubStack->currentIndex());
    });
    connect(m_modeTimingBtn, &QPushButton::clicked, this, [this, updateDockVisibility]{
        m_viewStack->setCurrentIndex(1);
        m_modeLyricsBtn->setChecked(false);
        m_modeRenderBtn->setChecked(false);
        updateDockVisibility(1);
    });
    connect(m_modeRenderBtn, &QPushButton::clicked, this, [this, updateDockVisibility]{
        m_viewStack->setCurrentIndex(2);
        m_modeLyricsBtn->setChecked(false);
        m_modeTimingBtn->setChecked(false);
        updateDockVisibility(2);
    });
    
    // Connect track selector
    connect(m_trackSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditorMode::onTrackSelectionChanged);

    connect(m_lyrSourceBtn, &QPushButton::clicked, this, [this, updateDockVisibility]{ m_lyricsSubStack->setCurrentIndex(0); updateDockVisibility(0, 0); });
    connect(m_lyrHistoryBtn, &QPushButton::clicked, this, [this, updateDockVisibility]{ m_lyricsSubStack->setCurrentIndex(1); updateDockVisibility(0, 1); });

    // ── Source Lyrics inline editing: write changes back to project ──────────
    connect(m_sourceLyricsEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_syncingFromEdit || !m_project) return;
        QString fullText = m_sourceLyricsEdit->toPlainText();
        QStringList newLines = fullText.split('\n', Qt::KeepEmptyParts);
        // Resize the lyrics lines if count differs
        while ((int)m_project->lyrics.lines.size() < newLines.size()) {
            core::LyricsLine ln;
            ln.start_time = 0.0f; ln.end_time = 0.0f;
            m_project->lyrics.lines.push_back(ln);
        }
        // Update text in place; preserve timing data
        for (int i = 0; i < newLines.size(); ++i) {
            m_project->lyrics.lines[i].text = newLines[i].trimmed().toStdString();
            m_project->lyrics.lines[i].splitIntoWords();
        }
        // Remove extra lines that were deleted
        if ((int)m_project->lyrics.lines.size() > newLines.size()) {
            m_project->lyrics.lines.resize(newLines.size());
        }
        emit unsavedChangesChanged(true);
        // Refresh sync table without recursing into updateSubtitleList's setText
        m_syncingFromEdit = true;
        m_syncTable->blockSignals(true);
        m_syncTable->setRowCount(0);
        const auto& lines = m_project->lyrics.lines;
        for (int i = 0; i < (int)lines.size(); ++i) {
            const auto& line = lines[i]; m_syncTable->insertRow(i);
            auto* it0 = new QTableWidgetItem(QString::number(i + 1)); it0->setFlags(it0->flags() & ~Qt::ItemIsEditable); m_syncTable->setItem(i, 0, it0);
            auto* it1 = new QTableWidgetItem(formatTimeMMSS(line.start_time)); it1->setData(Qt::UserRole, line.start_time); it1->setData(Qt::UserRole+1, i); m_syncTable->setItem(i, 1, it1);
            auto* it2 = new QTableWidgetItem(formatTimeMMSS(line.end_time)); it2->setData(Qt::UserRole+1, i); m_syncTable->setItem(i, 2, it2);
            auto* it3 = new QTableWidgetItem(QString::fromStdString(line.text)); it3->setData(Qt::UserRole+1, i); m_syncTable->setItem(i, 3, it3);
            double dur = line.end_time - line.start_time;
            auto* it4 = new QTableWidgetItem(QString("%1s").arg(dur, 0, 'f', 1)); it4->setFlags(it4->flags() & ~Qt::ItemIsEditable); m_syncTable->setItem(i, 4, it4);
        }
        m_syncTable->blockSignals(false);
        m_syncingFromEdit = false;
    });
    
    connect(playBtn, &QPushButton::clicked, this, [this]{
        if (m_audioPlayer->player()->playbackState() == QMediaPlayer::PlayingState) {
            m_audioPlayer->player()->pause();
            if (m_videoPlayer) m_videoPlayer->pause();
        } else {
            m_audioPlayer->player()->play();
            if (m_videoPlayer) m_videoPlayer->play();
        }
    });

    // Update play button state automatically
    connect(m_audioPlayer->player(), &QMediaPlayer::playbackStateChanged, this, [playBtn](QMediaPlayer::PlaybackState state){
        playBtn->setText(state == QMediaPlayer::PlayingState ? "⏸" : "▶");
    });

    // Initial visibility state
    updateDockVisibility(0, 0);
}

void EditorMode::setupToolBar() {
    m_toolBar->addAction("Save", this, [this]() { emit requestSave(); });
    m_toolBar->addSeparator();
    m_toolBar->addAction("Undo", m_undoManager, &UndoManager::undo);
    m_toolBar->addAction("Redo", m_undoManager, &UndoManager::redo);
    m_toolBar->addSeparator();
    m_toolBar->addSeparator();
    m_toolBar->addAction("Export...", this, &EditorMode::onExportClicked);
    m_toolBar->addAction("Settings", this, &EditorMode::onExportSettingsClicked);
}

void EditorMode::setupConnections() {
    connect(m_audioPlayer, &AudioPlayer::positionChanged, this, &EditorMode::onTimecodeChanged);
    connect(m_audioPlayer, &AudioPlayer::durationChanged, this, [this](double duration) { m_waveformWidget->setDuration(duration); });
    connect(m_waveformWidget, &WaveformWidget::seekRequested, m_audioPlayer, &AudioPlayer::seek);
    connect(m_timelineWidget, &TimelineWidget::seekRequested, m_audioPlayer, &AudioPlayer::seek);
    connect(m_syncTable, &QTableWidget::itemClicked, this, [this](QTableWidgetItem* item) {
        if (item->column() <= 1) {
            int row = item->row(); auto* startItem = m_syncTable->item(row, 1); if (startItem) { double t = startItem->data(Qt::UserRole).toDouble(); m_audioPlayer->seek(t); }
        }
    });
    connect(m_syncTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!m_project) return;
        int row = item->row(); int idx = item->data(Qt::UserRole + 1).toInt();
        if (idx < 0 || idx >= (int)m_project->lyrics.lines.size()) return;
        bool modified = false; QString text = item->text().trimmed();
        if (item->column() == 3) { if (!text.isEmpty()) { m_project->lyrics.lines[idx].text = text.toStdString(); m_project->lyrics.lines[idx].splitIntoWords(); modified = true; } }
        else if (item->column() == 1 || item->column() == 2) {
            double val = 0.0; if (text.contains(":")) { auto parts = text.split(":"); if (parts.size() >= 2) val = parts[0].toInt() * 60.0 + parts[1].toDouble(); } else val = text.toDouble();
            if (item->column() == 1) m_project->lyrics.lines[idx].start_time = val; else m_project->lyrics.lines[idx].end_time = val;
            modified = true;
        }
        if (modified) { emit unsavedChangesChanged(true); QTimer::singleShot(0, this, [this, row]() { updateSubtitleList(); m_syncTable->selectRow(row); }); }
    });
    connect(m_addSubtitleBtn, &QPushButton::clicked, this, &EditorMode::onAddSubtitleClicked);
    connect(m_splitTokensBtn, &QPushButton::clicked, this, [this]() {
        int row = m_syncTable->currentRow(); if (row < 0 || !m_project) { QMessageBox::information(this, "Selection Required", "Please select a line to split."); return; }
        int idx = m_syncTable->item(row, 3)->data(Qt::UserRole + 1).toInt(); m_project->lyrics.lines[idx].splitIntoWords(); updateSubtitleList();
    });
    m_syncTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_syncTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = m_syncTable->itemAt(pos); if (!item) return;
        int row = item->row(); QMenu menu(this);
        menu.addAction("⏱  Precision Timing...", this, [this, row]() {
            int idx = m_syncTable->item(row, 3)->data(Qt::UserRole + 1).toInt(); WordEditor editor(&m_project->lyrics.lines[idx], this); if (editor.exec() == QDialog::Accepted && editor.wasModified()) { updateSubtitleList(); emit unsavedChangesChanged(true); }
        });
        // Set Start / Set End — scrub waveform to current position and stamp it
        menu.addAction("[  Set START to Playhead", this, [this, row]() {
            int idx = m_syncTable->item(row, 3)->data(Qt::UserRole + 1).toInt();
            if (idx < 0 || idx >= (int)m_project->lyrics.lines.size()) return;
            m_project->lyrics.lines[idx].start_time = static_cast<float>(m_currentTime);
            updateSubtitleList(); emit unsavedChangesChanged(true);
            QMessageBox::information(this, "Start Set", QString("Line %1 START set to %2s").arg(idx+1).arg(m_currentTime, 0, 'f', 3));
        });
        menu.addAction("]  Set END to Playhead", this, [this, row]() {
            int idx = m_syncTable->item(row, 3)->data(Qt::UserRole + 1).toInt();
            if (idx < 0 || idx >= (int)m_project->lyrics.lines.size()) return;
            m_project->lyrics.lines[idx].end_time = static_cast<float>(m_currentTime);
            updateSubtitleList(); emit unsavedChangesChanged(true);
            QMessageBox::information(this, "End Set", QString("Line %1 END set to %2s").arg(idx+1).arg(m_currentTime, 0, 'f', 3));
        });
        menu.addSeparator();
        menu.addAction("✕  Delete", this, [this, row]() {
            int idx = m_syncTable->item(row, 3)->data(Qt::UserRole + 1).toInt(); m_project->lyrics.lines.erase(m_project->lyrics.lines.begin() + idx); updateSubtitleList(); emit unsavedChangesChanged(true);
        });
        menu.exec(m_syncTable->mapToGlobal(pos));
    });
}

void EditorMode::applyTheme() {
    QString qss = R"(
        QWidget { background-color: #0f172a; color: #e2e8f0; font-family: "Segoe UI", sans-serif; font-size: 13px; }
        QPushButton { background-color: #1e293b; border: 1px solid rgba(255,255,255,0.08); border-radius: 6px; padding: 6px 14px; color: #e2e8f0; font-weight: 500; }
        QPushButton:hover { background-color: #334155; border-color: rgba(59, 130, 246, 0.3); }
        QTableWidget { background-color: #0f172a; border: none; alternate-background-color: rgba(255, 255, 255, 0.02); }
        QTableWidget QLineEdit { background: #1e293b; color: white; border: 2px solid #3b82f6; border-radius: 4px; padding: 4px 8px; margin: 0px; selection-background-color: #3b82f6; }
        QHeaderView::section { background-color: #0f172a; color: #94a3b8; font-weight: 800; font-size: 10px; border: none; border-bottom: 1px solid rgba(255,255,255,0.05); padding: 8px; }
    )";
    this->setStyleSheet(qss);
}

void EditorMode::syncViewState() {
    if (!m_project) return;
    if (m_project->source_file.has_value() && !m_project->source_file.value().empty()) {
        if (!m_videoPlayer) {
            m_videoPlayer = new QMediaPlayer(this); m_videoAudioOutput = new QAudioOutput(this); m_videoAudioOutput->setVolume(0.0);
            m_videoPlayer->setAudioOutput(m_videoAudioOutput); m_previewWidget->setMediaPlayer(m_videoPlayer);
        }
        m_videoPlayer->setSource(QUrl::fromLocalFile(QString::fromStdString(m_project->source_file.value().string()))); m_videoPlayer->pause();
    }
    if (m_project->instrumental_file.has_value() && !m_project->instrumental_file.value().empty()) {
        QString path = QString::fromStdString(m_project->instrumental_file.value().string());
        m_audioPlayer->loadSource(path);
        requestWaveform(path);
        if (m_trackSelector) { m_trackSelector->blockSignals(true); m_trackSelector->setCurrentIndex(1); m_trackSelector->blockSignals(false); }
    } else if (m_project->audio_file.has_value() && !m_project->audio_file.value().empty()) {
        QString path = QString::fromStdString(m_project->audio_file.value().string());
        m_audioPlayer->loadSource(path);
        requestWaveform(path);
        if (m_trackSelector) { m_trackSelector->blockSignals(true); m_trackSelector->setCurrentIndex(0); m_trackSelector->blockSignals(false); }
    }
    updateSubtitleList();
}

void EditorMode::onPlayPauseToggled(bool isPlaying) { qDebug() << "Play state changed:" << isPlaying; }
void EditorMode::onTimecodeChanged(double timeSeconds) {
    m_currentTime = timeSeconds; m_previewWidget->updateTime(timeSeconds); m_waveformWidget->updateCursor(timeSeconds); m_timelineWidget->updateCursor(timeSeconds);
    
    const auto& lines = m_project->lyrics.lines;
    int currentLine = -1;
    for (int i = 0; i < (int)lines.size(); ++i) { 
        if (timeSeconds >= lines[i].start_time && timeSeconds <= lines[i].end_time) { 
            currentLine = i; break; 
        } 
    }

    // Update Table Highlight
    if (m_syncTable && m_syncTable->isVisible()) {
        if (currentLine != -1 && m_syncTable->currentRow() != currentLine) { 
            m_syncTable->blockSignals(true); 
            m_syncTable->selectRow(currentLine); 
            m_syncTable->scrollToItem(m_syncTable->item(currentLine, 0)); 
            m_syncTable->blockSignals(false); 
        }
    }

    // Update Source Lyrics View Highlight
    if (m_sourceLyricsEdit && m_sourceLyricsEdit->isVisible()) {
        if (currentLine != -1) {
            QList<QTextEdit::ExtraSelection> selections;
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(QColor(59, 130, 246, 80)); // Highlight color
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            
            QTextBlock block = m_sourceLyricsEdit->document()->findBlockByNumber(currentLine);
            if (block.isValid()) {
                sel.cursor = QTextCursor(block);
                selections.append(sel);
                m_sourceLyricsEdit->setExtraSelections(selections);
                
                // Keep the highlighted line in view
                m_sourceLyricsEdit->setTextCursor(sel.cursor);
                m_sourceLyricsEdit->ensureCursorVisible();
            }
        } else {
            m_sourceLyricsEdit->setExtraSelections({});
        }
    }

    if (m_videoPlayer) {
        qint64 vPos = m_videoPlayer->position();
        qint64 aPos = static_cast<qint64>(timeSeconds * 1000.0);
        bool isPaused = (m_audioPlayer->player()->playbackState() != QMediaPlayer::PlayingState);
        int driftLimit = isPaused ? 300 : 1000;
        if (std::abs(vPos - aPos) > driftLimit) {
            m_videoPlayer->setPosition(aPos);
        }
    }
}
void EditorMode::onLineSelected(int lineIndex) { if (m_syncTable && lineIndex >= 0 && lineIndex < m_syncTable->rowCount()) { m_syncTable->selectRow(lineIndex); m_syncTable->scrollToItem(m_syncTable->item(lineIndex, 0)); } }
void EditorMode::onWordSelected(int, int) {}
void EditorMode::onWordsChanged() { emit unsavedChangesChanged(true); }


void EditorMode::updateSubtitleList() {
    if (!m_project || !m_syncTable) return;
    m_syncTable->blockSignals(true); m_syncTable->setRowCount(0);
    const auto& lines = m_project->lyrics.lines;
    for (int i = 0; i < (int)lines.size(); ++i) {
        const auto& line = lines[i]; m_syncTable->insertRow(i);
        auto* it0 = new QTableWidgetItem(QString::number(i + 1)); it0->setFlags(it0->flags() & ~Qt::ItemIsEditable); m_syncTable->setItem(i, 0, it0);
        auto* it1 = new QTableWidgetItem(formatTimeMMSS(line.start_time)); it1->setData(Qt::UserRole, line.start_time); it1->setData(Qt::UserRole+1, i); m_syncTable->setItem(i, 1, it1);
        auto* it2 = new QTableWidgetItem(formatTimeMMSS(line.end_time)); it2->setData(Qt::UserRole+1, i); m_syncTable->setItem(i, 2, it2);
        auto* it3 = new QTableWidgetItem(QString::fromStdString(line.text)); it3->setData(Qt::UserRole+1, i); m_syncTable->setItem(i, 3, it3);
        double dur = line.end_time - line.start_time;
        auto* it4 = new QTableWidgetItem(QString("%1s").arg(dur, 0, 'f', 1)); it4->setFlags(it4->flags() & ~Qt::ItemIsEditable); m_syncTable->setItem(i, 4, it4);
    }
    m_syncTable->blockSignals(false); 
    
    // Update Source Lyrics View (guard the write so it doesn't trigger inline-edit callback)
    if (m_sourceLyricsEdit) {
        QString fullText;
        for (const auto& line : lines) {
            fullText += QString::fromStdString(line.text) + "\n";
        }
        m_syncingFromEdit = true;
        m_sourceLyricsEdit->setPlainText(fullText.trimmed());
        m_syncingFromEdit = false;
    }

    m_previewWidget->loadLyrics(&m_project->lyrics); 
    m_timelineWidget->loadLyrics(&m_project->lyrics);
}
void EditorMode::onTrackSelectionChanged(int index) {
    if (!m_project) return;
    QString src; 
    if (index == 0) {
        if (m_project->audio_file) src = QString::fromStdString(m_project->audio_file->string());
        else if (m_project->source_file) src = QString::fromStdString(m_project->source_file->string());
    } else if (index == 1 && m_project->instrumental_file) {
        src = QString::fromStdString(m_project->instrumental_file->string());
    } else if (index == 2 && m_project->vocals_file) {
        src = QString::fromStdString(m_project->vocals_file->string());
    }
    
    qDebug() << "Switching track to index" << index << "path:" << src;
    
    if (!src.isEmpty() && QFile::exists(src)) {
        m_audioPlayer->loadSource(src);
        requestWaveform(src);
    } else {
        qDebug() << "Track source NOT FOUND or empty for index" << index << "path:" << src;
    }
}
void EditorMode::onAddSubtitleClicked() {
    if (!m_project) return;
    core::LyricsLine ln; ln.text = "New Lyric Line"; ln.start_time = m_currentTime; ln.end_time = m_currentTime + 2.0; ln.splitIntoWords();
    m_project->lyrics.lines.push_back(std::move(ln)); updateSubtitleList(); emit unsavedChangesChanged(true);
}
void EditorMode::onAutoWhisperClicked() {
    if (!m_project) return;
    QString target = m_project->vocals_file ? QString::fromStdString(m_project->vocals_file->string()) : (m_project->audio_file ? QString::fromStdString(m_project->audio_file->string()) : "");
    if (target.isEmpty()) {
        QMessageBox::warning(this, "No Audio", "Please load an audio file first.");
        return;
    }

    // ── Confirmation dialog: choose model + language ─────────────────────
    auto* confirmDlg = new QDialog(this);
    confirmDlg->setWindowTitle("AI Auto-Sync Settings");
    confirmDlg->setModal(true);
    confirmDlg->setFixedSize(400, 260);
    confirmDlg->setStyleSheet("QDialog { background: #1e293b; color: #e2e8f0; } QLabel { color: #94a3b8; font-size: 12px; } QComboBox { background: #0f172a; border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; padding: 8px; color: white; } QPushButton { background: #3b82f6; color: white; border-radius: 6px; padding: 10px 20px; font-weight: bold; border: none; } QPushButton#cancelBtn { background: rgba(255,255,255,0.05); color: #94a3b8; }");
    auto* dlgLayout = new QVBoxLayout(confirmDlg);
    dlgLayout->setContentsMargins(28, 24, 28, 24); dlgLayout->setSpacing(16);
    auto* dlgTitle = new QLabel("AI Transcription — Whisper", confirmDlg);
    dlgTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: white;");
    dlgLayout->addWidget(dlgTitle);

    auto* modelLabel = new QLabel("Model:", confirmDlg); dlgLayout->addWidget(modelLabel);
    auto* modelCombo = new QComboBox(confirmDlg);
    modelCombo->addItems({"tiny", "base", "small", "medium", "large", "turbo"});
    modelCombo->setCurrentText("medium");
    dlgLayout->addWidget(modelCombo);

    auto* langLabel = new QLabel("Language:", confirmDlg); dlgLayout->addWidget(langLabel);
    auto* langCombo = new QComboBox(confirmDlg);
    langCombo->addItems({"Auto", "en", "ja", "zh", "ko", "ms", "id", "th", "vi", "ar", "es", "fr", "de", "pt", "ru", "hi"});
    langCombo->setCurrentText("Auto");
    dlgLayout->addWidget(langCombo);

    dlgLayout->addStretch();
    auto* btnRow = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("Cancel", confirmDlg); cancelBtn->setObjectName("cancelBtn");
    auto* startBtn  = new QPushButton("Start Transcription", confirmDlg);
    btnRow->addWidget(cancelBtn); btnRow->addWidget(startBtn);
    dlgLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, confirmDlg, &QDialog::reject);
    connect(startBtn, &QPushButton::clicked, confirmDlg, &QDialog::accept);

    if (confirmDlg->exec() != QDialog::Accepted) return;

    QString chosenModel = modelCombo->currentText();
    QString chosenLang  = langCombo->currentText();

    auto* overlay = new ProcessingOverlay(this); overlay->show();
    auto* worker = new TranscriptionWorker(this);
    connect(worker, &TranscriptionWorker::transcriptionComplete, this, [this, overlay](const QString& res){
        overlay->deleteLater();
        m_project->lyrics.importFromWhisperJson(res.toStdString());
        updateSubtitleList();
        emit unsavedChangesChanged(true);
    });
    connect(worker, &TranscriptionWorker::error, this, [this, overlay](const QString& e){
        overlay->deleteLater();
        QMessageBox::warning(this, "Transcription Error", e);
    });
    worker->startTranscription(target, chosenModel, chosenLang);
}
void EditorMode::onImportSubtitleClicked() {
    ImportDialog d(this); 
    if (d.exec() != QDialog::Accepted) return;
    
    QString path = d.filePath();
    if (path.isEmpty()) return;

    if (path.endsWith(".json", Qt::CaseInsensitive)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            QString c = QString::fromUtf8(f.readAll());
            m_project->lyrics.importFromWhisperJson(c.toStdString());
        }
    } else {
        // Use Unified Subtitle Parser for LRC, SRT, ASS, etc.
        ncktv::LyricsData uiLyrics = SubtitleParser::parseFile(path);
        if (!uiLyrics.lines.empty()) {
            m_project->lyrics.clear();
            for (const auto& line : uiLyrics.lines) {
                ncktv::core::LyricsLine coreLine;
                coreLine.text = line.text.toStdString();
                coreLine.start_time = static_cast<float>(line.startTime);
                coreLine.end_time = static_cast<float>(line.endTime);
                
                for (const auto& word : line.words) {
                    ncktv::core::LyricsToken token;
                    token.text = word.word.toStdString();
                    token.start_time = static_cast<float>(word.startTime);
                    token.end_time = static_cast<float>(word.endTime);
                    coreLine.tokens.push_back(std::move(token));
                }
                
                if (coreLine.tokens.empty()) {
                    coreLine.splitIntoWords();
                }
                
                m_project->lyrics.lines.push_back(std::move(coreLine));
            }
        } else {
            QMessageBox::warning(this, "Import Error", "Failed to parse subtitle file or file is empty.");
        }
    }

    updateSubtitleList(); 
    emit unsavedChangesChanged(true);
}
void EditorMode::onExportClicked() {
    if (!m_project) return;
    
    ExportDialog d(this);
    if (d.exec() == QDialog::Accepted) {
        QString outPath = d.outputPath();
        QString format = d.format();
        bool burnSubs = d.burnSubtitles();

        if (burnSubs) {
            // Generate temporary ASS file for FFmpeg
            AssStyle style; 
            QString assContent = AssGenerator::generate(m_project->lyrics, style);
            
            QString tempAssPath = QFileInfo(outPath).absolutePath() + "/temp_subs.ass";
            QFile assFile(tempAssPath);
            if (assFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                assFile.write(assContent.toUtf8());
                assFile.close();
            } else {
                QMessageBox::warning(this, "Export Error", "Could not create temporary subtitle file for burning.");
                return;
            }
        }

        auto* worker = new ExportWorker(this);
        auto* overlay = new ProcessingOverlay(this);
        overlay->msg->setText("Exporting Video...");
        overlay->show();

        connect(worker, &ExportWorker::exportComplete, this, [this, overlay, outPath](const QString& p) {
            overlay->deleteLater();
            QMessageBox::information(this, "Export Complete", "Project exported successfully to:\n" + outPath);
            // Cleanup
            QFile::remove(QFileInfo(outPath).absolutePath() + "/temp_subs.ass");
        });

        connect(worker, &ExportWorker::error, this, [this, overlay](const QString& e) {
            overlay->deleteLater();
            QMessageBox::critical(this, "Export Failed", "Error during export:\n" + e);
        });

        connect(worker, &ExportWorker::progress, this, [overlay](int p, const QString& m) {
            overlay->bar->setValue(p);
            overlay->msg->setText(m);
        });

        QString videoSrc = m_project->source_file.has_value() ? QString::fromStdString(m_project->source_file->string()) : "";
        QString audioSrc = videoSrc; // Default
        
        // Handle track choice if the user selected something else
        if (d.audioSource() == "Instrumental" && m_project->instrumental_file.has_value()) {
            audioSrc = QString::fromStdString(m_project->instrumental_file->string());
        } else if (d.audioSource() == "Vocals Only" && m_project->vocals_file.has_value()) {
            audioSrc = QString::fromStdString(m_project->vocals_file->string());
        }

        worker->startExport(videoSrc, audioSrc, outPath, format, burnSubs, d.videoWidth(), d.videoHeight());
    }
}
void EditorMode::onExportSettingsClicked() { if (m_config) { PreferencesDialog d(m_config, this); d.exec(); } }

void EditorMode::onWaveformReady(const QVector<float>& minData, const QVector<float>& maxData, double sampleRate, int samplesPerPixel) {
    if (m_waveformWidget) {
        m_waveformWidget->loadWaveformData(minData, maxData, sampleRate, samplesPerPixel);
    }
}

void EditorMode::requestWaveform(const QString& path) {
    auto* thread = new QThread(this);
    auto* worker = new WaveformWorker();
    worker->moveToThread(thread);
    
    connect(thread, &QThread::started, worker, [worker, path]{ worker->generate(path); });
    connect(worker, &WaveformWorker::waveformReady, this, &EditorMode::onWaveformReady);
    connect(worker, &WaveformWorker::waveformReady, thread, &QThread::quit);
    connect(worker, &WaveformWorker::error, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    
    thread->start();
}
} // namespace ncktv
