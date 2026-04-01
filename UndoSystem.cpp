#include "UndoSystem.h"

UndoSystem::UndoSystem(QObject* parent)
    : QObject(parent)
{
}

void UndoSystem::pushAction(const UndoAction& action)
{
    m_undoStack.push(action);
    m_redoStack.clear();
    
    trimUndoStack();
    
    emit undoStackChanged(canUndo(), canRedo());
}

bool UndoSystem::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool UndoSystem::canRedo() const
{
    return !m_redoStack.isEmpty();
}

UndoAction UndoSystem::undo()
{
    if (m_undoStack.isEmpty()) {
        return UndoAction();
    }
    
    UndoAction action = m_undoStack.pop();
    m_redoStack.push(action);
    
    emit undoStackChanged(canUndo(), canRedo());
    
    return action;
}

UndoAction UndoSystem::redo()
{
    if (m_redoStack.isEmpty()) {
        return UndoAction();
    }
    
    UndoAction action = m_redoStack.pop();
    m_undoStack.push(action);
    
    trimUndoStack();
    
    emit undoStackChanged(canUndo(), canRedo());
    
    return action;
}

void UndoSystem::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    
    emit undoStackChanged(false, false);
}

void UndoSystem::clearForTab(int tabIndex)
{
    QStack<UndoAction> newUndoStack;
    QStack<UndoAction> newRedoStack;
    
    while (!m_undoStack.isEmpty()) {
        UndoAction action = m_undoStack.pop();
        if (action.tabIndex != tabIndex) {
            newUndoStack.push(action);
        }
    }
    
    while (!m_redoStack.isEmpty()) {
        UndoAction action = m_redoStack.pop();
        if (action.tabIndex != tabIndex) {
            newRedoStack.push(action);
        }
    }
    
    m_undoStack = newUndoStack;
    m_redoStack = newRedoStack;
    
    emit undoStackChanged(canUndo(), canRedo());
}

QString UndoSystem::getUndoDescription() const
{
    if (m_undoStack.isEmpty()) {
        return QString();
    }
    
    return m_undoStack.top().description;
}

QString UndoSystem::getRedoDescription() const
{
    if (m_redoStack.isEmpty()) {
        return QString();
    }
    
    return m_redoStack.top().description;
}

void UndoSystem::trimUndoStack()
{
    while (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
        QStack<UndoAction> temp;
        while (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
            m_undoStack.pop();
        }
    }
}
