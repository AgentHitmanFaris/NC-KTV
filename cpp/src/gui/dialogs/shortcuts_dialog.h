#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
namespace ncktv {
class ShortcutsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutsDialog(QWidget* parent = nullptr);
private:
    void setupUi();
    void applyTheme();
    QTableWidget* m_table = nullptr;
};
} // namespace ncktv
