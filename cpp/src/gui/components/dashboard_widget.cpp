#include "dashboard_widget.h"
#include "ui_dashboard_widget.h"

namespace ncktv {

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DashboardWidget)
{
    ui->setupUi(this);

    connect(ui->btnNewProject, &QPushButton::clicked, this, &DashboardWidget::newProjectRequested);
    connect(ui->btnOpenProject, &QPushButton::clicked, this, &DashboardWidget::openProjectRequested);
}

DashboardWidget::~DashboardWidget() {
    delete ui;
}

} // namespace ncktv
