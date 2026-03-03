#pragma once
#include <QObject>
namespace ncktv {
class UndoManager : public QObject {
    Q_OBJECT

public:
    explicit UndoManager(QObject* parent = nullptr);
    void undo();
    void redo();
};
} // namespace ncktv
