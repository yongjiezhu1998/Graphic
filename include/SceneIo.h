#pragma once

#include <QByteArray>
class QGraphicsScene;
class QUndoStack;
class QString;

namespace SceneIo {

bool saveScene(QGraphicsScene *scene, const QString &filePath, QString *errorMessage = nullptr);
bool loadScene(QGraphicsScene *scene, QUndoStack *undoStack, const QString &filePath, QString *errorMessage = nullptr);

/** Same JSON as saved scene file; compact recommended for network/serial. */
QByteArray sceneToJsonBytes(QGraphicsScene *scene, bool compact = true);

} // namespace SceneIo
