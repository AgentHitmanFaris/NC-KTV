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
#include <QFrame>
#include <QScrollArea>
#include <QButtonGroup>
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

static QString formatTimecode(double seconds, int fps = 30) {
    int totalFrames = static_cast<int>(seconds * fps);
    int ff = totalFrames % fps;
    int ss = (totalFrames / fps) % 60;
    int mm = (totalFrames / fps / 60) % 60;
    int hh = totalFrames / fps / 3600;
    return QString("%1:%2:%3:%4")
        .arg(hh, 2, 10, QChar('0'))
        .arg(mm, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));
}

static QWidget* makePanelHeader(QWidget* parent, const QString& title, QList<QWidget*> rightActions = {}) {
    auto* header = new QWidget(parent);
    header->setObjectName("panelHeader");
    header->setFixedHeight(28);

    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(10, 0, 6, 0);
    layout->setSpacing(0);

    auto* titleLabel = new QLabel(title.toUpper(), header);
    titleLabel->setObjectName("panelTitle");
    layout->addWidget(titleLabel);

    layout->addStretch();

    for (QWidget* action : rightActions) {
        layout->addWidget(action);
    }

    return header;
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
        popup->setObjectName("progressCard");
        popup->setFixedSize(450, 200);
        auto* popupLayout = new QVBoxLayout(popup);
        popupLayout->setAlignment(Qt::AlignCenter);
        popupLayout->setContentsMargins(30, 25, 30, 25);
        popupLayout->setSpacing(15);
        iconLbl = new QLabel("✨", popup);
        iconLbl->setAlignment(Qt::AlignCenter);
        popupLayout->addWidget(iconLbl);
        msg = new QLabel("Initializing AI transcription...", popup);
        msg->setAlignment(Qt::AlignCenter);
        popupLayout->addWidget(msg);
        bar = new QProgressBar(popup);
        bar->setObjectName("processingBar");
        bar->setRange(0, 0); 
        bar->setFixedHeight(10);
        bar->setTextVisible(false);
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
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // ── ROW 0: TAB BAR (36px) ───────────────────────────────────────────────
    auto* tabBar = new QWidget(this);
    tabBar->setObjectName("tabBar");
    tabBar->setFixedHeight(36);
    auto* tabBarLayout = new QHBoxLayout(tabBar);
    tabBarLayout->setContentsMargins(8, 0, 8, 0);
    tabBarLayout->setSpacing(0);

    auto makeTabBtn = [&](const QString& text) {
        auto* btn = new QPushButton(text, tabBar);
        btn->setObjectName("tabBtn");
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        return btn;
    };
    m_tabLyricsBtn = makeTabBtn("Lyrics Editor");
    m_tabTimingBtn = makeTabBtn("Timing Sync");
    m_tabRenderBtn = makeTabBtn("Video Render");
    m_tabLyricsBtn->setChecked(true);

    tabBarLayout->addWidget(m_tabLyricsBtn);
    tabBarLayout->addWidget(m_tabTimingBtn);
    tabBarLayout->addWidget(m_tabRenderBtn);
    tabBarLayout->addStretch();

    m_saveProjectBtn = new QPushButton("Save", tabBar);
    m_saveProjectBtn->setObjectName("primaryAction");
    m_saveProjectBtn->setFixedHeight(24);
    tabBarLayout->addWidget(m_saveProjectBtn);

    m_consoleBtn = new QPushButton("Log", tabBar);
    m_consoleBtn->setObjectName("navBtn");
    m_consoleBtn->setFixedHeight(24);
    tabBarLayout->addWidget(m_consoleBtn);

    m_rootLayout->addWidget(tabBar);

    // ── ROW 1: WORKSPACE SPLITTER ────────────────────────────────────────────
    m_workspaceSplitter = new QSplitter(Qt::Horizontal, this);
    m_workspaceSplitter->setChildrenCollapsible(false);

    // ── Task 3.2: Tools Panel (32px fixed width) ─────────────────────────────
    auto* toolsPanel = new QWidget(m_workspaceSplitter);
    toolsPanel->setObjectName("toolsPanel");
    toolsPanel->setFixedWidth(40);
    auto* toolsPanelLayout = new QVBoxLayout(toolsPanel);
    toolsPanelLayout->setContentsMargins(4, 8, 4, 8);
    toolsPanelLayout->setSpacing(4);
    toolsPanelLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    m_toolBtnGroup = new QButtonGroup(this);
    m_toolBtnGroup->setExclusive(true);

    auto makeToolBtn = [&](const QString& icon, const QString& tip) {
        auto* btn = new QPushButton(icon, toolsPanel);
        btn->setObjectName("toolBtn");
        btn->setCheckable(true);
        btn->setToolTip(tip);
        btn->setFixedSize(28, 28);
        return btn;
    };

    auto* selectionBtn = makeToolBtn("\u25b6", "Selection Tool (V)");
    selectionBtn->setChecked(true);
    toolsPanelLayout->addWidget(selectionBtn, 0, Qt::AlignHCenter);
    m_toolBtnGroup->addButton(selectionBtn, static_cast<int>(ToolMode::Selection));

    auto* trackSelectBtn = makeToolBtn("\u21e5", "Track Select Tool (A)");
    toolsPanelLayout->addWidget(trackSelectBtn, 0, Qt::AlignHCenter);
    m_toolBtnGroup->addButton(trackSelectBtn, static_cast<int>(ToolMode::TrackSelect));

    auto* rollingEditBtn = makeToolBtn("\u21f9", "Rolling Edit Tool (N)");
    toolsPanelLayout->addWidget(rollingEditBtn, 0, Qt::AlignHCenter);
    m_toolBtnGroup->addButton(rollingEditBtn, static_cast<int>(ToolMode::RollingEdit));

    auto* razorBtn = makeToolBtn("\u2702", "Razor Tool (C)");
    toolsPanelLayout->addWidget(razorBtn, 0, Qt::AlignHCenter);
    m_toolBtnGroup->addButton(razorBtn, static_cast<int>(ToolMode::Razor));

    auto* slipBtn = makeToolBtn("\u21d4", "Slip Tool (Y)");
    toolsPanelLayout->addWidget(slipBtn, 0, Qt::AlignHCenter);
    m_toolBtnGroup->addButton(slipBtn, static_cast<int>(ToolMode::Slip));

    auto* slideBtn = makeToolBtn("\u21c6", "Slide Tool (U)");
    toolsPanelLayout->addWidget(slideBtn, 0, Qt::AlignHCenter);
    m_toolBtnGroup->addButton(slideBtn, static_cast<int>(ToolMode::Slide));

    auto* penBtn = makeToolBtn("\u270f", "Pen Tool (P)");
    toolsPanelLayout->addWidget(penBtn, 0, Qt::AlignHCenter);
    m_toolBtnGroup->addButton(penBtn, static_cast<int>(ToolMode::Pen));

    auto* zoomBtn = makeToolBtn("\u26b2", "Zoom Tool (Z)");
    toolsPanelLayout->addWidget(zoomBtn, 0, Qt::AlignHCenter);
    m_toolBtnGroup->addButton(zoomBtn, static_cast<int>(ToolMode::Zoom));

    connect(m_toolBtnGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_activeTool = static_cast<ToolMode>(id);
        emit toolModeChanged(m_activeTool);
    });

    // Keyboard shortcuts for tools
    auto addToolShortcut = [&](QKeySequence key, ToolMode mode) {
        auto* sc = new QShortcut(key, this);
        connect(sc, &QShortcut::activated, this, [this, mode]() {
            m_activeTool = mode;
            m_toolBtnGroup->button(static_cast<int>(mode))->setChecked(true);
            emit toolModeChanged(m_activeTool);
        });
    };
    addToolShortcut(Qt::Key_V, ToolMode::Selection);
    addToolShortcut(Qt::Key_A, ToolMode::TrackSelect);
    addToolShortcut(Qt::Key_N, ToolMode::RollingEdit);
    addToolShortcut(Qt::Key_C, ToolMode::Razor);
    addToolShortcut(Qt::Key_Y, ToolMode::Slip);
    addToolShortcut(Qt::Key_U, ToolMode::Slide);
    addToolShortcut(Qt::Key_P, ToolMode::Pen);
    addToolShortcut(Qt::Key_Z, ToolMode::Zoom);

    m_workspaceSplitter->addWidget(toolsPanel);

    // ── Content Splitter (Source | Captions | Program) ───────────────────────
    m_contentSplitter = new QSplitter(Qt::Horizontal, m_workspaceSplitter);
    m_contentSplitter->setChildrenCollapsible(false);

    // ── Task 3.3: Source Panel ────────────────────────────────────────────────
    auto* sourcePanel = new QWidget(m_contentSplitter);
    sourcePanel->setObjectName("sourcePanel");
    auto* sourcePanelLayout = new QVBoxLayout(sourcePanel);
    sourcePanelLayout->setContentsMargins(0, 0, 0, 0);
    sourcePanelLayout->setSpacing(0);
    // Display mode toggle button
    m_displayModeBtn = new QPushButton("\u25a3", sourcePanel); // ▣ = video overlay icon
    m_displayModeBtn->setObjectName("subtitleActionBtn");
    m_displayModeBtn->setFixedHeight(22);
    m_displayModeBtn->setFixedWidth(28);
    m_displayModeBtn->setToolTip("Toggle: Video Overlay / Karaoke Box (black background)");
    m_displayModeBtn->setCheckable(true);

    sourcePanelLayout->addWidget(makePanelHeader(sourcePanel, "Source Monitor", {m_displayModeBtn}));

    auto* sourceContent = new QWidget(sourcePanel);
    sourceContent->setObjectName("monitorContent");
    sourceContent->setStyleSheet("background: #0a0a0a;");
    auto* sourceContentLayout = new QVBoxLayout(sourceContent);
    sourceContentLayout->setContentsMargins(0, 0, 0, 0);
    sourceContentLayout->setSpacing(0);

    m_previewWidget = new KaraokePreview(sourceContent);
    m_previewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sourceContentLayout->addWidget(m_previewWidget);

    // Timecode overlay — use a QLabel positioned via stylesheet margin inside the content
    m_sourceTimecodeLabel = new QLabel("00:00:00:00", m_previewWidget);
    m_sourceTimecodeLabel->setObjectName("timecodeLabel");
    m_sourceTimecodeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_sourceTimecodeLabel->setStyleSheet("QLabel { background: transparent; color: #e0e0e0; font-family: 'Consolas', monospace; font-size: 11px; padding: 4px 6px; }");
    m_sourceTimecodeLabel->move(4, 4);
    m_sourceTimecodeLabel->raise();

    // Placeholder when no source
    bool hasSource = m_project && m_project->source_file.has_value() && !m_project->source_file.value().empty();
    if (!hasSource) {
        auto* sourcePlaceholder = new QLabel("No Source", sourceContent);
        sourcePlaceholder->setObjectName("monitorPlaceholder");
        sourcePlaceholder->setAlignment(Qt::AlignCenter);
        sourcePlaceholder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sourceContentLayout->addWidget(sourcePlaceholder);
        m_previewWidget->hide();
    }

    sourcePanelLayout->addWidget(sourceContent, 1);
    m_contentSplitter->addWidget(sourcePanel);

    // ── Task 3.4: Captions Panel ──────────────────────────────────────────────
    auto* captionsPanel = new QWidget(m_contentSplitter);
    captionsPanel->setObjectName("captionsPanel");
    auto* captionsPanelLayout = new QVBoxLayout(captionsPanel);
    captionsPanelLayout->setContentsMargins(0, 0, 0, 0);
    captionsPanelLayout->setSpacing(0);

    m_addSubtitleBtn = new QPushButton("+ Add", captionsPanel);
    m_addSubtitleBtn->setObjectName("subtitleActionBtn");
    m_addSubtitleBtn->setFixedHeight(22);

    m_splitTokensBtn = new QPushButton("AI Sync", captionsPanel);
    m_splitTokensBtn->setObjectName("subtitleActionBtnAccent");
    m_splitTokensBtn->setFixedHeight(22);

    captionsPanelLayout->addWidget(makePanelHeader(captionsPanel, "Captions",
        {m_addSubtitleBtn, m_splitTokensBtn}));

    m_syncTable = new QTableWidget(0, 5, captionsPanel);
    m_syncTable->setObjectName("subtitleTable");
    m_syncTable->setHorizontalHeaderLabels({"#", "START", "END", "CONTENT", "DUR."});
    m_syncTable->setShowGrid(false);
    m_syncTable->setAlternatingRowColors(true);
    m_syncTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_syncTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_syncTable->verticalHeader()->setVisible(false);
    m_syncTable->verticalHeader()->setDefaultSectionSize(28);
    auto* hh = m_syncTable->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::Fixed); m_syncTable->setColumnWidth(0, 28);
    hh->setSectionResizeMode(1, QHeaderView::Fixed); m_syncTable->setColumnWidth(1, 72);
    hh->setSectionResizeMode(2, QHeaderView::Fixed); m_syncTable->setColumnWidth(2, 72);
    hh->setSectionResizeMode(3, QHeaderView::Stretch);
    hh->setSectionResizeMode(4, QHeaderView::Fixed); m_syncTable->setColumnWidth(4, 52);
    captionsPanelLayout->addWidget(m_syncTable, 1);

    m_statusLabel = new QLabel("", captionsPanel);
    m_statusLabel->hide();

    m_contentSplitter->addWidget(captionsPanel);

    m_workspaceSplitter->addWidget(m_contentSplitter);

    // Load workspace splitter sizes — contentSplitter has 2 children: source, captions
    {
        QString sizesStr = m_config ? m_config->get<QString>("ui.editor.workspace_splitter", "") : "";
        QStringList parts = sizesStr.split(',', Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            QList<int> contentSizes;
            for (int i = 0; i < 2 && i < parts.size(); ++i) {
                int v = parts[i].trimmed().toInt();
                contentSizes.append(v > 0 ? v : 500);
            }
            m_contentSplitter->setSizes(contentSizes);
        } else {
            // Default: monitor takes ~55%, captions ~45%
            m_contentSplitter->setSizes({560, 440});
        }
    }

    connect(m_workspaceSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        if (!m_config) return;
        QList<int> cs = m_contentSplitter->sizes();
        QStringList parts;
        for (int s : cs) parts.append(QString::number(s));
        m_config->set("ui.editor.workspace_splitter", parts.join(','));
    });
    connect(m_contentSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        if (!m_config) return;
        QList<int> cs = m_contentSplitter->sizes();
        QStringList parts;
        for (int s : cs) parts.append(QString::number(s));
        m_config->set("ui.editor.workspace_splitter", parts.join(','));
    });

    // ── ROW 2: TIMELINE PANEL ────────────────────────────────────────────────
    auto* timelinePanel = new QWidget(this);
    timelinePanel->setObjectName("timelinePanel");
    auto* timelinePanelLayout = new QVBoxLayout(timelinePanel);
    timelinePanelLayout->setContentsMargins(0, 0, 0, 0);
    timelinePanelLayout->setSpacing(0);
    timelinePanelLayout->addWidget(makePanelHeader(timelinePanel, "Timeline"));

    auto* timelineSplitter = new QSplitter(Qt::Vertical, timelinePanel);
    timelineSplitter->setChildrenCollapsible(false);

    m_waveformWidget = new WaveformWidget(timelineSplitter);
    m_waveformWidget->setFixedHeight(60);
    timelineSplitter->addWidget(m_waveformWidget);

    m_timelineWidget = new TimelineWidget(timelineSplitter);
    m_timelineWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    timelineSplitter->addWidget(m_timelineWidget);

    timelinePanelLayout->addWidget(timelineSplitter, 1);
    timelinePanel->setMinimumHeight(100);

    // ── LYRICS BLOCKS PANEL (below timeline, for drag-to-timeline) ───────────
    m_lyricsBlocksPanel = new QWidget(this);
    m_lyricsBlocksPanel->setObjectName("lyricsBlocksPanel");
    m_lyricsBlocksPanel->setFixedHeight(72);
    auto* lbpLayout = new QVBoxLayout(m_lyricsBlocksPanel);
    lbpLayout->setContentsMargins(0, 0, 0, 0);
    lbpLayout->setSpacing(0);

    // Header
    auto* lbpHeader = makePanelHeader(m_lyricsBlocksPanel, "Lyrics Blocks  —  drag to timeline");
    lbpLayout->addWidget(lbpHeader);

    // Scrollable row of lyric blocks
    auto* lbpScroll = new QScrollArea(m_lyricsBlocksPanel);
    lbpScroll->setObjectName("lyricsBlocksScroll");
    lbpScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    lbpScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lbpScroll->setWidgetResizable(true);
    lbpScroll->setFixedHeight(44);

    auto* lbpContent = new QWidget();
    lbpContent->setObjectName("lyricsBlocksContent");
    auto* lbpContentLayout = new QHBoxLayout(lbpContent);
    lbpContentLayout->setContentsMargins(4, 2, 4, 2);
    lbpContentLayout->setSpacing(4);
    lbpContentLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Populate blocks from project lyrics
    if (m_project) {
        for (int i = 0; i < (int)m_project->lyrics.lines.size(); ++i) {
            const auto& line = m_project->lyrics.lines[i];
            auto* block = new QPushButton(QString::fromStdString(line.text), lbpContent);
            block->setObjectName("lyricsBlock");
            block->setFixedHeight(28);
            block->setToolTip(QString("Line %1 — drag to timeline or click to seek").arg(i + 1));
            // Click to seek to this line's start time
            connect(block, &QPushButton::clicked, this, [this, i]() {
                if (i < (int)m_project->lyrics.lines.size())
                    m_audioPlayer->seek(m_project->lyrics.lines[i].start_time);
            });
            lbpContentLayout->addWidget(block);
        }
    }
    lbpContentLayout->addStretch();
    lbpScroll->setWidget(lbpContent);
    lbpLayout->addWidget(lbpScroll);

    // ── VIEW STACK: wraps workspace + timeline so tabs can switch content ─────
    // Page 0: Timing Sync (workspace panels + timeline)
    m_timingView = new QWidget(this);
    auto* timingViewLayout = new QVBoxLayout(m_timingView);
    timingViewLayout->setContentsMargins(0, 0, 0, 0);
    timingViewLayout->setSpacing(0);
    timingViewLayout->addWidget(m_workspaceSplitter, 1);
    timingViewLayout->addWidget(timelinePanel);
    timingViewLayout->addWidget(m_lyricsBlocksPanel);

    // Page 1: Lyrics Editor
    m_lyricsView = new QWidget(this);
    auto* lyricsViewLayout = new QVBoxLayout(m_lyricsView);
    lyricsViewLayout->setContentsMargins(0, 0, 0, 0);
    lyricsViewLayout->setSpacing(0);

    m_sourceLyricsEdit = new QPlainTextEdit(m_lyricsView);
    m_sourceLyricsEdit->setObjectName("lyricsEdit");
    m_sourceLyricsEdit->setReadOnly(false);
    m_sourceLyricsEdit->setPlaceholderText("Paste lyrics here, one line per subtitle...");
    lyricsViewLayout->addWidget(m_sourceLyricsEdit, 1);

    // Page 2: Video Render
    m_renderView = new QWidget(this);
    auto* rvL = new QVBoxLayout(m_renderView);
    rvL->setContentsMargins(32, 32, 32, 32);
    rvL->setSpacing(24);
    rvL->setAlignment(Qt::AlignTop);
    auto* rvTitle = new QLabel("Finalize & Export Video", m_renderView);
    rvTitle->setStyleSheet("font-size: 20px; font-weight: 700; color: #e0e0e0;");
    rvL->addWidget(rvTitle);
    auto* settingsBox = new QWidget(m_renderView);
    settingsBox->setStyleSheet("background: #232323; border-radius: 6px; border: 1px solid #3a3a3a;");
    auto* sbl = new QGridLayout(settingsBox);
    sbl->setContentsMargins(24, 24, 24, 24);
    sbl->setSpacing(16);
    auto addSettingRow = [&](int row, QString label, QWidget* widget) {
        auto* lbl = new QLabel(label, settingsBox);
        lbl->setStyleSheet("color: #888888; font-weight: 700; font-size: 10px; letter-spacing: 0.5px;");
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
    sbl->addWidget(burnCheck, 3, 1);
    rvL->addWidget(settingsBox);
    auto* exportBtn = new QPushButton("START EXPORT & RENDER", m_renderView);
    exportBtn->setObjectName("primaryAction");
    exportBtn->setMinimumHeight(40);
    rvL->addWidget(exportBtn);
    connect(exportBtn, &QPushButton::clicked, this, &EditorMode::onExportClicked);
    rvL->addStretch();

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_timingView);   // index 0 — Timing Sync (default)
    m_viewStack->addWidget(m_lyricsView);   // index 1 — Lyrics Editor
    m_viewStack->addWidget(m_renderView);   // index 2 — Video Render
    m_viewStack->setCurrentIndex(0);

    m_rootLayout->addWidget(m_viewStack, 1);

    // ── ROW 3: TRANSPORT BAR (40px fixed height) ─────────────────────────────
    m_transportBar = new QWidget(this);
    m_transportBar->setObjectName("transportBar");
    m_transportBar->setFixedHeight(40);
    auto* tLay = new QHBoxLayout(m_transportBar);
    tLay->setContentsMargins(16, 0, 16, 0);
    tLay->setSpacing(8);

    auto makeTransportBtn = [&](const QString& text, const QString& tip) {
        auto* btn = new QPushButton(text, m_transportBar);
        btn->setObjectName("transportBtn");
        btn->setToolTip(tip);
        btn->setFixedSize(28, 28);
        return btn;
    };

    auto* goToInBtn    = makeTransportBtn("\u23ee", "Go to In Point");
    auto* stepBackBtn  = makeTransportBtn("\u23ea", "Step Back (1 frame)");
    auto* stepFwdBtn   = makeTransportBtn("\u23e9", "Step Forward (1 frame)");
    auto* goToOutBtn   = makeTransportBtn("\u23ed", "Go to Out Point");

    tLay->addWidget(goToInBtn);
    tLay->addWidget(stepBackBtn);
    tLay->addWidget(stepFwdBtn);
    tLay->addWidget(goToOutBtn);

    auto* playBtn = new QPushButton("\u25b6", m_transportBar); // ▶
    playBtn->setObjectName("playBtn");
    playBtn->setFixedSize(28, 28);
    tLay->addWidget(playBtn);

    m_playbackTimeLabel = new QLabel("00:00:00:00", m_transportBar);
    m_playbackTimeLabel->setObjectName("timecodeLabel");
    tLay->addWidget(m_playbackTimeLabel);

    // Stamp buttons — hidden in transport bar, accessible via captions table context menu
    m_setStartBtn = new QPushButton("[ Start", this);
    m_setStartBtn->setObjectName("stampBtn");
    m_setStartBtn->setToolTip("Set start time of selected line to current playback position");
    m_setStartBtn->hide();

    m_setEndBtn = new QPushButton("] End", this);
    m_setEndBtn->setObjectName("stampBtn");
    m_setEndBtn->setToolTip("Set end time of selected line to current playback position");
    m_setEndBtn->hide();

    m_trackSelector = new QComboBox(m_transportBar);
    m_trackSelector->setObjectName("trackSelector");
    m_trackSelector->addItems({"Original Mix", "Instrumental", "Vocals Only"});
    m_trackSelector->setFixedWidth(130);
    tLay->addWidget(m_trackSelector);

    auto* transportWaveform = new WaveformWidget(m_transportBar);
    transportWaveform->setMinimumHeight(30);
    tLay->addWidget(transportWaveform, 1);

    m_rootLayout->addWidget(m_transportBar);

    // ── Hidden legacy widgets ─────────────────────────────────────────────────
    m_toolBar = new QToolBar(this); m_toolBar->hide();
    m_langCombo = new QComboBox(this); m_langCombo->setCurrentText("Auto"); m_langCombo->hide();
    m_whisperBtn = new QPushButton(this); m_whisperBtn->hide();
    m_precisionBtn = new QPushButton(this); m_precisionBtn->hide();
    m_importBtn = new QPushButton(this); m_importBtn->hide();
    m_subtitleInput = new QLineEdit(this); m_subtitleInput->hide();
    m_audioPlayer = new AudioPlayer(this); m_audioPlayer->hide();

    m_lyricsSubStack = new QStackedWidget(this); m_lyricsSubStack->hide();
    m_lyrSourceBtn = new QPushButton(this); m_lyrSourceBtn->hide();
    m_lyrHistoryBtn = new QPushButton(this); m_lyrHistoryBtn->hide();

    // ── Tab bar connections ───────────────────────────────────────────────────
    // Tab 0 = Lyrics Editor, Tab 1 = Timing Sync, Tab 2 = Video Render
    // ViewStack: 0 = Timing Sync, 1 = Lyrics Editor, 2 = Video Render
    connect(m_tabLyricsBtn, &QPushButton::clicked, this, [this]{ m_viewStack->setCurrentIndex(1); });
    connect(m_tabTimingBtn, &QPushButton::clicked, this, [this]{ m_viewStack->setCurrentIndex(0); });
    connect(m_tabRenderBtn, &QPushButton::clicked, this, [this]{ m_viewStack->setCurrentIndex(2); });
    // Default: Timing Sync tab active
    m_tabTimingBtn->setChecked(true);
    m_tabLyricsBtn->setChecked(false);

    connect(m_saveProjectBtn, &QPushButton::clicked, this, [this]{ emit requestSave(); });
    connect(m_consoleBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath() + "/debug.log"));
    });

    connect(m_trackSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EditorMode::onTrackSelectionChanged);

    // Display mode toggle
    connect(m_displayModeBtn, &QPushButton::toggled, this, [this](bool checked) {
        auto mode = checked ? KaraokePreview::DisplayMode::CenteredBlack
                            : KaraokePreview::DisplayMode::VideoOverlay;
        m_previewWidget->setDisplayMode(mode);
        m_displayModeBtn->setText(checked ? "\u25a0" : "\u25a3"); // ■ / ▣
        m_displayModeBtn->setToolTip(checked ? "Mode: Karaoke Box (click for Video Overlay)"
                                             : "Mode: Video Overlay (click for Karaoke Box)");
    });

    // ── Source Lyrics inline editing ──────────────────────────────────────────
    connect(m_sourceLyricsEdit, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_syncingFromEdit || !m_project) return;
        QString fullText = m_sourceLyricsEdit->toPlainText();
        QStringList newLines = fullText.split('\n', Qt::KeepEmptyParts);
        while ((int)m_project->lyrics.lines.size() < newLines.size()) {
            core::LyricsLine ln; ln.start_time = 0.0f; ln.end_time = 0.0f;
            m_project->lyrics.lines.push_back(ln);
        }
        for (int i = 0; i < newLines.size(); ++i) {
            m_project->lyrics.lines[i].text = newLines[i].trimmed().toStdString();
            m_project->lyrics.lines[i].splitIntoWords();
        }
        if ((int)m_project->lyrics.lines.size() > newLines.size())
            m_project->lyrics.lines.resize(newLines.size());
        emit unsavedChangesChanged(true);
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

    // ── Transport bar connections ─────────────────────────────────────────────
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

    connect(playBtn, &QPushButton::clicked, this, [this]{
        if (m_audioPlayer->player()->playbackState() == QMediaPlayer::PlayingState) {
            m_audioPlayer->player()->pause();
            if (m_videoPlayer) m_videoPlayer->pause();
        } else {
            m_audioPlayer->player()->play();
            if (m_videoPlayer) m_videoPlayer->play();
        }
    });

    connect(m_audioPlayer->player(), &QMediaPlayer::playbackStateChanged, this, [playBtn](QMediaPlayer::PlaybackState state){
        playBtn->setText(state == QMediaPlayer::PlayingState ? "\u23f8" : "\u25b6");
    });

    // Transport navigation buttons
    connect(goToInBtn, &QPushButton::clicked, this, [this]{ m_audioPlayer->seek(0.0); });
    connect(goToOutBtn, &QPushButton::clicked, this, [this]{
        double dur = m_audioPlayer->player()->duration() / 1000.0;
        m_audioPlayer->seek(dur);
    });
    connect(stepBackBtn, &QPushButton::clicked, this, [this]{
        double t = std::max(0.0, m_currentTime - (1.0 / 30.0));
        m_audioPlayer->seek(t);
    });
    connect(stepFwdBtn, &QPushButton::clicked, this, [this]{
        double dur = m_audioPlayer->player()->duration() / 1000.0;
        double t = std::min(dur, m_currentTime + (1.0 / 30.0));
        m_audioPlayer->seek(t);
    });
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
        if (!m_project) return;
        int row = m_syncTable->currentRow();
        if (row >= 0) {
            // Single line: split words + split long lines
            int idx = m_syncTable->item(row, 3)->data(Qt::UserRole + 1).toInt();
            m_project->lyrics.lines[idx].splitIntoWords();
        } else {
            // No selection: apply to all lines — split words + split long lines
            for (auto& line : m_project->lyrics.lines)
                line.splitIntoWords();
            m_project->lyrics.splitLongLines(6);
        }
        updateSubtitleList();
        emit unsavedChangesChanged(true);
    });
    m_syncTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_syncTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = m_syncTable->itemAt(pos); if (!item) return;
        int row = item->row(); QMenu menu(this);
        menu.addAction("⏱  Precision Timing...", this, [this, row]() {
            int idx = m_syncTable->item(row, 3)->data(Qt::UserRole + 1).toInt(); WordEditor editor(&m_project->lyrics.lines[idx], this); if (editor.exec() == QDialog::Accepted && editor.wasModified()) { updateSubtitleList(); emit unsavedChangesChanged(true); }
        });
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
    // Let the global dark_theme.qss loaded by MainWindow handle all styling.
    this->setStyleSheet("");
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
    if (m_playbackTimeLabel) m_playbackTimeLabel->setText(formatTimecode(timeSeconds));
    if (m_sourceTimecodeLabel) m_sourceTimecodeLabel->setText(formatTimecode(timeSeconds));
    
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

    // Refresh lyrics blocks panel
    if (m_lyricsBlocksPanel) {
        auto* scroll = m_lyricsBlocksPanel->findChild<QScrollArea*>("lyricsBlocksScroll");
        if (scroll) {
            auto* content = new QWidget();
            content->setObjectName("lyricsBlocksContent");
            auto* lay = new QHBoxLayout(content);
            lay->setContentsMargins(4, 2, 4, 2);
            lay->setSpacing(4);
            lay->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            for (int i = 0; i < (int)lines.size(); ++i) {
                const auto& line = lines[i];
                auto* block = new QPushButton(QString::fromStdString(line.text), content);
                block->setObjectName("lyricsBlock");
                block->setFixedHeight(28);
                block->setToolTip(QString("Line %1 — click to seek to %2")
                    .arg(i + 1).arg(formatTimeMMSS(line.start_time)));
                connect(block, &QPushButton::clicked, this, [this, i]() {
                    if (i < (int)m_project->lyrics.lines.size())
                        m_audioPlayer->seek(m_project->lyrics.lines[i].start_time);
                });
                lay->addWidget(block);
            }
            lay->addStretch();
            scroll->setWidget(content);
        }
    }
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
    confirmDlg->setStyleSheet("QDialog { background: #1e293b; color: #e2e8f0; } QLabel { color: #94a3b8; font-size: 12px; } QComboBox { background: #0f172a; border: 1px solid rgba(255,255,255,0.1); border-radius: 8px; padding: 8px; color: white; } QPushButton { background: #3b82f6; color: white; border-radius: 8px; padding: 10px 20px; font-weight: bold; border: none; } QPushButton#cancelBtn { background: rgba(255,255,255,0.05); color: #94a3b8; }");
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
        m_project->lyrics.splitLongLines(6); // intelligently split lines > 6 words
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
            m_project->lyrics.splitLongLines(6);
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
