#pragma once

#include "AudioTypes.h"

#include <QGraphicsView>
#include <QUndoStack>

#include <functional>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;

class ModuleItem;

class DemoView : public QGraphicsView {
    Q_OBJECT
public:
    explicit DemoView(QGraphicsScene *scene, QWidget *parent = nullptr, bool ownsScene = false);
    ~DemoView() override;

    QUndoStack *undoStack() const { return m_undoStack; }

    /** Empty if never saved (untitled). */
    QString documentPath() const { return m_documentPath; }
    void setDocumentPath(const QString &path) { m_documentPath = path; }

    void syncModuleCounterFromScene();

    void setModuleParameterHandler(std::function<void(ModuleItem *)> handler);
    void notifyModuleParameters(ModuleItem *module);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void addModuleFromDrop(AudioModuleKind kind, const QPointF &scenePos);
    int m_moduleCounter = 1;
    QUndoStack *m_undoStack = nullptr;
    bool m_ownsScene = false;
    QString m_documentPath;
    std::function<void(ModuleItem *)> m_moduleParamHandler;
};
