#pragma once

#include <QObject>
#include <QStack>
#include <QString>
#include <QVariant>
#include <QPair>
#include <vector>
#include <cstdint>

struct FileTab;

enum class UndoActionType {
    EditCell,
    AddEntry,
    DeleteEntry,
    DeleteMultipleEntries,
    AddTable,
    DeleteTable,
    RenameTable,
    BatchEdit
};

struct UndoAction {
    UndoActionType type;
    int tabIndex;
    int tableIndex;
    QVariant beforeData;
    QVariant afterData;
    QString description;
    
    UndoAction() : type(UndoActionType::EditCell), tabIndex(-1), tableIndex(-1) {}
};

struct CellEditData {
    int row;
    int column;
    QString oldKey;
    QString oldValue;
    QString newKey;
    QString newValue;
    
    CellEditData() : row(-1), column(-1) {}
};

struct EntryData {
    int row;
    QString key;
    QString value;
    
    EntryData() : row(-1) {}
    EntryData(int r, const QString& k, const QString& v) : row(r), key(k), value(v) {}
};

struct MultipleEntriesData {
    QList<EntryData> entries;
};

struct TableData {
    QString name;
    QList<EntryData> entries;
};

class UndoSystem : public QObject {
    Q_OBJECT
    
public:
    static const int MAX_UNDO_STACK_SIZE = 100;
    
    explicit UndoSystem(QObject* parent = nullptr);
    
    void pushAction(const UndoAction& action);
    bool canUndo() const;
    bool canRedo() const;
    UndoAction undo();
    UndoAction redo();
    void clear();
    void clearForTab(int tabIndex);
    
    QString getUndoDescription() const;
    QString getRedoDescription() const;
    
    int undoStackSize() const { return m_undoStack.size(); }
    int redoStackSize() const { return m_redoStack.size(); }
    
signals:
    void undoStackChanged(bool canUndo, bool canRedo);
    
private:
    QStack<UndoAction> m_undoStack;
    QStack<UndoAction> m_redoStack;
    
    void trimUndoStack();
};

Q_DECLARE_METATYPE(CellEditData)
Q_DECLARE_METATYPE(EntryData)
Q_DECLARE_METATYPE(MultipleEntriesData)
Q_DECLARE_METATYPE(TableData)
