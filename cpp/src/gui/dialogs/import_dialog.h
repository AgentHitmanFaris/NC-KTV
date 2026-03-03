#pragma once
#include <QDialog>
namespace ncktv {
class ImportDialog : public QDialog { Q_OBJECT
public: explicit ImportDialog(QWidget* parent = nullptr); };
class NewProjectDialog : public QDialog { Q_OBJECT
public: explicit NewProjectDialog(QWidget* parent = nullptr); };
class ModelManagerDialog : public QDialog { Q_OBJECT
public: explicit ModelManagerDialog(QWidget* parent = nullptr); };
} // namespace ncktv
