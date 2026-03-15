#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVector>
#include <QJsonDocument>
#include "net/lrclib_client.h"

namespace ncktv {

class LyricsSearchDialog : public QDialog {
    Q_OBJECT
public:
    explicit LyricsSearchDialog(QWidget* parent = nullptr);
    
    void setInitialSearch(const QString& title, const QString& artist = "");

    QString syncedLyrics() const { return m_selectedSyncedLyrics; }
    QString plainLyrics() const { return m_selectedPlainLyrics; }
    QString title() const { return m_selectedTitle; }
    QString artist() const { return m_selectedArtist; }

private slots:
    void onSearchClicked();
    void onResultSelected(int index);
    void onSearchResult(const QVector<LrcLibResult>& results);
    void onSearchError(const QString& error);

private:
    void setupUi();
    void applyTheme();

    QLineEdit* m_titleEdit;
    QLineEdit* m_artistEdit;
    QPushButton* m_searchBtn;
    QListWidget* m_resultsList;
    QTextEdit* m_previewText;
    QPushButton* m_selectBtn;
    QLabel* m_statusLabel;

    LrcLibClient* m_client;
    QVector<LrcLibResult> m_currentResults;

    QString m_selectedSyncedLyrics;
    QString m_selectedPlainLyrics;
    QString m_selectedTitle;
    QString m_selectedArtist;
};

} // namespace ncktv
