#pragma once

#include <QGraphicsPathItem>
#include <QColor>
#include <QPointF>
#include <QRectF>

class ModuleItem;
class PortItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class ConnectionItem : public QGraphicsPathItem {
public:
    struct Style {
        QColor color = QColor(66, 66, 66);
        qreal width = 2.0;
        bool dashed = false;
        bool showArrow = true;
    };

    ConnectionItem(PortItem *fromPort, PortItem *toPort);
    void updatePath();
    void setToPort(PortItem *toPort);
    void setLooseEndPoint(const QPointF &scenePos);

    PortItem *fromPort() const { return m_fromPort; }
    PortItem *toPort() const { return m_toPort; }

    ModuleItem *from() const;
    ModuleItem *to() const;

    Style style() const { return m_style; }
    void setStyle(const Style &style);
    void setColor(const QColor &c);
    void setWidth(qreal w);
    void setDashed(bool dashed);
    void setShowArrow(bool show);

protected:
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    PortItem *m_fromPort = nullptr;
    PortItem *m_toPort = nullptr;
    QPointF m_looseEndPoint;
    Style m_style;
};
