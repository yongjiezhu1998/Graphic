#include "ModuleTreeWidget.h"

#include "AudioDrag.h"
#include "AudioTypes.h"

#include <QDrag>
#include <QMimeData>
#include <QTreeWidgetItem>

ModuleTreeWidget::ModuleTreeWidget(QWidget *parent) : QTreeWidget(parent) {
    setHeaderHidden(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setIndentation(16);
    populateTree();
}

void ModuleTreeWidget::rebuildTree() {
    clear();
    populateTree();
}

void ModuleTreeWidget::addLeaf(QTreeWidgetItem *parent, const QString &label, const QByteArray &kindPayload) {
    auto *item = new QTreeWidgetItem(parent);
    item->setText(0, label);
    item->setData(0, Qt::UserRole, kindPayload);
    item->setFlags((item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled) &
                   ~Qt::ItemIsDropEnabled);
}

void ModuleTreeWidget::populateTree() {
    auto mkCat = [this](const QString &title) {
        auto *cat = new QTreeWidgetItem(this);
        cat->setText(0, title);
        cat->setFlags(Qt::ItemIsEnabled);
        return cat;
    };

    QTreeWidgetItem *io = mkCat(tr("Audio I/O"));
    addLeaf(io, tr("Input"), audioModuleKindToString(AudioModuleKind::Input).toUtf8());
    addLeaf(io, tr("Output"), audioModuleKindToString(AudioModuleKind::Output).toUtf8());

    QTreeWidgetItem *dyn = mkCat(tr("Dynamics"));
    addLeaf(dyn, tr("Gain"), audioModuleKindToString(AudioModuleKind::Gain).toUtf8());

    QTreeWidgetItem *flt = mkCat(tr("Filters"));
    addLeaf(flt, tr("Biquad filter"), audioModuleKindToString(AudioModuleKind::BiquadFilter).toUtf8());

    QTreeWidgetItem *eq = mkCat(tr("EQ"));
    addLeaf(eq, tr("Parametric EQ"), audioModuleKindToString(AudioModuleKind::ParametricEq).toUtf8());

    QTreeWidgetItem *xo = mkCat(tr("Crossover"));
    addLeaf(xo, tr("2-way crossover"), audioModuleKindToString(AudioModuleKind::Crossover2Way).toUtf8());
    addLeaf(xo, tr("3-way crossover"), audioModuleKindToString(AudioModuleKind::Crossover3Way).toUtf8());

    expandAll();
}

void ModuleTreeWidget::startDrag(Qt::DropActions supportedActions) {
    QTreeWidgetItem *item = currentItem();
    if (!item) {
        QTreeWidget::startDrag(supportedActions);
        return;
    }
    const QVariant v = item->data(0, Qt::UserRole);
    if (!v.isValid()) {
        return;
    }

    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(AudioDrag::mimeModuleKind()), v.toByteArray());

    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
}
