#pragma once

#include <QUndoCommand>

#include <QHash>
#include <QList>
#include <QPointF>
#include <QSet>
#include <QString>

#include "AudioTypes.h"
#include "ConnectionItem.h"
#include "ModuleItem.h"

class QGraphicsItem;
class QGraphicsScene;
class PortItem;

class ModuleItem;

class AddModuleCommand : public QUndoCommand {
public:
    AddModuleCommand(QGraphicsScene *scene, const QPointF &scenePos, AudioModuleKind kind, const QString &name = QString(),
                     QUndoCommand *parent = nullptr);
    ~AddModuleCommand() override;

    void undo() override;
    void redo() override;

    ModuleItem *module() const { return m_module; }

private:
    QGraphicsScene *m_scene = nullptr;
    QPointF m_pos;
    AudioModuleKind m_kind = AudioModuleKind::Gain;
    QString m_name;
    ModuleItem *m_module = nullptr;
    bool m_inScene = false;
};

class AddConnectionCommand : public QUndoCommand {
public:
    AddConnectionCommand(QGraphicsScene *scene, PortItem *fromPort, PortItem *toPort, QUndoCommand *parent = nullptr);
    ~AddConnectionCommand() override;

    void undo() override;
    void redo() override;

private:
    QGraphicsScene *m_scene = nullptr;
    PortItem *m_fromPort = nullptr;
    PortItem *m_toPort = nullptr;
    ConnectionItem *m_connection = nullptr;
    bool m_inScene = false;
};

class DeleteItemsCommand : public QUndoCommand {
public:
    DeleteItemsCommand(QGraphicsScene *scene, const QList<QGraphicsItem *> &selectedItems, QUndoCommand *parent = nullptr);
    ~DeleteItemsCommand() override;

    void undo() override;
    void redo() override;

private:
    void removeFromScene();
    void addToScene();

    QGraphicsScene *m_scene = nullptr;
    QList<ModuleItem *> m_modules;
    QList<ConnectionItem *> m_connections;
    bool m_inScene = true;
};

class RenameModuleCommand : public QUndoCommand {
public:
    RenameModuleCommand(ModuleItem *module, const QString &before, const QString &after, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ModuleItem *m_module = nullptr;
    QString m_before;
    QString m_after;
};

class DuplicateSelectionCommand : public QUndoCommand {
public:
    DuplicateSelectionCommand(QGraphicsScene *scene, const QList<QGraphicsItem *> &selectedItems,
                              const QPointF &offset = QPointF(40, 40), QUndoCommand *parent = nullptr);
    ~DuplicateSelectionCommand() override;

    void undo() override;
    void redo() override;

private:
    QGraphicsScene *m_scene = nullptr;
    QPointF m_offset;
    QList<ModuleItem *> m_oldModules;
    QList<ModuleItem *> m_newModules;
    QList<ConnectionItem *> m_newConnections;
    bool m_inScene = false;
};

class ChangeConnectionStyleCommand : public QUndoCommand {
public:
    ChangeConnectionStyleCommand(ConnectionItem *connection, const ConnectionItem::Style &before,
                                 const ConnectionItem::Style &after, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ConnectionItem *m_connection = nullptr;
    ConnectionItem::Style m_before;
    ConnectionItem::Style m_after;
};

class ChangeModuleStyleCommand : public QUndoCommand {
public:
    ChangeModuleStyleCommand(ModuleItem *module, const ModuleItem::Style &before, const ModuleItem::Style &after,
                             QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ModuleItem *m_module = nullptr;
    ModuleItem::Style m_before;
    ModuleItem::Style m_after;
};

/** One logical drag = one command (push on mouse release). Positions are in scene coordinates. */
class MoveItemsCommand : public QUndoCommand {
public:
    /** If \a description is non-empty, it is used as the undo step label; otherwise "Move modules". */
    MoveItemsCommand(QGraphicsScene *scene, const QHash<ModuleItem *, QPointF> &startScenePos,
                     const QHash<ModuleItem *, QPointF> &endScenePos, const QString &description = QString(),
                     QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    void applyScenePositions(const QHash<ModuleItem *, QPointF> &scenePos);

    QGraphicsScene *m_scene = nullptr;
    QHash<ModuleItem *, QPointF> m_start;
    QHash<ModuleItem *, QPointF> m_end;
};

