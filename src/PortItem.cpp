#include "PortItem.h"

#include "Commands.h"
#include "ConnectionItem.h"
#include "DemoView.h"
#include "ModuleItem.h"

#include <QBrush>
#include <QColor>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPen>

PortItem::PortItem(Type type, ModuleItem *owner, const QString &portId, const QString &label)
    : QGraphicsEllipseItem(owner), m_type(type), m_owner(owner), m_portId(portId), m_label(label) {
    constexpr qreal r = 6.0;
    setRect(0, 0, r * 2.0, r * 2.0);
    setPen(QPen(QColor(40, 40, 40), 1));
    setBrush(type == Type::Input ? QColor(33, 150, 243) : QColor(244, 67, 54));
    setCursor(QCursor(Qt::CrossCursor));
    setAcceptHoverEvents(true);
    // Scale with view zoom (canvas transform); do not use ItemIgnoresTransformations.
    setZValue(2);
    if (!m_label.isEmpty()) {
        setToolTip(QStringLiteral("%1 — %2").arg(portId, m_label));
    } else {
        setToolTip(portId);
    }
}

QPointF PortItem::anchorScenePos() const {
    return mapToScene(boundingRect().center());
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (m_type != Type::Output || !m_owner || !scene()) {
        QGraphicsEllipseItem::mousePressEvent(event);
        return;
    }

    m_dragConnection = new ConnectionItem(this, nullptr);
    m_owner->addConnection(m_dragConnection);
    scene()->addItem(m_dragConnection);
    m_dragConnection->setLooseEndPoint(event->scenePos());
    event->accept();
}

void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_dragConnection) {
        m_dragConnection->setLooseEndPoint(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsEllipseItem::mouseMoveEvent(event);
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (!m_dragConnection || !scene()) {
        QGraphicsEllipseItem::mouseReleaseEvent(event);
        return;
    }

    PortItem *targetPort = nullptr;
    const auto items = scene()->items(event->scenePos());
    for (QGraphicsItem *it : items) {
        if (it == this) {
            continue;
        }
        if (auto *p = dynamic_cast<PortItem *>(it)) {
            targetPort = p;
            break;
        }
    }

    if (targetPort && targetPort->portType() == Type::Input && targetPort->owner() && targetPort->owner() != m_owner) {
        DemoView *view = nullptr;
        const auto views = scene()->views();
        for (QGraphicsView *v : views) {
            if (auto *dv = dynamic_cast<DemoView *>(v)) {
                view = dv;
                break;
            }
        }
        if (view && view->undoStack()) {
            view->undoStack()->push(new AddConnectionCommand(scene(), this, targetPort));
        }
    }

    scene()->removeItem(m_dragConnection);
    m_owner->removeConnection(m_dragConnection);
    delete m_dragConnection;

    m_dragConnection = nullptr;
    event->accept();
}
