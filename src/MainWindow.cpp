#include "MainWindow.h"

#include "AudioPropertiesDialog.h"
#include "AudioTypes.h"
#include "Commands.h"
#include "ConnectionItem.h"
#include "DemoView.h"
#include "ModuleItem.h"
#include "ModuleTreeWidget.h"
#include "PatchTransport.h"
#include "PortItem.h"
#include "SceneIo.h"

#include <algorithm>
#include <limits>

#include <QAction>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QGraphicsScene>
#include <QInputDialog>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QActionGroup>
#include <QLocale>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#ifdef GRAPHICS_DEMO_HAS_SERIAL
#include <QSerialPortInfo>
#endif
#include <QSettings>
#include <QTabWidget>
#include <QUndoView>
#include <QTranslator>
#include <QUndoGroup>
#include <QUndoStack>

namespace {

void populateDemoScene(QGraphicsScene *scene) {
    scene->setSceneRect(-480, -300, 1280, 600);

    constexpr qreal yChain = 0;
    auto *inBlk = new ModuleItem(AudioModuleKind::Input);
    inBlk->setPos(-360, yChain);
    scene->addItem(inBlk);

    auto *gain = new ModuleItem(AudioModuleKind::Gain, QStringLiteral("Trim"));
    gain->setPos(-136, yChain);
    scene->addItem(gain);

    auto *flt = new ModuleItem(AudioModuleKind::BiquadFilter);
    flt->setPos(88, yChain);
    scene->addItem(flt);

    auto *eq = new ModuleItem(AudioModuleKind::ParametricEq);
    eq->setPos(312, yChain);
    scene->addItem(eq);

    auto *xo = new ModuleItem(AudioModuleKind::Crossover2Way);
    xo->setPos(536, yChain);
    scene->addItem(xo);

    auto *outLow = new ModuleItem(AudioModuleKind::Output, QStringLiteral("Amp Low"));
    outLow->setPos(780, -100);
    scene->addItem(outLow);

    auto *outHigh = new ModuleItem(AudioModuleKind::Output, QStringLiteral("Amp High"));
    outHigh->setPos(780, 100);
    scene->addItem(outHigh);

    struct Link {
        ModuleItem *a;
        QString ap;
        ModuleItem *b;
        QString bp;
    };
    const Link links[] = {
        {inBlk, QStringLiteral("out"), gain, QStringLiteral("in")},
        {gain, QStringLiteral("out"), flt, QStringLiteral("in")},
        {flt, QStringLiteral("out"), eq, QStringLiteral("in")},
        {eq, QStringLiteral("out"), xo, QStringLiteral("in")},
        {xo, QStringLiteral("low"), outLow, QStringLiteral("in")},
        {xo, QStringLiteral("high"), outHigh, QStringLiteral("in")},
    };

    for (const Link &lk : links) {
        PortItem *pa = lk.a->findPortById(lk.ap);
        PortItem *pb = lk.b->findPortById(lk.bp);
        if (!pa || !pb) {
            continue;
        }
        auto *line = new ConnectionItem(pa, pb);
        lk.a->addConnection(line);
        lk.b->addConnection(line);
        scene->addItem(line);
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QSettings settings("GraphicsDemo", "GraphicsDemo");
    const QString saved = settings.value("language").toString();
    if (!saved.isEmpty()) {
        m_localeName = saved;
    } else {
        const QString sys = QLocale::system().name();
        m_localeName = sys.startsWith("zh", Qt::CaseInsensitive) ? "zh_CN" : "en_US";
    }
    installTranslatorForCurrentLocale();

    m_undoGroup = new QUndoGroup(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    QObject::connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    QObject::connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    m_moduleDock = new QDockWidget(tr("Module library"), this);
    m_moduleDock->setObjectName(QStringLiteral("ModuleLibraryDock"));
    m_moduleTree = new ModuleTreeWidget(this);
    m_moduleDock->setWidget(m_moduleTree);
    m_moduleDock->setMinimumWidth(220);
    addDockWidget(Qt::LeftDockWidgetArea, m_moduleDock);

    setCentralWidget(m_tabWidget);
    resize(1100, 700);

    addDocumentTab(true);

    createMenus();
    updateWindowTitle();
    updateActionStates();
}

void MainWindow::createMenus() {
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileNewTab = m_fileMenu->addAction(tr("New &tab"), this, &MainWindow::actionNewTab);
    m_fileNewTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    m_fileOpen = m_fileMenu->addAction(tr("&Open..."), this, &MainWindow::actionOpenScene);
    m_fileOpen->setShortcut(QKeySequence::Open);
    m_fileOpenInNewTab = m_fileMenu->addAction(tr("Open in new &tab..."), this, &MainWindow::actionOpenSceneInNewTab);
    m_fileOpenInNewTab->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    m_fileSave = m_fileMenu->addAction(tr("&Save"), this, &MainWindow::actionSaveScene);
    m_fileSave->setShortcut(QKeySequence::Save);
    m_fileSaveAs = m_fileMenu->addAction(tr("Save &As..."), this, &MainWindow::actionSaveAsScene);
    m_fileSaveAs->setShortcut(QKeySequence::SaveAs);
    m_fileCloseTab = m_fileMenu->addAction(tr("&Close tab"), this, &MainWindow::actionCloseTab);
    m_fileCloseTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    m_fileMenu->addSeparator();

    m_deviceMenu = menuBar()->addMenu(tr("&Device"));
    m_deviceMenu->addAction(tr("Send patch via &TCP..."), this, &MainWindow::actionSendPatchTcp);
    m_deviceMenu->addAction(tr("Send patch via &Serial..."), this, &MainWindow::actionSendPatchSerial);

    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    {
        auto *a = m_undoGroup->createUndoAction(this, tr("Undo"));
        a->setShortcut(QKeySequence::Undo);
        m_editMenu->addAction(a);
    }
    {
        auto *a = m_undoGroup->createRedoAction(this, tr("Redo"));
        a->setShortcut(QKeySequence::Redo);
        m_editMenu->addAction(a);
    }
    m_editMenu->addSeparator();
    m_editParameters = m_editMenu->addAction(tr("Module &parameters..."), this, &MainWindow::actionModuleParameters);
    m_editParameters->setShortcut(QKeySequence(Qt::Key_F4));
    m_editUndoHistory = m_editMenu->addAction(tr("Undo stack &history..."), this, &MainWindow::actionUndoHistory);
    m_editMenu->addSeparator();
    m_editAlignMenu = m_editMenu->addMenu(tr("Align && distribute"));
    m_editAlignMenu->addAction(tr("Align &left"), this, [this]() { applyAlignOrDistribute(AlignOp::Left); });
    m_editAlignMenu->addAction(tr("Align &right"), this, [this]() { applyAlignOrDistribute(AlignOp::Right); });
    m_editAlignMenu->addAction(tr("Align &top"), this, [this]() { applyAlignOrDistribute(AlignOp::Top); });
    m_editAlignMenu->addAction(tr("Align &bottom"), this, [this]() { applyAlignOrDistribute(AlignOp::Bottom); });
    m_editAlignMenu->addSeparator();
    m_editAlignMenu->addAction(tr("Align &horizontal centers"), this, [this]() { applyAlignOrDistribute(AlignOp::HCenter); });
    m_editAlignMenu->addAction(tr("Align &vertical centers"), this, [this]() { applyAlignOrDistribute(AlignOp::VCenter); });
    m_editAlignMenu->addSeparator();
    m_editAlignMenu->addAction(tr("Distribute &horizontally"), this, [this]() { applyAlignOrDistribute(AlignOp::DistH); });
    m_editAlignMenu->addAction(tr("Distribute &vertically"), this, [this]() { applyAlignOrDistribute(AlignOp::DistV); });

    m_moduleMenu = menuBar()->addMenu(tr("&Module Style"));
    m_moduleFillColor = m_moduleMenu->addAction(tr("Fill color..."), this, &MainWindow::actionModuleFillColor);
    m_moduleBorderColor = m_moduleMenu->addAction(tr("Border color..."), this, &MainWindow::actionModuleBorderColor);
    m_moduleBorderWidth = m_moduleMenu->addAction(tr("Border width..."), this, &MainWindow::actionModuleBorderWidth);
    m_moduleCornerRadius = m_moduleMenu->addAction(tr("Corner radius..."), this, &MainWindow::actionModuleCornerRadius);

    m_connMenu = menuBar()->addMenu(tr("&Connection Style"));
    m_connColor = m_connMenu->addAction(tr("Color..."), this, &MainWindow::actionConnectionColor);
    m_connWidth = m_connMenu->addAction(tr("Width..."), this, &MainWindow::actionConnectionWidth);
    m_connToggleDashed = m_connMenu->addAction(tr("Toggle dashed"), this, &MainWindow::actionConnectionToggleDashed);
    m_connToggleArrow = m_connMenu->addAction(tr("Toggle arrow"), this, &MainWindow::actionConnectionToggleArrow);

    m_langMenu = menuBar()->addMenu(tr("&Language"));
    m_langEnglish = m_langMenu->addAction(tr("English"));
    m_langEnglish->setCheckable(true);
    m_langChinese = m_langMenu->addAction(tr("中文"));
    m_langChinese->setCheckable(true);
    auto *langGroup = new QActionGroup(this);
    langGroup->setExclusive(true);
    m_langEnglish->setActionGroup(langGroup);
    m_langChinese->setActionGroup(langGroup);

    m_langEnglish->setChecked(m_localeName != "zh_CN");
    m_langChinese->setChecked(m_localeName == "zh_CN");

    connect(m_langEnglish, &QAction::triggered, this, [this]() { switchLanguage("en_US"); });
    connect(m_langChinese, &QAction::triggered, this, [this]() { switchLanguage("zh_CN"); });
}

static QList<ModuleItem *> selectedModules(QGraphicsScene *scene) {
    QList<ModuleItem *> out;
    if (!scene) {
        return out;
    }
    for (QGraphicsItem *it : scene->selectedItems()) {
        if (auto *m = dynamic_cast<ModuleItem *>(it)) {
            out.push_back(m);
        }
    }
    return out;
}

static QList<ConnectionItem *> selectedConnections(QGraphicsScene *scene) {
    QList<ConnectionItem *> out;
    if (!scene) {
        return out;
    }
    for (QGraphicsItem *it : scene->selectedItems()) {
        if (auto *c = dynamic_cast<ConnectionItem *>(it)) {
            out.push_back(c);
        }
    }
    return out;
}

namespace {

bool movePositionsDiffer(const QHash<ModuleItem *, QPointF> &a, const QHash<ModuleItem *, QPointF> &b) {
    for (auto it = a.constBegin(); it != a.constEnd(); ++it) {
        ModuleItem *m = it.key();
        if (!b.contains(m)) {
            continue;
        }
        const QPointF d = b.value(m) - it.value();
        if (d.x() * d.x() + d.y() * d.y() > 1e-12) {
            return true;
        }
    }
    return false;
}

void pushMoveCommand(QGraphicsScene *scene, QUndoStack *stack, const QHash<ModuleItem *, QPointF> &start,
                     const QHash<ModuleItem *, QPointF> &end, const QString &text) {
    if (!scene || !stack || !movePositionsDiffer(start, end)) {
        return;
    }
    stack->push(new MoveItemsCommand(scene, start, end, text));
}

} // namespace

DemoView *MainWindow::currentDemoView() const {
    return qobject_cast<DemoView *>(m_tabWidget ? m_tabWidget->currentWidget() : nullptr);
}

QString MainWindow::tabDisplayName(const DemoView *view) const {
    if (!view) {
        return tr("Untitled");
    }
    if (view->documentPath().isEmpty()) {
        return tr("Untitled");
    }
    return QFileInfo(view->documentPath()).fileName();
}

void MainWindow::updateTabTitleForView(DemoView *view) {
    if (!m_tabWidget || !view) {
        return;
    }
    const int idx = m_tabWidget->indexOf(view);
    if (idx < 0) {
        return;
    }
    QString name = tabDisplayName(view);
    if (!view->undoStack()->isClean()) {
        name += QLatin1Char('*');
    }
    m_tabWidget->setTabText(idx, name);
}

bool MainWindow::promptSaveIfDirty(DemoView *view) {
    if (!view || view->undoStack()->isClean()) {
        return true;
    }
    const auto ret =
        QMessageBox::question(this, tr("Save changes?"), tr("Save changes to \"%1\"?").arg(tabDisplayName(view)),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (ret == QMessageBox::Cancel) {
        return false;
    }
    if (ret == QMessageBox::Discard) {
        return true;
    }
    QString path = view->documentPath();
    if (path.isEmpty()) {
        QString start = QDir::homePath() + QStringLiteral("/untitled.json");
        path = QFileDialog::getSaveFileName(this, tr("Save scene"), start,
                                            tr("GraphicsDemo JSON (*.json);;All Files (*)"));
        if (path.isEmpty()) {
            return false;
        }
        QString err;
        if (!SceneIo::saveScene(view->scene(), path, &err)) {
            QMessageBox::warning(this, tr("Save scene"), err);
            return false;
        }
        view->setDocumentPath(path);
        view->undoStack()->setClean();
        updateTabTitleForView(view);
    } else {
        QString err;
        if (!SceneIo::saveScene(view->scene(), path, &err)) {
            QMessageBox::warning(this, tr("Save scene"), err);
            return false;
        }
        view->undoStack()->setClean();
        updateTabTitleForView(view);
    }
    updateWindowTitle();
    return true;
}

DemoView *MainWindow::addDocumentTab(bool withDemoContent) {
    auto *scene = new QGraphicsScene;
    scene->setSceneRect(-480, -300, 1280, 600);
    if (withDemoContent) {
        populateDemoScene(scene);
    }
    auto *view = new DemoView(scene, m_tabWidget, true);
    view->setDocumentPath(QString());
    view->setModuleParameterHandler([this](ModuleItem *m) { editModuleParameters(m); });

    QObject::connect(scene, &QGraphicsScene::selectionChanged, this, [this, scene]() {
        DemoView *v = currentDemoView();
        if (v && v->scene() == scene) {
            updateActionStates();
        }
    });
    QObject::connect(view->undoStack(), &QUndoStack::cleanChanged, this, [this, view]() { updateTabTitleForView(view); });

    m_undoGroup->addStack(view->undoStack());

    const int idx = m_tabWidget->addTab(view, QString());
    m_tabWidget->setCurrentIndex(idx);
    updateTabTitleForView(view);
    m_undoGroup->setActiveStack(view->undoStack());
    view->syncModuleCounterFromScene();
    if (withDemoContent) {
        view->undoStack()->setClean();
    }
    return view;
}

void MainWindow::onCurrentTabChanged(int index) {
    Q_UNUSED(index);
    DemoView *v = currentDemoView();
    if (v) {
        m_undoGroup->setActiveStack(v->undoStack());
    }
    updateWindowTitle();
    updateActionStates();
}

void MainWindow::onTabCloseRequested(int index) {
    QWidget *w = m_tabWidget->widget(index);
    auto *view = qobject_cast<DemoView *>(w);
    if (!view) {
        return;
    }
    if (!promptSaveIfDirty(view)) {
        return;
    }
    m_undoGroup->removeStack(view->undoStack());
    m_tabWidget->removeTab(index);
    delete view;

    if (m_tabWidget->count() == 0) {
        addDocumentTab(false);
    }
    updateWindowTitle();
    updateActionStates();
}

void MainWindow::actionNewTab() {
    addDocumentTab(false);
}

void MainWindow::actionCloseTab() {
    if (!m_tabWidget) {
        return;
    }
    const int idx = m_tabWidget->currentIndex();
    if (idx < 0) {
        return;
    }
    onTabCloseRequested(idx);
}

void MainWindow::actionOpenSceneInNewTab() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open scene"), QString(),
                                                    tr("GraphicsDemo JSON (*.json);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }
    DemoView *view = addDocumentTab(false);
    QString err;
    if (!SceneIo::loadScene(view->scene(), view->undoStack(), path, &err)) {
        QMessageBox::warning(this, tr("Open scene"), err);
        const int i = m_tabWidget->indexOf(view);
        if (i >= 0) {
            m_undoGroup->removeStack(view->undoStack());
            m_tabWidget->removeTab(i);
            delete view;
        }
        if (m_tabWidget->count() == 0) {
            addDocumentTab(false);
        }
        return;
    }
    view->setDocumentPath(path);
    view->undoStack()->setClean();
    view->syncModuleCounterFromScene();
    updateTabTitleForView(view);
    updateWindowTitle();
}

void MainWindow::updateActionStates() {
    DemoView *v = currentDemoView();
    QGraphicsScene *s = v ? v->scene() : nullptr;
    const bool hasModuleSelection = !selectedModules(s).isEmpty();
    const bool hasConnectionSelection = !selectedConnections(s).isEmpty();
    const bool multiModuleSelection = selectedModules(s).size() >= 2;

    if (m_editAlignMenu) {
        m_editAlignMenu->setEnabled(multiModuleSelection);
    }

    if (m_moduleFillColor) {
        m_moduleFillColor->setEnabled(hasModuleSelection);
    }
    if (m_moduleBorderColor) {
        m_moduleBorderColor->setEnabled(hasModuleSelection);
    }
    if (m_moduleBorderWidth) {
        m_moduleBorderWidth->setEnabled(hasModuleSelection);
    }
    if (m_moduleCornerRadius) {
        m_moduleCornerRadius->setEnabled(hasModuleSelection);
    }

    if (m_connColor) {
        m_connColor->setEnabled(hasConnectionSelection);
    }
    if (m_connWidth) {
        m_connWidth->setEnabled(hasConnectionSelection);
    }
    if (m_connToggleDashed) {
        m_connToggleDashed->setEnabled(hasConnectionSelection);
    }
    if (m_connToggleArrow) {
        m_connToggleArrow->setEnabled(hasConnectionSelection);
    }
    if (m_editParameters) {
        const auto mods = selectedModules(s);
        bool canEdit = false;
        if (mods.size() == 1) {
            const AudioModuleKind k = mods.first()->kind();
            canEdit = (k != AudioModuleKind::Input && k != AudioModuleKind::Output);
        }
        m_editParameters->setEnabled(canEdit);
    }
}

void MainWindow::switchLanguage(const QString &localeName) {
    m_localeName = localeName;
    QSettings settings("GraphicsDemo", "GraphicsDemo");
    settings.setValue("language", m_localeName);

    // Uninstall previous translator
    if (m_translator) {
        qApp->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    installTranslatorForCurrentLocale();

    retranslateUi();
}

void MainWindow::installTranslatorForCurrentLocale() {
    if (m_localeName != "zh_CN") {
        return;
    }

    // Load from next to the executable: ./i18n/GraphicsDemo_zh_CN.qm
    const QString baseDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(baseDir).filePath("i18n/GraphicsDemo_zh_CN.qm"),
        QDir(baseDir).filePath("GraphicsDemo_zh_CN.qm"),
        QDir(baseDir).filePath("../GraphicsDemo_zh_CN.qm"),
    };

    for (const QString &qmPath : candidates) {
        auto *t = new QTranslator(this);
        if (t->load(qmPath)) {
            qApp->installTranslator(t);
            m_translator = t;
            return;
        }
        delete t;
    }
}

void MainWindow::retranslateUi() {
    // Rebuild menus to update all texts in one place.
    menuBar()->clear();
    m_fileMenu = nullptr;
    m_fileNewTab = nullptr;
    m_fileCloseTab = nullptr;
    m_fileOpen = nullptr;
    m_fileOpenInNewTab = nullptr;
    m_fileSave = nullptr;
    m_fileSaveAs = nullptr;
    m_deviceMenu = nullptr;
    m_editMenu = nullptr;
    m_editParameters = nullptr;
    m_editUndoHistory = nullptr;
    m_editAlignMenu = nullptr;
    m_moduleMenu = nullptr;
    m_connMenu = nullptr;
    m_langMenu = nullptr;

    m_moduleFillColor = nullptr;
    m_moduleBorderColor = nullptr;
    m_moduleBorderWidth = nullptr;
    m_moduleCornerRadius = nullptr;

    m_connColor = nullptr;
    m_connWidth = nullptr;
    m_connToggleDashed = nullptr;
    m_connToggleArrow = nullptr;

    m_langEnglish = nullptr;
    m_langChinese = nullptr;

    createMenus();
    updateWindowTitle();
    if (m_moduleDock) {
        m_moduleDock->setWindowTitle(tr("Module library"));
    }
    if (m_moduleTree) {
        m_moduleTree->rebuildTree();
    }
    if (m_tabWidget) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (auto *dv = qobject_cast<DemoView *>(m_tabWidget->widget(i))) {
                updateTabTitleForView(dv);
            }
        }
    }
    updateActionStates();
}

void MainWindow::updateWindowTitle() {
    const QString base = tr("Audio patch editor");
    DemoView *v = currentDemoView();
    if (v && !v->documentPath().isEmpty()) {
        QFileInfo fi(v->documentPath());
        setWindowTitle(QString("%1 - %2").arg(fi.fileName(), base));
    } else {
        setWindowTitle(base);
    }
}

void MainWindow::actionOpenScene() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    if (!promptSaveIfDirty(v)) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Open scene"), QString(),
                                                      tr("GraphicsDemo JSON (*.json);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!SceneIo::loadScene(v->scene(), v->undoStack(), path, &err)) {
        QMessageBox::warning(this, tr("Open scene"), err);
        return;
    }
    v->setDocumentPath(path);
    v->undoStack()->setClean();
    v->syncModuleCounterFromScene();
    updateTabTitleForView(v);
    updateWindowTitle();
}

void MainWindow::actionSaveScene() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    if (v->documentPath().isEmpty()) {
        actionSaveAsScene();
        return;
    }
    QString err;
    if (!SceneIo::saveScene(v->scene(), v->documentPath(), &err)) {
        QMessageBox::warning(this, tr("Save scene"), err);
        return;
    }
    v->undoStack()->setClean();
    updateTabTitleForView(v);
}

void MainWindow::actionSaveAsScene() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    QString start = v->documentPath();
    if (start.isEmpty()) {
        start = QDir::homePath() + QStringLiteral("/untitled.json");
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save scene"), start,
                                                      tr("GraphicsDemo JSON (*.json);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!SceneIo::saveScene(v->scene(), path, &err)) {
        QMessageBox::warning(this, tr("Save scene"), err);
        return;
    }
    v->setDocumentPath(path);
    v->undoStack()->setClean();
    updateTabTitleForView(v);
    updateWindowTitle();
}

void MainWindow::editModuleParameters(ModuleItem *module) {
    if (!module) {
        return;
    }
    const AudioModuleKind k = module->kind();
    if (k == AudioModuleKind::Input || k == AudioModuleKind::Output) {
        return;
    }
    AudioModuleParams p = module->params();
    if (AudioPropertiesDialog::editParams(this, k, &p)) {
        module->setParams(p);
    }
}

void MainWindow::actionModuleParameters() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto mods = selectedModules(v->scene());
    if (mods.size() != 1) {
        return;
    }
    editModuleParameters(mods.first());
}

void MainWindow::actionUndoHistory() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Undo stack"));
    auto *layout = new QVBoxLayout(&dlg);
    auto *view = new QUndoView(&dlg);
    view->setGroup(m_undoGroup);
    view->setEmptyLabel(tr("No undo steps"));
    view->setAlternatingRowColors(true);
    layout->addWidget(view);
    auto *closeBtn = new QPushButton(tr("Close"));
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);
    dlg.resize(420, 360);
    dlg.exec();
}

void MainWindow::applyAlignOrDistribute(AlignOp op) {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    QGraphicsScene *scene = v->scene();
    QList<ModuleItem *> mods = selectedModules(scene);
    if (mods.size() < 2) {
        return;
    }

    QHash<ModuleItem *, QPointF> start;
    QHash<ModuleItem *, QRectF> rects;
    for (ModuleItem *m : mods) {
        start[m] = m->scenePos();
        rects[m] = m->sceneBoundingRect();
    }

    QHash<ModuleItem *, QPointF> end = start;

    switch (op) {
    case AlignOp::Left: {
        qreal minL = std::numeric_limits<qreal>::max();
        for (ModuleItem *m : mods) {
            minL = std::min(minL, rects[m].left());
        }
        for (ModuleItem *m : mods) {
            const qreal dx = minL - rects[m].left();
            end[m] = start[m] + QPointF(dx, 0);
        }
        pushMoveCommand(scene, v->undoStack(), start, end, tr("Align left"));
        break;
    }
    case AlignOp::Right: {
        qreal maxR = std::numeric_limits<qreal>::lowest();
        for (ModuleItem *m : mods) {
            maxR = std::max(maxR, rects[m].right());
        }
        for (ModuleItem *m : mods) {
            const qreal dx = maxR - rects[m].right();
            end[m] = start[m] + QPointF(dx, 0);
        }
        pushMoveCommand(scene, v->undoStack(), start, end, tr("Align right"));
        break;
    }
    case AlignOp::Top: {
        qreal minT = std::numeric_limits<qreal>::max();
        for (ModuleItem *m : mods) {
            minT = std::min(minT, rects[m].top());
        }
        for (ModuleItem *m : mods) {
            const qreal dy = minT - rects[m].top();
            end[m] = start[m] + QPointF(0, dy);
        }
        pushMoveCommand(scene, v->undoStack(), start, end, tr("Align top"));
        break;
    }
    case AlignOp::Bottom: {
        qreal maxB = std::numeric_limits<qreal>::lowest();
        for (ModuleItem *m : mods) {
            maxB = std::max(maxB, rects[m].bottom());
        }
        for (ModuleItem *m : mods) {
            const qreal dy = maxB - rects[m].bottom();
            end[m] = start[m] + QPointF(0, dy);
        }
        pushMoveCommand(scene, v->undoStack(), start, end, tr("Align bottom"));
        break;
    }
    case AlignOp::HCenter: {
        QRectF u;
        for (ModuleItem *m : mods) {
            u |= rects[m];
        }
        const qreal cx = u.center().x();
        for (ModuleItem *m : mods) {
            const qreal dx = cx - rects[m].center().x();
            end[m] = start[m] + QPointF(dx, 0);
        }
        pushMoveCommand(scene, v->undoStack(), start, end, tr("Align horizontal centers"));
        break;
    }
    case AlignOp::VCenter: {
        QRectF u;
        for (ModuleItem *m : mods) {
            u |= rects[m];
        }
        const qreal cy = u.center().y();
        for (ModuleItem *m : mods) {
            const qreal dy = cy - rects[m].center().y();
            end[m] = start[m] + QPointF(0, dy);
        }
        pushMoveCommand(scene, v->undoStack(), start, end, tr("Align vertical centers"));
        break;
    }
    case AlignOp::DistH: {
        QList<ModuleItem *> sorted = mods;
        std::sort(sorted.begin(), sorted.end(), [&rects](ModuleItem *a, ModuleItem *b) {
            return rects[a].center().x() < rects[b].center().x();
        });
        const qreal leftEdge = rects[sorted.first()].left();
        const qreal rightEdge = rects[sorted.last()].right();
        qreal sumW = 0;
        for (ModuleItem *m : sorted) {
            sumW += rects[m].width();
        }
        const qreal span = rightEdge - leftEdge;
        const int n = sorted.size();
        qreal gap = (n > 1) ? (span - sumW) / (n - 1) : 0;
        if (gap < 0) {
            gap = 0;
        }
        qreal x = leftEdge;
        for (int i = 0; i < n; ++i) {
            ModuleItem *m = sorted[i];
            const qreal w = rects[m].width();
            const qreal targetLeft = x;
            const qreal dx = targetLeft - rects[m].left();
            end[m] = start[m] + QPointF(dx, 0);
            x += w + gap;
        }
        pushMoveCommand(scene, v->undoStack(), start, end, tr("Distribute horizontally"));
        break;
    }
    case AlignOp::DistV: {
        QList<ModuleItem *> sorted = mods;
        std::sort(sorted.begin(), sorted.end(), [&rects](ModuleItem *a, ModuleItem *b) {
            return rects[a].center().y() < rects[b].center().y();
        });
        const qreal topEdge = rects[sorted.first()].top();
        const qreal bottomEdge = rects[sorted.last()].bottom();
        qreal sumH = 0;
        for (ModuleItem *m : sorted) {
            sumH += rects[m].height();
        }
        const qreal span = bottomEdge - topEdge;
        const int n = sorted.size();
        qreal gap = (n > 1) ? (span - sumH) / (n - 1) : 0;
        if (gap < 0) {
            gap = 0;
        }
        qreal y = topEdge;
        for (int i = 0; i < n; ++i) {
            ModuleItem *m = sorted[i];
            const qreal h = rects[m].height();
            const qreal targetTop = y;
            const qreal dy = targetTop - rects[m].top();
            end[m] = start[m] + QPointF(0, dy);
            y += h + gap;
        }
        pushMoveCommand(scene, v->undoStack(), start, end, tr("Distribute vertically"));
        break;
    }
    }
}

void MainWindow::actionModuleFillColor() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto *scene = v->scene();
    auto mods = selectedModules(scene);
    if (mods.isEmpty()) {
        return;
    }

    const QColor c = QColorDialog::getColor(mods.first()->style().fillColor, this, tr("Fill color"));
    if (!c.isValid()) {
        return;
    }

    auto *stack = v->undoStack();
    stack->beginMacro(tr("Change module fill color"));
    for (ModuleItem *m : mods) {
        const auto before = m->style();
        auto after = before;
        after.fillColor = c;
        stack->push(new ChangeModuleStyleCommand(m, before, after));
    }
    stack->endMacro();
}

void MainWindow::actionModuleBorderColor() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto *scene = v->scene();
    auto mods = selectedModules(scene);
    if (mods.isEmpty()) {
        return;
    }

    const QColor c = QColorDialog::getColor(mods.first()->style().borderColor, this, tr("Border color"));
    if (!c.isValid()) {
        return;
    }

    auto *stack = v->undoStack();
    stack->beginMacro(tr("Change module border color"));
    for (ModuleItem *m : mods) {
        const auto before = m->style();
        auto after = before;
        after.borderColor = c;
        stack->push(new ChangeModuleStyleCommand(m, before, after));
    }
    stack->endMacro();
}

void MainWindow::actionModuleBorderWidth() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto *scene = v->scene();
    auto mods = selectedModules(scene);
    if (mods.isEmpty()) {
        return;
    }

    bool ok = false;
    const double w = QInputDialog::getDouble(this, tr("Border width"), tr("Width:"), mods.first()->style().borderWidth, 1.0,
                                             20.0, 1, &ok);
    if (!ok) {
        return;
    }

    auto *stack = v->undoStack();
    stack->beginMacro(tr("Change module border width"));
    for (ModuleItem *m : mods) {
        const auto before = m->style();
        auto after = before;
        after.borderWidth = w;
        stack->push(new ChangeModuleStyleCommand(m, before, after));
    }
    stack->endMacro();
}

void MainWindow::actionModuleCornerRadius() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto *scene = v->scene();
    auto mods = selectedModules(scene);
    if (mods.isEmpty()) {
        return;
    }

    bool ok = false;
    const double r = QInputDialog::getDouble(this, tr("Corner radius"), tr("Radius:"), mods.first()->style().cornerRadius, 0.0,
                                             60.0, 1, &ok);
    if (!ok) {
        return;
    }

    auto *stack = v->undoStack();
    stack->beginMacro(tr("Change module corner radius"));
    for (ModuleItem *m : mods) {
        const auto before = m->style();
        auto after = before;
        after.cornerRadius = r;
        stack->push(new ChangeModuleStyleCommand(m, before, after));
    }
    stack->endMacro();
}

void MainWindow::actionConnectionColor() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto *scene = v->scene();
    auto conns = selectedConnections(scene);
    if (conns.isEmpty()) {
        return;
    }

    const QColor c = QColorDialog::getColor(conns.first()->style().color, this, tr("Connection color"));
    if (!c.isValid()) {
        return;
    }

    auto *stack = v->undoStack();
    stack->beginMacro(tr("Change connection color"));
    for (ConnectionItem *conn : conns) {
        const auto before = conn->style();
        auto after = before;
        after.color = c;
        stack->push(new ChangeConnectionStyleCommand(conn, before, after));
    }
    stack->endMacro();
}

void MainWindow::actionConnectionWidth() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto *scene = v->scene();
    auto conns = selectedConnections(scene);
    if (conns.isEmpty()) {
        return;
    }

    bool ok = false;
    const double w =
        QInputDialog::getDouble(this, tr("Connection width"), tr("Width:"), conns.first()->style().width, 1.0, 20.0, 1, &ok);
    if (!ok) {
        return;
    }

    auto *stack = v->undoStack();
    stack->beginMacro(tr("Change connection width"));
    for (ConnectionItem *conn : conns) {
        const auto before = conn->style();
        auto after = before;
        after.width = w;
        stack->push(new ChangeConnectionStyleCommand(conn, before, after));
    }
    stack->endMacro();
}

void MainWindow::actionConnectionToggleDashed() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto *scene = v->scene();
    auto conns = selectedConnections(scene);
    if (conns.isEmpty()) {
        return;
    }

    auto *stack = v->undoStack();
    stack->beginMacro(tr("Toggle dashed"));
    for (ConnectionItem *conn : conns) {
        const auto before = conn->style();
        auto after = before;
        after.dashed = !before.dashed;
        stack->push(new ChangeConnectionStyleCommand(conn, before, after));
    }
    stack->endMacro();
}

void MainWindow::actionConnectionToggleArrow() {
    DemoView *v = currentDemoView();
    if (!v) {
        return;
    }
    auto *scene = v->scene();
    auto conns = selectedConnections(scene);
    if (conns.isEmpty()) {
        return;
    }

    auto *stack = v->undoStack();
    stack->beginMacro(tr("Toggle arrow"));
    for (ConnectionItem *conn : conns) {
        const auto before = conn->style();
        auto after = before;
        after.showArrow = !before.showArrow;
        stack->push(new ChangeConnectionStyleCommand(conn, before, after));
    }
    stack->endMacro();
}

void MainWindow::actionSendPatchTcp() {
    DemoView *v = currentDemoView();
    QGraphicsScene *s = v ? v->scene() : nullptr;
    if (!s) {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Send patch via TCP"));

    auto *hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"));
    auto *portSpin = new QSpinBox;
    portSpin->setRange(1, 65535);
    portSpin->setValue(9753);

    auto *form = new QFormLayout;
    form->addRow(tr("Host:"), hostEdit);
    form->addRow(tr("Port:"), portSpin);

    auto *hint =
        new QLabel(tr("Sends compact UTF-8 JSON (same schema as scene files) over TCP, followed by a newline. "
                      "Use for a local service controlling the sound card."));
    hint->setWordWrap(true);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(box);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QByteArray payload = SceneIo::sceneToJsonBytes(s, true);
    QString err;
    if (!PatchTransport::sendTcp(hostEdit->text(), static_cast<quint16>(portSpin->value()), payload, &err)) {
        QMessageBox::warning(this, tr("Send failed"), err);
    } else {
        QMessageBox::information(this, tr("Send patch"), tr("Patch sent successfully."));
    }
}

void MainWindow::actionSendPatchSerial() {
    if (!PatchTransport::serialSupported()) {
        QMessageBox::information(
            this, tr("Send patch via Serial"),
            tr("Serial port is not available in this build.\n\n"
               "On Debian/Ubuntu, install the development package matching your Qt, then reconfigure and rebuild:\n"
               "  Qt 6: sudo apt install qt6-serialport-dev\n"
               "  Qt 5: sudo apt install libqt5serialport5-dev\n\n"
               "Then run: cmake -S . -B build && cmake --build build"));
        return;
    }

    DemoView *v = currentDemoView();
    QGraphicsScene *s = v ? v->scene() : nullptr;
    if (!s) {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Send patch via Serial"));

    auto *portCombo = new QComboBox;
    auto *refreshBtn = new QPushButton(tr("Refresh"));
    auto *row = new QHBoxLayout;
    row->addWidget(portCombo, 1);
    row->addWidget(refreshBtn);

    auto *baudCombo = new QComboBox;
    const QList<int> bauds = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    for (int b : bauds) {
        baudCombo->addItem(QString::number(b), b);
    }
    baudCombo->setCurrentIndex(4);

    auto refreshPorts = [portCombo]() {
        portCombo->clear();
#ifdef GRAPHICS_DEMO_HAS_SERIAL
        const auto ports = QSerialPortInfo::availablePorts();
        if (ports.isEmpty()) {
            portCombo->addItem(QStringLiteral("—"), QString());
            return;
        }
        for (const QSerialPortInfo &info : ports) {
            portCombo->addItem(info.portName() + QStringLiteral(" — ") + info.description(), info.portName());
        }
#endif
    };
    refreshPorts();
    QObject::connect(refreshBtn, &QPushButton::clicked, refreshPorts);

    auto *form = new QFormLayout;
    form->addRow(tr("Port:"), row);
    form->addRow(tr("Baud rate:"), baudCombo);

    auto *hint =
        new QLabel(tr("Sends compact UTF-8 JSON (same schema as scene files) over UART, followed by a newline. "
                      "Use for embedded DSP / MCU."));
    hint->setWordWrap(true);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(box);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString portName = portCombo->currentData().toString();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, tr("Send patch via Serial"), tr("No serial port selected."));
        return;
    }

    const QByteArray payload = SceneIo::sceneToJsonBytes(s, true);
    QString err;
    if (!PatchTransport::sendSerial(portName, baudCombo->currentData().toInt(), payload, &err)) {
        if (err.contains(QLatin1String("Permission denied"), Qt::CaseInsensitive)) {
            err += QLatin1Char('\n');
            err += tr("On Linux, serial devices (e.g. /dev/ttyUSB0) are usually owned by group \"dialout\". "
                      "Add your user to that group, then log out and back in:\n"
                      "  sudo usermod -aG dialout $USER");
        }
        QMessageBox::warning(this, tr("Send failed"), err);
    } else {
        QMessageBox::information(this, tr("Send patch"), tr("Patch sent successfully."));
    }
}

