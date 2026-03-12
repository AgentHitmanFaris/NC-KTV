#pragma once
#include <QDialog>
#include "model_manager_widget.h"
namespace ncktv {
class ModelManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelManagerDialog(QWidget* parent = nullptr);
private:
    ModelManagerWidget* m_widget = nullptr;
};
} // namespace ncktv
