#include "lyrics_search_dialog.h"
#include <QGroupBox>
#include <QHeaderView>
#include <QFormLayout>

namespace ncktv {

LyricsSearchDialog::LyricsSearchDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Search Lyrics Online (LRCLIB)");
    setMinimumSize(800, 500);
    
    m_client = new LrcLibClient(this);
    connect(m_client, &LrcLibClient::searchComplete, this, &LyricsSearchDialog::onSearchResult);
    connect(m_client, &LrcLibClient::searchError, this, &LyricsSearchDialog::onSearchError);

    setupUi();
    applyTheme();
}

void LyricsSearchDialog::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ── Left Panel (Search & Results) ────────────────────────────────────────
    auto* leftContainer = new QWidget(this);
    leftContainer->setFixedWidth(320);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(16, 16, 16, 16);
    leftLayout->setSpacing(12);

    auto* searchGroup = new QGroupBox("Search Reference", this);
    auto* formLayout = new QFormLayout(searchGroup);
    
    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText("Song Title...");
    m_artistEdit = new QLineEdit(this);
    m_artistEdit->setPlaceholderText("Artist Name (Optional)...");
    
    formLayout->addRow("Title:", m_titleEdit);
    formLayout->addRow("Artist:", m_artistEdit);
    
    m_searchBtn = new QPushButton("Search", this);
    m_searchBtn->setStyleSheet("background: #4f46e5; color: white; font-weight: bold; padding: 6px;");
    connect(m_searchBtn, &QPushButton::clicked, this, &LyricsSearchDialog::onSearchClicked);
    
    leftLayout->addWidget(searchGroup);
    leftLayout->addWidget(m_searchBtn);

    leftLayout->addWidget(new QLabel("Results:"));
    m_resultsList = new QListWidget(this);
    connect(m_resultsList, &QListWidget::currentRowChanged, this, &LyricsSearchDialog::onResultSelected);
    leftLayout->addWidget(m_resultsList, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #888; font-size: 11px;");
    leftLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(leftContainer);

    // ── Divider ─────────────────────────────────────────────────────────────
    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Plain);
    divider->setStyleSheet("background-color: #333;");
    mainLayout->addWidget(divider);

    // ── Right Panel (Preview) ───────────────────────────────────────────────
    auto* rightContainer = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(16, 16, 16, 16);
    rightLayout->setSpacing(12);

    rightLayout->addWidget(new QLabel("Lyrics Preview:"));
    m_previewText = new QTextEdit(this);
    m_previewText->setReadOnly(true);
    m_previewText->setPlaceholderText("Select a result to preview lyrics...");
    m_previewText->setStyleSheet("background: #1e1e1e; font-family: 'Consolas'; font-size: 13px;");
    rightLayout->addWidget(m_previewText, 1);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    m_selectBtn = new QPushButton("Use These Lyrics", this);
    m_selectBtn->setEnabled(false);
    m_selectBtn->setStyleSheet("background: #10b981; color: white; font-weight: bold; padding: 8px 20px;");
    connect(m_selectBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(m_selectBtn);
    rightLayout->addLayout(btnLayout);

    mainLayout->addWidget(rightContainer);
}

void LyricsSearchDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background: #252526; color: #d4d4d4; }
        QLabel { color: #ccc; }
        QLineEdit { background: #3c3c3c; border: 1px solid #555; padding: 4px; color: #fff; }
        QPushButton { background: #333; border: 1px solid #555; padding: 4px 10px; color: #ccc; border-radius: 3px; }
        QPushButton:hover { background: #444; }
        QListWidget { background: #1e1e1e; border: 1px solid #333; color: #ccc; }
        QListWidget::item { padding: 8px; border-bottom: 1px solid #333; }
        QListWidget::item:selected { background: #2d2d30; color: #fff; border-left: 3px solid #4f46e5; }
        QGroupBox { font-weight: bold; color: #aaa; border: 1px solid #333; margin-top: 10px; padding-top: 10px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; }
    )");
}

void LyricsSearchDialog::setInitialSearch(const QString& title, const QString& artist) {
    m_titleEdit->setText(title);
    m_artistEdit->setText(artist);
    if (!title.isEmpty()) {
        onSearchClicked();
    }
}

void LyricsSearchDialog::onSearchClicked() {
    QString title = m_titleEdit->text().trimmed();
    QString artist = m_artistEdit->text().trimmed();
    
    if (title.isEmpty()) return;

    m_searchBtn->setEnabled(false);
    m_searchBtn->setText("Searching...");
    m_resultsList->clear();
    m_currentResults.clear();
    m_previewText->clear();
    m_selectBtn->setEnabled(false);
    m_statusLabel->setText("Querying LRCLIB API...");

    m_client->searchLyrics(title, artist);
}

void LyricsSearchDialog::onSearchResult(const QVector<LrcLibResult>& results) {
    m_searchBtn->setEnabled(true);
    m_searchBtn->setText("Search");
    m_currentResults = results;
    
    if (results.isEmpty()) {
        m_statusLabel->setText("No results found.");
        return;
    }

    m_statusLabel->setText(QString("Found %1 results.").arg(results.size()));

    for (const auto& r : results) {
        QString label = QString("%1\n%2").arg(r.title).arg(r.artist);
        auto* item = new QListWidgetItem(label, m_resultsList);
        bool hasSynced = !r.syncedLyrics.isEmpty();
        if (hasSynced) {
            item->setForeground(QColor("#10b981")); // Green for synced
            item->setToolTip("Synced lyrics available");
        } else {
            item->setForeground(QColor("#fbbf24")); // Amber for plain
            item->setToolTip("Plain text only");
        }
    }
}

void LyricsSearchDialog::onSearchError(const QString& error) {
    m_searchBtn->setEnabled(true);
    m_searchBtn->setText("Search");
    m_statusLabel->setText("Error: " + error);
}

void LyricsSearchDialog::onResultSelected(int index) {
    if (index < 0 || index >= m_currentResults.size()) return;

    const auto& r = m_currentResults[index];
    
    QString display = !r.syncedLyrics.isEmpty() ? r.syncedLyrics : r.plainLyrics;
    m_previewText->setPlainText(display);
    
    m_selectedSyncedLyrics = r.syncedLyrics;
    m_selectedPlainLyrics = r.plainLyrics;
    m_selectedTitle = r.title;
    m_selectedArtist = r.artist;
    
    m_selectBtn->setEnabled(!display.isEmpty());
}

} // namespace ncktv
