#include "Commands.h"

#include "ConnectionItem.h"
#include "ModuleItem.h"
#include "PortItem.h"

#include <QCoreApplication>
#include <QGraphicsScene>
#include <QHash>

static void safeRemove(QGraphicsScene *scene, QGraphicsItem *item) {
    if (scene && item && item->scene() == scene) {
        scene->removeItem(item);
    }
}

AddModuleCommand::AddModuleCommand(QGraphicsScene *scene, const QPointF &scenePos, AudioModuleKind kind, const QString &name,
                                   QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene), m_pos(scenePos), m_kind(kind), m_name(name) {
    setText(QCoreApplication::translate("Commands", "Add module"));
}

AddModuleCommand::~AddModuleCommand() {
    // If the module is currently in a scene, let that scene own and delete it.
    // If it's been undone (removed from scene), we must delete it to avoid leaks when the command is discarded.
    if (m_module && !m_module->scene()) {
        delete m_module;
        m_module = nullptr;
    }
}

void AddModuleCommand::redo() {
    if (!m_scene) {
        return;
    }
    if (!m_module) {
        m_module = new ModuleItem(m_kind, m_name);
        m_module->setPos(m_pos);
    }
    if (!m_inScene) {
        m_scene->addItem(m_module);
        m_inScene = true;
    } else if (m_module->scene() != m_scene) {
        m_scene->addItem(m_module);
    }
}

void AddModuleCommand::undo() {
    if (!m_scene || !m_module) {
        return;
    }
    safeRemove(m_scene, m_module);
    m_inScene = false;
}

AddConnectionCommand::AddConnectionCommand(QGraphicsScene *scene, PortItem *fromPort, PortItem *toPort, QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene), m_fromPort(fromPort), m_toPort(toPort) {
    setText(QCoreApplication::translate("Commands", "Add connection"));
}

AddConnectionCommand::~AddConnectionCommand() {
    // Same ownership rule as AddModuleCommand.
    if (m_connection && !m_connection->scene()) {
        if (auto *fp = m_connection->fromPort()) {
            if (fp->owner()) {
                fp->owner()->removeConnection(m_connection);
            }
        }
        if (auto *tp = m_connection->toPort()) {
            if (tp->owner()) {
                tp->owner()->removeConnection(m_connection);
            }
        }
        delete m_connection;
        m_connection = nullptr;
    }
}

void AddConnectionCommand::redo() {
    if (!m_scene || !m_fromPort || !m_toPort) {
        return;
    }

    if (!m_connection) {
        m_connection = new ConnectionItem(m_fromPort, m_toPort);
    } else {
        m_connection->setToPort(m_toPort);
    }

    if (m_connection->scene() != m_scene) {
        m_scene->addItem(m_connection);
    }
    if (auto *f = m_connection->from()) {
        f->addConnection(m_connection);
    }
    if (auto *t = m_connection->to()) {
        t->addConnection(m_connection);
    }
    m_inScene = true;
}

void AddConnectionCommand::undo() {
    if (!m_scene || !m_connection) {
        return;
    }

    if (auto *f = m_connection->from()) {
        f->removeConnection(m_connection);
    }
    if (auto *t = m_connection->to()) {
        t->removeConnection(m_connection);
    }
    safeRemove(m_scene, m_connection);
    m_inScene = false;
}

DeleteItemsCommand::DeleteItemsCommand(QGraphicsScene *scene, const QList<QGraphicsItem *> &selectedItems, QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene) {
    setText(QCoreApplication::translate("Commands", "Delete selected"));

    if (!m_scene) {
        return;
    }

    QSet<ModuleItem *> modules;
    QSet<ConnectionItem *> connections;

    for (QGraphicsItem *it : selectedItems) {
        if (auto *c = dynamic_cast<ConnectionItem *>(it)) {
            connections.insert(c);
        } else if (auto *m = dynamic_cast<ModuleItem *>(it)) {
            modules.insert(m);
        }
    }

    // If deleting modules, also delete their related connections.
    for (ModuleItem *m : modules) {
        for (ConnectionItem *c : m->connections()) {
            if (c) {
                connections.insert(c);
            }
        }
    }

    m_modules = QList<ModuleItem *>(modules.begin(), modules.end());
    m_connections = QList<ConnectionItem *>(connections.begin(), connections.end());
}

DeleteItemsCommand::~DeleteItemsCommand() {
    // If the delete is currently applied (items removed from scene), we own them and must free them when the command is discarded.
    // If the delete has been undone (items back in scene), the scene will delete them on shutdown.
    if (m_inScene) {
        return;
    }

    for (ConnectionItem *c : m_connections) {
        if (!c) {
            continue;
        }
        if (!c->scene()) {
            delete c;
        }
    }
    for (ModuleItem *m : m_modules) {
        if (!m) {
            continue;
        }
        if (!m->scene()) {
            delete m;
        }
    }
}

void DeleteItemsCommand::removeFromScene() {
    if (!m_scene || !m_inScene) {
        return;
    }

    for (ConnectionItem *c : m_connections) {
        if (!c) {
            continue;
        }
        if (auto *f = c->from()) {
            f->removeConnection(c);
        }
        if (auto *t = c->to()) {
            t->removeConnection(c);
        }
        safeRemove(m_scene, c);
    }

    for (ModuleItem *m : m_modules) {
        safeRemove(m_scene, m);
    }

    m_inScene = false;
}

void DeleteItemsCommand::addToScene() {
    if (!m_scene || m_inScene) {
        return;
    }

    for (ModuleItem *m : m_modules) {
        if (m && m->scene() != m_scene) {
            m_scene->addItem(m);
        }
    }

    for (ConnectionItem *c : m_connections) {
        if (!c) {
            continue;
        }
        if (c->scene() != m_scene) {
            m_scene->addItem(c);
        }
        if (auto *f = c->from()) {
            f->addConnection(c);
        }
        if (auto *t = c->to()) {
            t->addConnection(c);
        }
        c->updatePath();
    }

    m_inScene = true;
}

void DeleteItemsCommand::redo() {
    removeFromScene();
}

void DeleteItemsCommand::undo() {
    addToScene();
}

RenameModuleCommand::RenameModuleCommand(ModuleItem *module, const QString &before, const QString &after, QUndoCommand *parent)
    : QUndoCommand(parent), m_module(module), m_before(before), m_after(after) {
    setText(QCoreApplication::translate("Commands", "Rename module"));
}

void RenameModuleCommand::redo() {
    if (m_module) {
        m_module->setName(m_after);
    }
}

void RenameModuleCommand::undo() {
    if (m_module) {
        m_module->setName(m_before);
    }
}

DuplicateSelectionCommand::DuplicateSelectionCommand(QGraphicsScene *scene, const QList<QGraphicsItem *> &selectedItems,
                                                     const QPointF &offset, QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene), m_offset(offset) {
    setText(QCoreApplication::translate("Commands", "Duplicate selection"));

    if (!m_scene) {
        return;
    }

    QSet<ModuleItem *> modules;
    for (QGraphicsItem *it : selectedItems) {
        if (auto *m = dynamic_cast<ModuleItem *>(it)) {
            modules.insert(m);
        }
    }
    m_oldModules = QList<ModuleItem *>(modules.begin(), modules.end());
}

DuplicateSelectionCommand::~DuplicateSelectionCommand() {
    // If the duplicated items are currently NOT in a scene, we own them and must delete.
    if (m_inScene) {
        return;
    }
    for (ConnectionItem *c : m_newConnections) {
        if (c && !c->scene()) {
            delete c;
        }
    }
    for (ModuleItem *m : m_newModules) {
        if (m && !m->scene()) {
            delete m;
        }
    }
}

void DuplicateSelectionCommand::redo() {
    if (!m_scene || m_oldModules.isEmpty()) {
        return;
    }

    // Create on first redo
    if (m_newModules.isEmpty()) {
        QHash<ModuleItem *, ModuleItem *> map;
        for (ModuleItem *oldM : m_oldModules) {
            if (!oldM) {
                continue;
            }
            auto *newM = new ModuleItem(oldM->kind(), oldM->name(), QSizeF(oldM->rect().width(), oldM->rect().height()));
            newM->setParams(oldM->params());
            newM->setStyle(oldM->style());
            newM->setPos(oldM->pos() + m_offset);
            m_newModules.push_back(newM);
            map.insert(oldM, newM);
        }

        // Duplicate internal connections (both endpoints in selection), only once per connection.
        QSet<ConnectionItem *> seen;
        for (ModuleItem *oldM : m_oldModules) {
            if (!oldM) {
                continue;
            }
            for (ConnectionItem *c : oldM->connections()) {
                if (!c || seen.contains(c)) {
                    continue;
                }
                seen.insert(c);

                ModuleItem *fromOld = c->from();
                ModuleItem *toOld = c->to();
                if (!fromOld || !toOld) {
                    continue;
                }
                if (!map.contains(fromOld) || !map.contains(toOld)) {
                    continue; // only copy connections fully inside the selection
                }

                auto *fromNew = map.value(fromOld);
                auto *toNew = map.value(toOld);
                PortItem *fp = c->fromPort() ? fromNew->findPortById(c->fromPort()->portId()) : nullptr;
                PortItem *tp = c->toPort() ? toNew->findPortById(c->toPort()->portId()) : nullptr;
                if (!fp || !tp) {
                    continue;
                }
                auto *newC = new ConnectionItem(fp, tp);
                fromNew->addConnection(newC);
                toNew->addConnection(newC);
                m_newConnections.push_back(newC);
            }
        }
    }

    // Add to scene + select
    for (ModuleItem *m : m_newModules) {
        if (m && m->scene() != m_scene) {
            m_scene->addItem(m);
        }
    }
    for (ConnectionItem *c : m_newConnections) {
        if (c && c->scene() != m_scene) {
            m_scene->addItem(c);
        }
        if (c) {
            c->updatePath();
        }
    }

    // Select new copies, deselect originals (so drag moves the copy).
    for (ModuleItem *m : m_oldModules) {
        if (m) {
            m->setSelected(false);
        }
    }
    for (ModuleItem *m : m_newModules) {
        if (m) {
            m->setSelected(true);
        }
    }

    m_inScene = true;
}

void DuplicateSelectionCommand::undo() {
    if (!m_scene || !m_inScene) {
        return;
    }

    // Remove connections first (and detach from modules).
    for (ConnectionItem *c : m_newConnections) {
        if (!c) {
            continue;
        }
        if (auto *f = c->from()) {
            f->removeConnection(c);
        }
        if (auto *t = c->to()) {
            t->removeConnection(c);
        }
        safeRemove(m_scene, c);
    }

    for (ModuleItem *m : m_newModules) {
        safeRemove(m_scene, m);
    }

    m_inScene = false;
}

ChangeConnectionStyleCommand::ChangeConnectionStyleCommand(ConnectionItem *connection, const ConnectionItem::Style &before,
                                                           const ConnectionItem::Style &after, QUndoCommand *parent)
    : QUndoCommand(parent), m_connection(connection), m_before(before), m_after(after) {
    setText(QCoreApplication::translate("Commands", "Change connection style"));
}

void ChangeConnectionStyleCommand::redo() {
    if (m_connection) {
        m_connection->setStyle(m_after);
    }
}

void ChangeConnectionStyleCommand::undo() {
    if (m_connection) {
        m_connection->setStyle(m_before);
    }
}

ChangeModuleStyleCommand::ChangeModuleStyleCommand(ModuleItem *module, const ModuleItem::Style &before, const ModuleItem::Style &after,
                                                   QUndoCommand *parent)
    : QUndoCommand(parent), m_module(module), m_before(before), m_after(after) {
    setText(QCoreApplication::translate("Commands", "Change module style"));
}

void ChangeModuleStyleCommand::redo() {
    if (m_module) {
        m_module->setStyle(m_after);
    }
}

void ChangeModuleStyleCommand::undo() {
    if (m_module) {
        m_module->setStyle(m_before);
    }
}

MoveItemsCommand::MoveItemsCommand(QGraphicsScene *scene, const QHash<ModuleItem *, QPointF> &startScenePos,
                                   const QHash<ModuleItem *, QPointF> &endScenePos, const QString &description,
                                   QUndoCommand *parent)
    : QUndoCommand(parent), m_scene(scene), m_start(startScenePos), m_end(endScenePos) {
    if (!description.isEmpty()) {
        setText(description);
    } else {
        setText(QCoreApplication::translate("Commands", "Move modules"));
    }
}

void MoveItemsCommand::applyScenePositions(const QHash<ModuleItem *, QPointF> &scenePos) {
    if (!m_scene) {
        return;
    }
    for (auto it = scenePos.constBegin(); it != scenePos.constEnd(); ++it) {
        ModuleItem *m = it.key();
        if (!m || m->scene() != m_scene) {
            continue;
        }
        const QPointF target = it.value();
        const QPointF delta = target - m->scenePos();
        m->moveBy(delta.x(), delta.y());
    }
}

void MoveItemsCommand::redo() {
    applyScenePositions(m_end);
}

void MoveItemsCommand::undo() {
    applyScenePositions(m_start);
}

