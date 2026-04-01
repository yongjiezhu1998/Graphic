#pragma once

#include <QTreeWidget>

class ModuleTreeWidget : public QTreeWidget {
public:
    explicit ModuleTreeWidget(QWidget *parent = nullptr);

    void rebuildTree();

protected:
    void startDrag(Qt::DropActions supportedActions) override;

private:
    void populateTree();
    static void addLeaf(QTreeWidgetItem *parent, const QString &label, const QByteArray &kindPayload);
};
