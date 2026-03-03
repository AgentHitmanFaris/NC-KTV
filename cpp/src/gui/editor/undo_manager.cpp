#include "undo_manager.h"
namespace ncktv {
UndoManager::UndoManager(QObject* parent) : QObject(parent) {}
void UndoManager::undo() {}
void UndoManager::redo() {}
}
