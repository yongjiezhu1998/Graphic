#pragma once

#include "AudioTypes.h"

#include <QGraphicsRectItem>
#include <QColor>
#include <QVector>

class ConnectionItem;
class PortItem;
class QPainter;
class QGraphicsTextItem;
class QStyleOptionGraphicsItem;
class QWidget;
class TitleItem;
class QGraphicsSceneMouseEvent;
class QUndoStack;

class ModuleItem : public QGraphicsRectItem {
public:
    struct Style {
        QColor fillColor = QColor(76, 175, 80);
        QColor borderColor = QColor(40, 40, 40);
        qreal borderWidth = 2.0;
        qreal cornerRadius = 10.0;
    };

    explicit ModuleItem(AudioModuleKind kind, const QString &name = QString(), const QSizeF &size = QSizeF());

    QString instanceId() const;
    void setInstanceId(const QString &id);

    AudioModuleKind kind() const { return m_kind; }
    AudioModuleParams params() const { return m_params; }
    void setParams(const AudioModuleParams &p);

    QString name() const;
    void setName(const QString &name);

    Style style() const { return m_style; }
    void setStyle(const Style &style);
    void setFillColor(const QColor &c);
    void setBorderColor(const QColor &c);
    void setBorderWidth(qreal w);
    void setCornerRadius(qreal r);
    void setBodySize(const QSizeF &size);
    void beginInlineRename();
    void commitInlineRename(bool revert);

    PortItem *findPortById(const QString &portId) const;
    void addConnection(ConnectionItem *connection);
    void removeConnection(ConnectionItem *connection);
    const QVector<ConnectionItem *> &connections() const;

    void refreshSummary();

    /** Call from the view on left-button release: records one MoveItemsCommand if the mouse drag moved modules. */
    static void commitPendingMove(QUndoStack *stack, QGraphicsScene *scene);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void layoutChildren();
    void buildPorts();
    void applyDefaultStyleForKind();
    static QSizeF defaultSizeForKind(AudioModuleKind k);
    static Style defaultStyleForKind(AudioModuleKind k);
    static QString defaultTitleForKind(AudioModuleKind k);

    QVector<ConnectionItem *> m_connections;
    QVector<PortItem *> m_inputPorts;
    QVector<PortItem *> m_outputPorts;
    TitleItem *m_title = nullptr;
    QGraphicsTextItem *m_subtitle = nullptr;
    QString m_instanceId;
    QString m_oldNameBeforeEdit;
    AudioModuleKind m_kind = AudioModuleKind::Gain;
    AudioModuleParams m_params;
    Style m_style;
};
