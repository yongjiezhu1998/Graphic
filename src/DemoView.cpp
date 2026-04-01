#include "DemoView.h"

#include <algorithm>

#include "AudioDrag.h"
#include "AudioTypes.h"
#include "Commands.h"
#include "ConnectionItem.h"
#include "ModuleItem.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QUndoStack>
#include <QAction>
#include <QInputDialog>
#include <QKeySequence>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QWheelEvent>

DemoView::DemoView(QGraphicsScene *scene, QWidget *parent, bool ownsScene)
    : QGraphicsView(scene, parent), m_ownsScene(ownsScene) {
    m_undoStack = new QUndoStack(this);

    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    // Keep anti-aliasing correction enabled for moving curved items.
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, false);
    setDragMode(QGraphicsView::RubberBandDrag);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(true);
}

DemoView::~DemoView() {
    // Destroy undo commands while scene items still exist; otherwise command destructors
    // may touch dangling ModuleItem/ConnectionItem pointers after the scene is gone.
    if (m_undoStack) {
        m_undoStack->clear();
    }
    if (m_ownsScene) {
        QGraphicsScene *s = scene();
        if (s) {
            setScene(nullptr);
            delete s;
        }
    }
}

static QString mimeModuleKind() {
    return QString::fromLatin1(AudioDrag::mimeModuleKind());
}

void DemoView::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat(mimeModuleKind())) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void DemoView::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasFormat(mimeModuleKind())) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void DemoView::addModuleFromDrop(AudioModuleKind kind, const QPointF &scenePos) {
    if (!m_undoStack || !scene()) {
        return;
    }
    QString name;
    if (kind == AudioModuleKind::Gain) {
        name = QStringLiteral("Gain %1").arg(m_moduleCounter++);
    }
    m_undoStack->push(new AddModuleCommand(scene(), scenePos, kind, name));
}

void DemoView::dropEvent(QDropEvent *event) {
    if (!event->mimeData()->hasFormat(mimeModuleKind())) {
        QGraphicsView::dropEvent(event);
        return;
    }
    const QByteArray raw = event->mimeData()->data(mimeModuleKind());
    bool ok = false;
    const AudioModuleKind kind = audioModuleKindFromString(QString::fromUtf8(raw), &ok);
    if (!ok) {
        event->ignore();
        return;
    }
    addModuleFromDrop(kind, mapToScene(event->pos()));
    event->acceptProposedAction();
}

void DemoView::setModuleParameterHandler(std::function<void(ModuleItem *)> handler) {
    m_moduleParamHandler = std::move(handler);
}

void DemoView::notifyModuleParameters(ModuleItem *module) {
    if (m_moduleParamHandler && module) {
        m_moduleParamHandler(module);
    }
}

void DemoView::mouseReleaseEvent(QMouseEvent *event) {
    QGraphicsView::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        ModuleItem::commitPendingMove(m_undoStack, scene());
    }
}

void DemoView::wheelEvent(QWheelEvent *event) {
    constexpr qreal scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        scale(scaleFactor, scaleFactor);
    } else {
        scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }
}

void DemoView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F2) {
        if (!scene() || !m_undoStack) {
            return;
        }
        ModuleItem *target = nullptr;
        const auto selected = scene()->selectedItems();
        for (QGraphicsItem *it : selected) {
            if (auto *m = dynamic_cast<ModuleItem *>(it)) {
                target = m;
                break;
            }
        }
        if (!target) {
            return;
        }

        const QString before = target->name();
        bool ok = false;
        const QString after =
            QInputDialog::getText(this, tr("Rename module"), tr("Name:"), QLineEdit::Normal, before, &ok).trimmed();
        if (ok && !after.isEmpty() && after != before) {
            m_undoStack->push(new RenameModuleCommand(target, before, after));
        }

        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete) {
        if (!scene() || !m_undoStack) {
            return;
        }

        const auto selected = scene()->selectedItems();
        if (selected.isEmpty()) {
            return;
        }
        m_undoStack->push(new DeleteItemsCommand(scene(), selected));

        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void DemoView::contextMenuEvent(QContextMenuEvent *event) {
    if (!scene()) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    const QPointF scenePos = mapToScene(event->pos());
    QMenu menu(this);
    QMenu *addMenu = menu.addMenu(tr("Add audio block"));
    addMenu->addAction(tr("Input"), this, [this, scenePos]() {
        if (m_undoStack) {
            m_undoStack->push(new AddModuleCommand(scene(), scenePos, AudioModuleKind::Input));
        }
    });
    addMenu->addAction(tr("Output"), this, [this, scenePos]() {
        if (m_undoStack) {
            m_undoStack->push(new AddModuleCommand(scene(), scenePos, AudioModuleKind::Output));
        }
    });
    addMenu->addAction(tr("Gain"), this, [this, scenePos]() {
        if (m_undoStack) {
            const QString name = QString("Gain %1").arg(m_moduleCounter++);
            m_undoStack->push(new AddModuleCommand(scene(), scenePos, AudioModuleKind::Gain, name));
        }
    });
    addMenu->addAction(tr("Biquad filter"), this, [this, scenePos]() {
        if (m_undoStack) {
            m_undoStack->push(new AddModuleCommand(scene(), scenePos, AudioModuleKind::BiquadFilter));
        }
    });
    addMenu->addAction(tr("Parametric EQ"), this, [this, scenePos]() {
        if (m_undoStack) {
            m_undoStack->push(new AddModuleCommand(scene(), scenePos, AudioModuleKind::ParametricEq));
        }
    });
    addMenu->addAction(tr("2-way crossover"), this, [this, scenePos]() {
        if (m_undoStack) {
            m_undoStack->push(new AddModuleCommand(scene(), scenePos, AudioModuleKind::Crossover2Way));
        }
    });
    addMenu->addAction(tr("3-way crossover"), this, [this, scenePos]() {
        if (m_undoStack) {
            m_undoStack->push(new AddModuleCommand(scene(), scenePos, AudioModuleKind::Crossover3Way));
        }
    });
    auto *deleteSelected = menu.addAction(tr("Delete selected"));

    const bool hasSelection = !scene()->selectedItems().isEmpty();
    deleteSelected->setEnabled(hasSelection);

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen) {
        event->accept();
        return;
    }

    if (chosen == deleteSelected) {
        // Reuse the same logic as Delete key.
        QKeyEvent del(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        keyPressEvent(&del);
        event->accept();
        return;
    }

    QGraphicsView::contextMenuEvent(event);
}

void DemoView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier) && scene() && m_undoStack) {
        // Only duplicate if clicking on a selected module.
        const QPointF scenePos = mapToScene(event->pos());
        QGraphicsItem *top = scene()->itemAt(scenePos, transform());
        if (auto *m = dynamic_cast<ModuleItem *>(top)) {
            if (m->isSelected()) {
                const auto selected = scene()->selectedItems();
                m_undoStack->push(new DuplicateSelectionCommand(scene(), selected));
                // Fall through to default handling so the newly created selection can be dragged immediately.
            }
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void DemoView::syncModuleCounterFromScene() {
    if (!scene()) {
        return;
    }
    int maxN = 0;
    const QRegularExpression re(QStringLiteral("^Module (\\d+)$"));
    for (QGraphicsItem *it : scene()->items()) {
        if (auto *m = dynamic_cast<ModuleItem *>(it)) {
            const QRegularExpressionMatch match = re.match(m->name());
            if (match.hasMatch()) {
                maxN = std::max(maxN, match.captured(1).toInt());
            }
        }
    }
    m_moduleCounter = maxN + 1;
}
