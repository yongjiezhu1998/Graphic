#include "ConnectionItem.h"

#include "ModuleItem.h"
#include "PortItem.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

ConnectionItem::ConnectionItem(PortItem *fromPort, PortItem *toPort) : m_fromPort(fromPort), m_toPort(toPort) {
    setFlags(ItemIsSelectable);
    setZValue(-1);
    if (m_fromPort) {
        m_looseEndPoint = m_fromPort->anchorScenePos();
    }
    updatePath();
}

void ConnectionItem::setStyle(const Style &style) {
    m_style = style;
    updatePath();
    update();
}

void ConnectionItem::setColor(const QColor &c) {
    m_style.color = c;
    updatePath();
    update();
}

void ConnectionItem::setWidth(qreal w) {
    m_style.width = std::max<qreal>(1.0, w);
    updatePath();
    update();
}

void ConnectionItem::setDashed(bool dashed) {
    m_style.dashed = dashed;
    updatePath();
    update();
}

void ConnectionItem::setShowArrow(bool show) {
    m_style.showArrow = show;
    update();
}

void ConnectionItem::setToPort(PortItem *toPort) {
    m_toPort = toPort;
    updatePath();
}

void ConnectionItem::setLooseEndPoint(const QPointF &scenePos) {
    m_looseEndPoint = scenePos;
    updatePath();
}

ModuleItem *ConnectionItem::from() const {
    return m_fromPort ? m_fromPort->owner() : nullptr;
}

ModuleItem *ConnectionItem::to() const {
    return m_toPort ? m_toPort->owner() : nullptr;
}

void ConnectionItem::updatePath() {
    if (!m_fromPort) {
        return;
    }

    const QPointF p1 = m_fromPort->anchorScenePos();
    const QPointF p2 = m_toPort ? m_toPort->anchorScenePos() : m_looseEndPoint;

    QPainterPath path;
    path.moveTo(p1);
    ModuleItem *fromMod = from();
    ModuleItem *toMod = to();
    if (m_toPort && fromMod && toMod && fromMod == toMod) {
        const qreal dx = std::abs(p2.x() - p1.x());
        const qreal ctrlX = std::max<qreal>(80.0, dx * 0.8);
        const qreal loopHeight = std::max<qreal>(90.0, dx * 0.6);

        const QPointF c1(p1.x() + ctrlX, p1.y() - loopHeight);
        const QPointF c2(p2.x() - ctrlX, p2.y() - loopHeight);
        path.cubicTo(c1, c2, p2);
    } else {
        const qreal dx = std::abs(p2.x() - p1.x());
        const qreal ctrlOffset = std::max<qreal>(40.0, dx * 0.45);
        path.cubicTo(QPointF(p1.x() + ctrlOffset, p1.y()), QPointF(p2.x() - ctrlOffset, p2.y()), p2);
    }
    setPath(path);

    QPen linePen(m_style.color, m_style.width);
    linePen.setStyle(m_style.dashed ? Qt::DashLine : Qt::SolidLine);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    setPen(linePen);
}

QRectF ConnectionItem::boundingRect() const {
    const qreal extra = std::max<qreal>(16.0, m_style.width + 16.0);
    return QGraphicsPathItem::boundingRect().adjusted(-extra, -extra, extra, extra);
}

void ConnectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen drawPen = pen();
    if (isSelected()) {
        drawPen.setColor(QColor(255, 87, 34));
        drawPen.setWidthF(std::max<qreal>(drawPen.widthF(), m_style.width + 1.0));
    }
    painter->setPen(drawPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    if (!m_style.showArrow) {
        return;
    }
    const QPainterPath arrowPath = path();
    const qreal length = arrowPath.length();
    if (length < 1.0) {
        return;
    }

    const qreal t = arrowPath.percentAtLength(length);
    const qreal tPrev = arrowPath.percentAtLength(std::max<qreal>(0.0, length - 10.0));
    const QPointF end = arrowPath.pointAtPercent(t);
    const QPointF prev = arrowPath.pointAtPercent(tPrev);

    const qreal angle = std::atan2(end.y() - prev.y(), end.x() - prev.x());
    constexpr qreal arrowSize = 8.0;
    constexpr qreal pi = 3.14159265358979323846;
    QPolygonF arrow;
    arrow << end
          << QPointF(end.x() - arrowSize * std::cos(angle - pi / 6.0),
                     end.y() - arrowSize * std::sin(angle - pi / 6.0))
          << QPointF(end.x() - arrowSize * std::cos(angle + pi / 6.0),
                     end.y() - arrowSize * std::sin(angle + pi / 6.0));

    painter->setBrush(drawPen.color());
    painter->setPen(Qt::NoPen);
    painter->drawPolygon(arrow);
}
