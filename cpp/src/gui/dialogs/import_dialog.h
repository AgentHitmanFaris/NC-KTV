#pragma once
/*
 * NC-KTV GUI — Import Dialog
 * Subtitle/lyrics file browser with format auto-detection and preview
 */

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include <QRadioButton>

namespace ncktv {

class ImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportDialog(QWidget* parent = nullptr);

    QString filePath() const;
    QString detectedFormat() const;

private slots:
    void browseFile();
    void searchOnline();
    void onFileSelected(const QString& path);
    void onAccepted();

private:
    void setupUi();
    void applyTheme();
    void detectFormat(const QString& path);

    QLineEdit*   m_pathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QLabel*      m_formatLabel = nullptr;
    QLabel*      m_lineCountLabel = nullptr;
    QTextEdit*   m_previewText = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    QString m_detectedFormat;
};

} // namespace ncktv
