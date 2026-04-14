#pragma once
/*
 * NC-KTV GUI — New Project Dialog
 * File picker, project name, initial settings
 */

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class NewProjectDialog; }
QT_END_NAMESPACE

namespace ncktv {

class NewProjectDialog : public QDialog {
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget* parent = nullptr);
    ~NewProjectDialog() override;

    QString projectName() const;
    QString sourceFile() const;
    QString uvrModel() const;
    bool    useGpu() const;

private slots:
    void browseSourceFile();
    void onAccepted();

private:
    Ui::NewProjectDialog* ui;
};

} // namespace ncktv
