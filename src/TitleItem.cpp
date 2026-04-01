#include "TitleItem.h"

#include "ModuleItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

TitleItem::TitleItem(ModuleItem *owner) : QGraphicsTextItem(owner), m_owner(owner) {
    setDefaultTextColor(Qt::white);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setZValue(3);
}

void TitleItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (m_owner) {
        m_owner->beginInlineRename();
        event->accept();
        return;
    }
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void TitleItem::focusOutEvent(QFocusEvent *event) {
    QGraphicsTextItem::focusOutEvent(event);
    if (m_owner) {
        m_owner->commitInlineRename(false);
    }
}

void TitleItem::keyPressEvent(QKeyEvent *event) {
    if (!m_owner) {
        QGraphicsTextItem::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        m_owner->commitInlineRename(false);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        m_owner->commitInlineRename(true);
        event->accept();
        return;
    }

    QGraphicsTextItem::keyPressEvent(event);
}

