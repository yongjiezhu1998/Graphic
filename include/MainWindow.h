#pragma once

#include <QMainWindow>

class DemoView;
class ModuleItem;
class ModuleTreeWidget;
class QDockWidget;
class QGraphicsScene;
class QTabWidget;
class QTranslator;
class QUndoGroup;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createMenus();
    void updateActionStates();
    void switchLanguage(const QString &localeName);
    void retranslateUi();
    void installTranslatorForCurrentLocale();

    DemoView *currentDemoView() const;
    void updateWindowTitle();
    void updateTabTitleForView(DemoView *view);
    QString tabDisplayName(const DemoView *view) const;

    bool promptSaveIfDirty(DemoView *view);
    DemoView *addDocumentTab(bool withDemoContent);

    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);

    void actionNewTab();
    void actionCloseTab();
    void actionOpenScene();
    void actionOpenSceneInNewTab();
    void actionSaveScene();
    void actionSaveAsScene();

    void editModuleParameters(ModuleItem *module);
    void actionModuleParameters();
    void actionUndoHistory();

    enum class AlignOp { Left, Right, Top, Bottom, HCenter, VCenter, DistH, DistV };
    void applyAlignOrDistribute(AlignOp op);

    void actionSendPatchTcp();
    void actionSendPatchSerial();

    void actionModuleFillColor();
    void actionModuleBorderColor();
    void actionModuleBorderWidth();
    void actionModuleCornerRadius();

    void actionConnectionColor();
    void actionConnectionWidth();
    void actionConnectionToggleDashed();
    void actionConnectionToggleArrow();

    QTabWidget *m_tabWidget = nullptr;
    QUndoGroup *m_undoGroup = nullptr;
    QDockWidget *m_moduleDock = nullptr;
    ModuleTreeWidget *m_moduleTree = nullptr;

    QAction *m_fileNewTab = nullptr;
    QAction *m_fileCloseTab = nullptr;
    QAction *m_fileOpen = nullptr;
    QAction *m_fileOpenInNewTab = nullptr;
    QAction *m_fileSave = nullptr;
    QAction *m_fileSaveAs = nullptr;

    QAction *m_moduleFillColor = nullptr;
    QAction *m_moduleBorderColor = nullptr;
    QAction *m_moduleBorderWidth = nullptr;
    QAction *m_moduleCornerRadius = nullptr;

    QAction *m_connColor = nullptr;
    QAction *m_connWidth = nullptr;
    QAction *m_connToggleDashed = nullptr;
    QAction *m_connToggleArrow = nullptr;

    QMenu *m_fileMenu = nullptr;

    QMenu *m_deviceMenu = nullptr;
    QMenu *m_editMenu = nullptr;
    QAction *m_editParameters = nullptr;
    QAction *m_editUndoHistory = nullptr;
    QMenu *m_editAlignMenu = nullptr;
    QMenu *m_moduleMenu = nullptr;
    QMenu *m_connMenu = nullptr;
    QMenu *m_langMenu = nullptr;

    QAction *m_langEnglish = nullptr;
    QAction *m_langChinese = nullptr;

    QTranslator *m_translator = nullptr;
    QString m_localeName = "en_US";
};
