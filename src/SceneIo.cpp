#include "SceneIo.h"

#include "AudioTypes.h"
#include "ConnectionItem.h"
#include "ModuleItem.h"
#include "PortItem.h"

#include <algorithm>

#include <QFile>
#include <QGraphicsScene>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QCoreApplication>
#include <QUndoStack>

namespace {

constexpr const char *kFormatId = "DspTunerHostScene";
/** Legacy saves from the previous project name still load. */
constexpr const char *kFormatIdLegacy = "GraphicsDemoScene";
constexpr int kFormatVersionCurrent = 2;

static QJsonObject moduleStyleToJson(const ModuleItem::Style &s) {
    QJsonObject o;
    o["fill"] = s.fillColor.name(QColor::HexArgb);
    o["border"] = s.borderColor.name(QColor::HexArgb);
    o["borderWidth"] = s.borderWidth;
    o["cornerRadius"] = s.cornerRadius;
    return o;
}

static ModuleItem::Style moduleStyleFromJson(const QJsonObject &obj) {
    ModuleItem::Style s;
    if (obj.contains("fill")) {
        const QColor c(obj["fill"].toString());
        if (c.isValid()) {
            s.fillColor = c;
        }
    }
    if (obj.contains("border")) {
        const QColor c(obj["border"].toString());
        if (c.isValid()) {
            s.borderColor = c;
        }
    }
    if (obj.contains("borderWidth")) {
        s.borderWidth = std::max<qreal>(1.0, obj["borderWidth"].toDouble());
    }
    if (obj.contains("cornerRadius")) {
        s.cornerRadius = std::max<qreal>(0.0, obj["cornerRadius"].toDouble());
    }
    return s;
}

static QJsonObject paramsToJson(const AudioModuleParams &p) {
    QJsonObject o;
    o["gainDb"] = p.gainDb;
    o["crossoverLowHz"] = p.crossoverLowHz;
    o["crossoverHighHz"] = p.crossoverHighHz;
    o["filterFreqHz"] = p.filterFreqHz;
    o["filterQ"] = p.filterQ;
    o["filterType"] = p.filterType;
    QJsonArray eqF;
    QJsonArray eqG;
    QJsonArray eqQ;
    for (int i = 0; i < 3; ++i) {
        eqF.append(p.eqFreq[i]);
        eqG.append(p.eqGainDb[i]);
        eqQ.append(p.eqQ[i]);
    }
    o["eqFreq"] = eqF;
    o["eqGainDb"] = eqG;
    o["eqQ"] = eqQ;
    return o;
}

static AudioModuleParams paramsFromJson(const QJsonObject &obj) {
    AudioModuleParams p;
    if (obj.contains("gainDb")) {
        p.gainDb = obj["gainDb"].toDouble();
    }
    if (obj.contains("crossoverLowHz")) {
        p.crossoverLowHz = obj["crossoverLowHz"].toDouble();
    }
    if (obj.contains("crossoverHighHz")) {
        p.crossoverHighHz = obj["crossoverHighHz"].toDouble();
    }
    if (obj.contains("filterFreqHz")) {
        p.filterFreqHz = obj["filterFreqHz"].toDouble();
    }
    if (obj.contains("filterQ")) {
        p.filterQ = obj["filterQ"].toDouble();
    }
    if (obj.contains("filterType")) {
        p.filterType = obj["filterType"].toInt();
    }
    const QJsonArray eqF = obj["eqFreq"].toArray();
    const QJsonArray eqG = obj["eqGainDb"].toArray();
    const QJsonArray eqQ = obj["eqQ"].toArray();
    for (int i = 0; i < 3; ++i) {
        if (i < eqF.size()) {
            p.eqFreq[i] = eqF.at(i).toDouble();
        }
        if (i < eqG.size()) {
            p.eqGainDb[i] = eqG.at(i).toDouble();
        }
        if (i < eqQ.size()) {
            p.eqQ[i] = eqQ.at(i).toDouble();
        }
    }
    return p;
}

static QJsonObject connectionStyleToJson(const ConnectionItem::Style &s) {
    QJsonObject o;
    o["color"] = s.color.name(QColor::HexArgb);
    o["width"] = s.width;
    o["dashed"] = s.dashed;
    o["arrow"] = s.showArrow;
    return o;
}

static ConnectionItem::Style connectionStyleFromJson(const QJsonObject &obj) {
    ConnectionItem::Style s;
    if (obj.contains("color")) {
        const QColor c(obj["color"].toString());
        if (c.isValid()) {
            s.color = c;
        }
    }
    if (obj.contains("width")) {
        s.width = std::max<qreal>(1.0, obj["width"].toDouble());
    }
    if (obj.contains("dashed")) {
        s.dashed = obj["dashed"].toBool();
    }
    if (obj.contains("arrow")) {
        s.showArrow = obj["arrow"].toBool();
    }
    return s;
}

static QJsonObject buildSceneRootObject(QGraphicsScene *scene) {
    QJsonObject root;
    if (!scene) {
        return root;
    }

    QList<ModuleItem *> modules;
    QList<ConnectionItem *> connections;
    for (QGraphicsItem *it : scene->items()) {
        if (auto *m = dynamic_cast<ModuleItem *>(it)) {
            modules.push_back(m);
        } else if (auto *c = dynamic_cast<ConnectionItem *>(it)) {
            connections.push_back(c);
        }
    }

    root["format"] = kFormatId;
    root["version"] = kFormatVersionCurrent;

    const QRectF sr = scene->sceneRect();
    QJsonObject sceneRect;
    sceneRect["x"] = sr.x();
    sceneRect["y"] = sr.y();
    sceneRect["width"] = sr.width();
    sceneRect["height"] = sr.height();
    root["sceneRect"] = sceneRect;

    QJsonArray modArr;
    for (ModuleItem *m : modules) {
        QJsonObject mo;
        mo["id"] = m->instanceId();
        mo["name"] = m->name();
        mo["kind"] = audioModuleKindToString(m->kind());
        mo["params"] = paramsToJson(m->params());
        const QPointF p = m->pos();
        mo["x"] = p.x();
        mo["y"] = p.y();
        mo["width"] = m->rect().width();
        mo["height"] = m->rect().height();
        mo["style"] = moduleStyleToJson(m->style());
        modArr.append(mo);
    }
    root["modules"] = modArr;

    QJsonArray connArr;
    for (ConnectionItem *c : connections) {
        PortItem *fp = c->fromPort();
        PortItem *tp = c->toPort();
        ModuleItem *from = c->from();
        ModuleItem *to = c->to();
        if (!fp || !tp || !from || !to) {
            continue;
        }
        QJsonObject co;
        co["fromModule"] = from->instanceId();
        co["fromPort"] = fp->portId();
        co["toModule"] = to->instanceId();
        co["toPort"] = tp->portId();
        co["style"] = connectionStyleToJson(c->style());
        connArr.append(co);
    }
    root["connections"] = connArr;

    return root;
}

} // namespace

namespace SceneIo {

QByteArray sceneToJsonBytes(QGraphicsScene *scene, bool compact) {
    if (!scene) {
        return {};
    }
    const QJsonObject root = buildSceneRootObject(scene);
    const QJsonDocument doc(root);
    return doc.toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented);
}

bool saveScene(QGraphicsScene *scene, const QString &filePath, QString *errorMessage) {
    if (!scene) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate("SceneIo", "No scene.");
        }
        return false;
    }

    const QJsonObject root = buildSceneRootObject(scene);

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = f.errorString();
        }
        return false;
    }
    const QJsonDocument doc(root);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool loadScene(QGraphicsScene *scene, QUndoStack *undoStack, const QString &filePath, QString *errorMessage) {
    if (!scene) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate("SceneIo", "No scene.");
        }
        return false;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = f.errorString();
        }
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = parseErr.errorString();
        }
        return false;
    }

    const QJsonObject root = doc.object();
    const QString fmt = root["format"].toString();
    if (fmt != QLatin1String(kFormatId) && fmt != QLatin1String(kFormatIdLegacy)) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate("SceneIo", "Not a valid scene file.");
        }
        return false;
    }
    const int ver = root["version"].toInt(0);
    if (ver < 1 || ver > kFormatVersionCurrent) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate("SceneIo", "Unsupported file version.");
        }
        return false;
    }

    scene->clear();
    if (undoStack) {
        undoStack->clear();
    }

    if (root.contains("sceneRect")) {
        const QJsonObject sr = root["sceneRect"].toObject();
        scene->setSceneRect(sr["x"].toDouble(), sr["y"].toDouble(), sr["width"].toDouble(), sr["height"].toDouble());
    }

    const QJsonArray modArr = root["modules"].toArray();
    QHash<QString, ModuleItem *> idMap;
    idMap.reserve(modArr.size());

    for (const QJsonValue &v : modArr) {
        const QJsonObject mo = v.toObject();
        const QString id = mo["id"].toString();
        if (id.isEmpty()) {
            continue;
        }
        const QString name = mo["name"].toString();
        const double w = std::max(1.0, mo["width"].toDouble(140.0));
        const double h = std::max(1.0, mo["height"].toDouble(80.0));

        AudioModuleKind kind = AudioModuleKind::Gain;
        if (ver >= 2 && mo.contains("kind")) {
            bool ok = false;
            kind = audioModuleKindFromString(mo["kind"].toString(), &ok);
            if (!ok) {
                kind = AudioModuleKind::Gain;
            }
        }

        auto *m = new ModuleItem(kind, name.isEmpty() ? QString() : name, QSizeF(w, h));
        m->setInstanceId(id);
        if (ver >= 2 && mo.contains("params")) {
            m->setParams(paramsFromJson(mo["params"].toObject()));
        }
        m->setPos(mo["x"].toDouble(), mo["y"].toDouble());
        if (mo.contains("style")) {
            m->setStyle(moduleStyleFromJson(mo["style"].toObject()));
        }
        scene->addItem(m);
        idMap[id] = m;
    }

    const QJsonArray connArr = root["connections"].toArray();
    for (const QJsonValue &v : connArr) {
        const QJsonObject co = v.toObject();

        if (ver >= 2 && co.contains("fromModule")) {
            const QString fromId = co["fromModule"].toString();
            const QString toId = co["toModule"].toString();
            const QString fromPortId = co["fromPort"].toString();
            const QString toPortId = co["toPort"].toString();
            ModuleItem *from = idMap.value(fromId, nullptr);
            ModuleItem *to = idMap.value(toId, nullptr);
            if (!from || !to) {
                continue;
            }
            PortItem *fp = from->findPortById(fromPortId);
            PortItem *tp = to->findPortById(toPortId);
            if (!fp || !tp) {
                continue;
            }
            auto *c = new ConnectionItem(fp, tp);
            if (co.contains("style")) {
                c->setStyle(connectionStyleFromJson(co["style"].toObject()));
            }
            from->addConnection(c);
            to->addConnection(c);
            scene->addItem(c);
        } else {
            const QString fromId = co["from"].toString();
            const QString toId = co["to"].toString();
            ModuleItem *from = idMap.value(fromId, nullptr);
            ModuleItem *to = idMap.value(toId, nullptr);
            if (!from || !to) {
                continue;
            }
            PortItem *fp = from->findPortById(QStringLiteral("out"));
            PortItem *tp = to->findPortById(QStringLiteral("in"));
            if (!fp || !tp) {
                continue;
            }
            auto *c = new ConnectionItem(fp, tp);
            if (co.contains("style")) {
                c->setStyle(connectionStyleFromJson(co["style"].toObject()));
            }
            from->addConnection(c);
            to->addConnection(c);
            scene->addItem(c);
        }
    }

    return true;
}

} // namespace SceneIo
