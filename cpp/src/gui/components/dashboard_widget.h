#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class DashboardWidget; }
QT_END_NAMESPACE

namespace ncktv {

class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget *parent = nullptr);
    ~DashboardWidget() override;

signals:
    void newProjectRequested();
    void openProjectRequested();

private:
    Ui::DashboardWidget *ui;
};

} // namespace ncktv
