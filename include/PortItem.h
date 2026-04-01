#pragma once

#include <QGraphicsEllipseItem>
#include <QString>

class ConnectionItem;
class ModuleItem;
class QGraphicsSceneMouseEvent;

class PortItem : public QGraphicsEllipseItem {
public:
    enum class Type { Input, Output };

    PortItem(Type type, ModuleItem *owner, const QString &portId, const QString &label = QString());

    Type portType() const { return m_type; }
    ModuleItem *owner() const { return m_owner; }
    QString portId() const { return m_portId; }
    QString label() const { return m_label; }

    QPointF anchorScenePos() const;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    Type m_type;
    ModuleItem *m_owner;
    QString m_portId;
    QString m_label;
    ConnectionItem *m_dragConnection = nullptr;
};
