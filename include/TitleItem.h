#pragma once

#include <QGraphicsTextItem>

class ModuleItem;
class QFocusEvent;
class QGraphicsSceneMouseEvent;
class QKeyEvent;

class TitleItem : public QGraphicsTextItem {
public:
    explicit TitleItem(ModuleItem *owner);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    ModuleItem *m_owner = nullptr;
};

