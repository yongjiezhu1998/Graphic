#include "ModuleItem.h"

#include "Commands.h"
#include "ConnectionItem.h"
#include "DemoView.h"
#include "PortItem.h"
#include "TitleItem.h"

#include <QCoreApplication>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QCursor>
#include <QFont>
#include <QHash>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QPainter>
#include <QPen>
#include <QTextCursor>
#include <QUndoStack>
#include <QUuid>

#include <algorithm>

namespace {

QGraphicsScene *g_moveScene = nullptr;
QHash<ModuleItem *, QPointF> g_moveStart;
bool g_moveRecorded = false;

static bool scenePosNearlyEqual(const QPointF &a, const QPointF &b) {
    const QPointF d = a - b;
    return (d.x() * d.x() + d.y() * d.y()) < 1e-8;
}

} // namespace

static QString translateKind(AudioModuleKind k) {
    switch (k) {
    case AudioModuleKind::Input:
        return QCoreApplication::translate("ModuleItem", "Input");
    case AudioModuleKind::Output:
        return QCoreApplication::translate("ModuleItem", "Output");
    case AudioModuleKind::Gain:
        return QCoreApplication::translate("ModuleItem", "Gain");
    case AudioModuleKind::BiquadFilter:
        return QCoreApplication::translate("ModuleItem", "Biquad filter");
    case AudioModuleKind::ParametricEq:
        return QCoreApplication::translate("ModuleItem", "Parametric EQ");
    case AudioModuleKind::Crossover2Way:
        return QCoreApplication::translate("ModuleItem", "2-way crossover");
    case AudioModuleKind::Crossover3Way:
        return QCoreApplication::translate("ModuleItem", "3-way crossover");
    }
    return QCoreApplication::translate("ModuleItem", "Gain");
}

QSizeF ModuleItem::defaultSizeForKind(AudioModuleKind k) {
    switch (k) {
    case AudioModuleKind::ParametricEq:
    case AudioModuleKind::Crossover3Way:
        return QSizeF(190, 104);
    case AudioModuleKind::Crossover2Way:
        return QSizeF(180, 96);
    default:
        return QSizeF(168, 88);
    }
}

ModuleItem::Style ModuleItem::defaultStyleForKind(AudioModuleKind k) {
    Style s;
    switch (k) {
    case AudioModuleKind::Input:
        s.fillColor = QColor(33, 150, 243);
        break;
    case AudioModuleKind::Output:
        s.fillColor = QColor(156, 39, 176);
        break;
    case AudioModuleKind::Gain:
        s.fillColor = QColor(76, 175, 80);
        break;
    case AudioModuleKind::BiquadFilter:
        s.fillColor = QColor(255, 152, 0);
        break;
    case AudioModuleKind::ParametricEq:
        s.fillColor = QColor(0, 150, 136);
        break;
    case AudioModuleKind::Crossover2Way:
    case AudioModuleKind::Crossover3Way:
        s.fillColor = QColor(63, 81, 181);
        break;
    default:
        break;
    }
    return s;
}

QString ModuleItem::defaultTitleForKind(AudioModuleKind k) {
    return translateKind(k);
}

ModuleItem::ModuleItem(AudioModuleKind kind, const QString &name, const QSizeF &size)
    : QGraphicsRectItem(0, 0, 0, 0), m_kind(kind) {
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setCursor(QCursor(Qt::OpenHandCursor));

    m_instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QSizeF sz = (size.width() > 1.0 && size.height() > 1.0) ? size : defaultSizeForKind(kind);
    setRect(0, 0, sz.width(), sz.height());

    applyDefaultStyleForKind();
    buildPorts();

    m_title = new TitleItem(this);
    m_title->setPlainText(name.isEmpty() ? defaultTitleForKind(kind) : name);

    m_subtitle = new QGraphicsTextItem(this);
    m_subtitle->setDefaultTextColor(QColor(230, 230, 230));
    QFont f = m_subtitle->font();
    f.setPointSizeF(8.0);
    m_subtitle->setFont(f);
    m_subtitle->setAcceptedMouseButtons(Qt::NoButton);

    refreshSummary();
    layoutChildren();
}

void ModuleItem::buildPorts() {
    m_inputPorts.clear();
    m_outputPorts.clear();

    auto addIn = [this](const QString &id, const QString &label) {
        auto *p = new PortItem(PortItem::Type::Input, this, id, label);
        m_inputPorts.push_back(p);
    };
    auto addOut = [this](const QString &id, const QString &label) {
        auto *p = new PortItem(PortItem::Type::Output, this, id, label);
        m_outputPorts.push_back(p);
    };

    switch (m_kind) {
    case AudioModuleKind::Input:
        addOut(QStringLiteral("out"), QCoreApplication::translate("ModuleItem", "Out"));
        break;
    case AudioModuleKind::Output:
        addIn(QStringLiteral("in"), QCoreApplication::translate("ModuleItem", "In"));
        break;
    case AudioModuleKind::Gain:
    case AudioModuleKind::BiquadFilter:
    case AudioModuleKind::ParametricEq:
        addIn(QStringLiteral("in"), QCoreApplication::translate("ModuleItem", "In"));
        addOut(QStringLiteral("out"), QCoreApplication::translate("ModuleItem", "Out"));
        break;
    case AudioModuleKind::Crossover2Way:
        addIn(QStringLiteral("in"), QCoreApplication::translate("ModuleItem", "In"));
        addOut(QStringLiteral("low"), QCoreApplication::translate("ModuleItem", "Low"));
        addOut(QStringLiteral("high"), QCoreApplication::translate("ModuleItem", "High"));
        break;
    case AudioModuleKind::Crossover3Way:
        addIn(QStringLiteral("in"), QCoreApplication::translate("ModuleItem", "In"));
        addOut(QStringLiteral("low"), QCoreApplication::translate("ModuleItem", "Low"));
        addOut(QStringLiteral("mid"), QCoreApplication::translate("ModuleItem", "Mid"));
        addOut(QStringLiteral("high"), QCoreApplication::translate("ModuleItem", "High"));
        break;
    }
}

void ModuleItem::applyDefaultStyleForKind() {
    m_style = defaultStyleForKind(m_kind);
}

void ModuleItem::setParams(const AudioModuleParams &p) {
    m_params = p;
    refreshSummary();
}

void ModuleItem::refreshSummary() {
    if (!m_subtitle) {
        return;
    }
    QString s;
    switch (m_kind) {
    case AudioModuleKind::Input:
    case AudioModuleKind::Output:
        s = QCoreApplication::translate("ModuleItem", "Audio I/O");
        break;
    case AudioModuleKind::Gain:
        s = QCoreApplication::translate("ModuleItem", "%1 dB").arg(m_params.gainDb, 0, 'f', 1);
        break;
    case AudioModuleKind::BiquadFilter:
        s = QCoreApplication::translate("ModuleItem", "%1 @ %2 Hz, Q %3")
                .arg(audioFilterTypeName(m_params.filterType))
                .arg(m_params.filterFreqHz, 0, 'f', 0)
                .arg(m_params.filterQ, 0, 'f', 2);
        break;
    case AudioModuleKind::ParametricEq:
        s = QCoreApplication::translate("ModuleItem", "3 bands · %1 / %2 / %3 Hz")
                .arg(m_params.eqFreq[0], 0, 'f', 0)
                .arg(m_params.eqFreq[1], 0, 'f', 0)
                .arg(m_params.eqFreq[2], 0, 'f', 0);
        break;
    case AudioModuleKind::Crossover2Way:
        s = QCoreApplication::translate("ModuleItem", "Split @ %1 Hz").arg(m_params.crossoverLowHz, 0, 'f', 0);
        break;
    case AudioModuleKind::Crossover3Way:
        s = QCoreApplication::translate("ModuleItem", "Low/Mid %1 Hz · Mid/High %2 Hz")
                .arg(m_params.crossoverLowHz, 0, 'f', 0)
                .arg(m_params.crossoverHighHz, 0, 'f', 0);
        break;
    }
    m_subtitle->setPlainText(s);
    layoutChildren();
}

QString ModuleItem::instanceId() const {
    return m_instanceId;
}

void ModuleItem::setInstanceId(const QString &id) {
    if (!id.isEmpty()) {
        m_instanceId = id;
    }
}

void ModuleItem::layoutChildren() {
    constexpr qreal r = 6.0;
    if (m_title) {
        const QRectF textRect = m_title->boundingRect();
        m_title->setPos((rect().width() - textRect.width()) * 0.5, 6);
    }
    if (m_subtitle) {
        const qreal y = m_title ? (m_title->pos().y() + m_title->boundingRect().height() + 2) : 6;
        const QRectF sr = m_subtitle->boundingRect();
        m_subtitle->setPos((rect().width() - sr.width()) * 0.5, y);
    }

    // Ports: evenly along the full left / right edge (not only below the title block).
    constexpr qreal vmargin = 8.0;
    const qreal top = vmargin;
    const qreal bottom = rect().height() - vmargin;
    const qreal span = std::max(1.0, bottom - top);

    auto placeVertical = [&](int index, int count) -> qreal {
        if (count <= 0) {
            return (top + bottom) * 0.5;
        }
        if (count == 1) {
            return (top + bottom) * 0.5;
        }
        return top + (index + 1) * span / (count + 1);
    };

    for (int i = 0; i < m_inputPorts.size(); ++i) {
        PortItem *p = m_inputPorts[i];
        const qreal y = placeVertical(i, m_inputPorts.size());
        p->setPos(-r, y - r);
    }
    for (int i = 0; i < m_outputPorts.size(); ++i) {
        PortItem *p = m_outputPorts[i];
        const qreal y = placeVertical(i, m_outputPorts.size());
        p->setPos(rect().right() - r, y - r);
    }
}

void ModuleItem::setBodySize(const QSizeF &size) {
    prepareGeometryChange();
    setRect(0, 0, size.width(), size.height());
    layoutChildren();
}

void ModuleItem::setStyle(const Style &style) {
    m_style = style;
    update();
}

void ModuleItem::setFillColor(const QColor &c) {
    m_style.fillColor = c;
    update();
}

void ModuleItem::setBorderColor(const QColor &c) {
    m_style.borderColor = c;
    update();
}

void ModuleItem::setBorderWidth(qreal w) {
    m_style.borderWidth = std::max<qreal>(1.0, w);
    update();
}

void ModuleItem::setCornerRadius(qreal r) {
    m_style.cornerRadius = std::max<qreal>(0.0, r);
    update();
}

QString ModuleItem::name() const {
    return m_title ? m_title->toPlainText() : QString();
}

void ModuleItem::setName(const QString &name) {
    if (!m_title) {
        return;
    }
    m_title->setPlainText(name);
    layoutChildren();
}

static QUndoStack *findUndoStackFromItem(QGraphicsItem *item) {
    if (!item || !item->scene()) {
        return nullptr;
    }
    for (QGraphicsView *v : item->scene()->views()) {
        if (auto *dv = dynamic_cast<DemoView *>(v)) {
            return dv->undoStack();
        }
    }
    return nullptr;
}

static DemoView *findDemoView(QGraphicsItem *item) {
    if (!item || !item->scene()) {
        return nullptr;
    }
    for (QGraphicsView *v : item->scene()->views()) {
        if (auto *dv = dynamic_cast<DemoView *>(v)) {
            return dv;
        }
    }
    return nullptr;
}

void ModuleItem::beginInlineRename() {
    if (!m_title) {
        return;
    }
    m_oldNameBeforeEdit = m_title->toPlainText();
    m_title->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_title->setFocus(Qt::MouseFocusReason);
    QTextCursor cursor = m_title->textCursor();
    cursor.select(QTextCursor::Document);
    m_title->setTextCursor(cursor);
}

void ModuleItem::commitInlineRename(bool revert) {
    if (!m_title) {
        return;
    }

    const QString before = m_oldNameBeforeEdit;
    QString after = m_title->toPlainText().trimmed();
    if (after.isEmpty()) {
        after = before;
    }

    m_title->setTextInteractionFlags(Qt::NoTextInteraction);
    clearFocus();

    if (revert || before == after) {
        setName(before);
        return;
    }

    if (auto *stack = findUndoStackFromItem(this)) {
        stack->push(new RenameModuleCommand(this, before, after));
    } else {
        setName(after);
    }
}

void ModuleItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (m_title) {
        const QPointF localPos = event->pos();
        const QRectF titleRectInParent = m_title->boundingRect().translated(m_title->pos());
        if (titleRectInParent.contains(localPos)) {
            beginInlineRename();
            event->accept();
            return;
        }
    }
    if (auto *dv = findDemoView(this)) {
        dv->notifyModuleParameters(this);
        event->accept();
        return;
    }
    QGraphicsRectItem::mouseDoubleClickEvent(event);
}

PortItem *ModuleItem::findPortById(const QString &portId) const {
    for (PortItem *p : m_inputPorts) {
        if (p && p->portId() == portId) {
            return p;
        }
    }
    for (PortItem *p : m_outputPorts) {
        if (p && p->portId() == portId) {
            return p;
        }
    }
    return nullptr;
}

void ModuleItem::addConnection(ConnectionItem *connection) {
    if (!connection) {
        return;
    }
    if (!m_connections.contains(connection)) {
        m_connections.push_back(connection);
    }
}

void ModuleItem::removeConnection(ConnectionItem *connection) {
    m_connections.removeAll(connection);
}

const QVector<ConnectionItem *> &ModuleItem::connections() const {
    return m_connections;
}

QVariant ModuleItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionChange && scene() && (QApplication::mouseButtons() & Qt::LeftButton)) {
        if (!g_moveRecorded) {
            g_moveScene = scene();
            g_moveStart.clear();
            for (QGraphicsItem *gi : scene()->selectedItems()) {
                if (auto *m = dynamic_cast<ModuleItem *>(gi)) {
                    g_moveStart[m] = m->scenePos();
                }
            }
            g_moveRecorded = true;
        }
    }
    if (change == ItemPositionHasChanged) {
        for (ConnectionItem *connection : m_connections) {
            if (connection) {
                connection->updatePath();
            }
        }
    }
    return QGraphicsRectItem::itemChange(change, value);
}

void ModuleItem::commitPendingMove(QUndoStack *stack, QGraphicsScene *scene) {
    if (!stack || !scene) {
        g_moveRecorded = false;
        g_moveStart.clear();
        g_moveScene = nullptr;
        return;
    }
    if (!g_moveRecorded || g_moveScene != scene) {
        g_moveRecorded = false;
        g_moveStart.clear();
        g_moveScene = nullptr;
        return;
    }

    QHash<ModuleItem *, QPointF> end;
    for (auto it = g_moveStart.constBegin(); it != g_moveStart.constEnd(); ++it) {
        ModuleItem *m = it.key();
        if (m && m->scene() == scene) {
            end[m] = m->scenePos();
        }
    }

    bool changed = false;
    for (auto it = g_moveStart.constBegin(); it != g_moveStart.constEnd(); ++it) {
        ModuleItem *m = it.key();
        if (!m || m->scene() != scene) {
            continue;
        }
        if (!scenePosNearlyEqual(it.value(), end.value(m))) {
            changed = true;
            break;
        }
    }

    if (changed) {
        stack->push(new MoveItemsCommand(scene, g_moveStart, end));
    }

    g_moveRecorded = false;
    g_moveStart.clear();
    g_moveScene = nullptr;
}

void ModuleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen border(m_style.borderColor, m_style.borderWidth);
    border.setJoinStyle(Qt::RoundJoin);
    painter->setPen(border);
    painter->setBrush(m_style.fillColor);
    painter->drawRoundedRect(rect(), m_style.cornerRadius, m_style.cornerRadius);

    if (isSelected()) {
        painter->setPen(QPen(QColor(255, 193, 7), 2, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rect().adjusted(2, 2, -2, -2), m_style.cornerRadius, m_style.cornerRadius);
    }
}
