#include <QtTest/QtTest>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUrl>

#include "services/TabManager.h"
#include "ui/FileBrowserWidget.h"
#include "ui/TabStrip.h"

class UrlOpenHandler : public QObject {
    Q_OBJECT

public slots:
    void openUrl(const QUrl &url) {
        openedUrls.append(url);
    }

public:
    QList<QUrl> openedUrls;
};

class FileBrowserWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void defaultRestoreCreatesHomeTab();
    void newTabButtonCreatesHomeTab();
    void restoresSavedTabs();
    void setCurrentPathUpdatesViewAndEmits();
    void addressBarNavigatesToExistingDirectoryOnly();
    void upButtonNavigatesToParentDirectory();
    void addressBarUsesModernNavigationChrome();
    void doubleClickingFolderChangesCurrentPath();
    void doubleClickingFileOpensDesktopUrl();
    void selectingEntryEmitsSelectedPath();
    void enablesDragAndDropFileOperations();
    void switchesBetweenDetailsListAndTilesViews();
    void viewModeSwitchPreservesCurrentPath();
    void viewModeButtonsChangeActiveView();
    void showsBreadcrumbByDefaultAndPathEditorOnCtrlL();
    void breadcrumbButtonNavigatesToAncestor();
    void escapeLeavesPathEditModeWithoutNavigation();
};

void FileBrowserWidgetTest::defaultRestoreCreatesHomeTab() {
    QTabWidget tabs;
    TabManager manager;
    manager.setTabWidget(&tabs);

    manager.restoreTabs(AppSettings{});

    QCOMPARE(manager.count(), 1);
    QCOMPARE(tabs.count(), 1);
    QCOMPARE(manager.browserAt(0)->currentPath(), QDir::homePath());
}

void FileBrowserWidgetTest::newTabButtonCreatesHomeTab() {
    TabStrip strip;
    TabManager manager;
    strip.setTabManager(&manager);
    manager.restoreTabs(AppSettings{});

    const auto buttons = strip.findChildren<QToolButton *>("newTabButton");
    QCOMPARE(buttons.size(), 1);
    auto *button = buttons.constFirst();
    QVERIFY(button != nullptr);
    QCOMPARE(manager.tabWidget()->cornerWidget(Qt::TopRightCorner), button);

    QTest::mouseClick(button, Qt::LeftButton);

    QCOMPARE(manager.count(), 2);
    QCOMPARE(manager.browserAt(1)->currentPath(), QDir::homePath());
    QCOMPARE(manager.tabWidget()->currentIndex(), 1);
}

void FileBrowserWidgetTest::restoresSavedTabs() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    AppSettings settings;
    settings.tabs.append({first.path(), {}, {}});
    settings.tabs.append({second.path(), {}, {}});

    QTabWidget tabs;
    TabManager manager;
    manager.setTabWidget(&tabs);

    manager.restoreTabs(settings);

    QCOMPARE(manager.count(), 2);
    QCOMPARE(tabs.count(), 2);
    QCOMPARE(manager.browserAt(0)->currentPath(), first.path());
    QCOMPARE(manager.browserAt(1)->currentPath(), second.path());
}

void FileBrowserWidgetTest::setCurrentPathUpdatesViewAndEmits() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    FileBrowserWidget browser;
    QSignalSpy pathChangedSpy(&browser, &FileBrowserWidget::pathChanged);

    QVERIFY(browser.setCurrentPath(directory.path()));

    QCOMPARE(browser.currentPath(), directory.path());
    QCOMPARE(browser.addressBar()->text(), directory.path());
    QCOMPARE(pathChangedSpy.count(), 1);
    QCOMPARE(pathChangedSpy.takeFirst().at(0).toString(), directory.path());
    QTableView *view = browser.view();
    QVERIFY(view != nullptr);
    QVERIFY(browser.view()->rootIndex().isValid());
}

void FileBrowserWidgetTest::addressBarNavigatesToExistingDirectoryOnly() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(first.path()));

    browser.addressBar()->setText(second.path());
    QTest::keyClick(browser.addressBar(), Qt::Key_Return);
    QCOMPARE(browser.currentPath(), second.path());

    browser.addressBar()->setText(QDir(second.path()).filePath("missing"));
    QTest::keyClick(browser.addressBar(), Qt::Key_Return);
    QCOMPARE(browser.currentPath(), second.path());
    QCOMPARE(browser.addressBar()->text(), second.path());
}

void FileBrowserWidgetTest::upButtonNavigatesToParentDirectory() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir("child"));
    const QString childPath = QDir(root.path()).filePath("child");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(childPath));
    auto *upButton = browser.findChild<QToolButton *>("upButton");
    QVERIFY(upButton != nullptr);

    QTest::mouseClick(upButton, Qt::LeftButton);

    QCOMPARE(browser.currentPath(), root.path());
}

void FileBrowserWidgetTest::addressBarUsesModernNavigationChrome() {
    FileBrowserWidget browser;

    QVERIFY(browser.findChild<QWidget *>("addressBarContainer") != nullptr);
    QVERIFY(browser.findChild<QToolButton *>("upButton") != nullptr);
    QCOMPARE(browser.addressBar()->placeholderText(), QString("Enter a folder path"));
}

void FileBrowserWidgetTest::doubleClickingFolderChangesCurrentPath() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString childPath = QDir(root.path()).filePath("child");
    QVERIFY(QDir(root.path()).mkdir("child"));

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));

    auto *model = qobject_cast<QFileSystemModel *>(browser.view()->model());
    QVERIFY(model != nullptr);
    const QModelIndex childIndex = model->index(childPath);
    QVERIFY(childIndex.isValid());

    QMetaObject::invokeMethod(browser.view(), "doubleClicked", Q_ARG(QModelIndex, childIndex));

    QCOMPARE(browser.currentPath(), childPath);
}

void FileBrowserWidgetTest::doubleClickingFileOpensDesktopUrl() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("document.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("content");
    file.close();

    UrlOpenHandler handler;
    QDesktopServices::setUrlHandler("file", &handler, "openUrl");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));

    auto *model = qobject_cast<QFileSystemModel *>(browser.view()->model());
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = model->index(filePath);
    QVERIFY(fileIndex.isValid());

    QMetaObject::invokeMethod(browser.view(), "doubleClicked", Q_ARG(QModelIndex, fileIndex));

    QDesktopServices::unsetUrlHandler("file");
    QCOMPARE(browser.currentPath(), root.path());
    QCOMPARE(handler.openedUrls.size(), 1);
    QCOMPARE(handler.openedUrls.first(), QUrl::fromLocalFile(filePath));
}

void FileBrowserWidgetTest::selectingEntryEmitsSelectedPath() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("document.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("content");
    file.close();

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));
    QSignalSpy selectedPathSpy(&browser, &FileBrowserWidget::selectedPathChanged);

    auto *model = qobject_cast<QFileSystemModel *>(browser.view()->model());
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = model->index(filePath);
    QVERIFY(fileIndex.isValid());

    browser.view()->selectionModel()->select(fileIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    QCOMPARE(selectedPathSpy.count(), 1);
    QCOMPARE(selectedPathSpy.takeFirst().at(0).toString(), filePath);
}

void FileBrowserWidgetTest::enablesDragAndDropFileOperations() {
    FileBrowserWidget browser;

    QVERIFY(browser.view()->dragEnabled());
    QVERIFY(browser.view()->acceptDrops());
    QVERIFY(browser.view()->showDropIndicator());
    QCOMPARE(browser.view()->dragDropMode(), QAbstractItemView::DragDrop);
}

void FileBrowserWidgetTest::switchesBetweenDetailsListAndTilesViews() {
    FileBrowserWidget browser;

    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::Details);
    QVERIFY(browser.detailsView()->isVisible() || browser.detailsView()->parentWidget() != nullptr);

    browser.setViewMode(FileBrowserWidget::ViewMode::List);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::List);
    QCOMPARE(browser.activeView(), browser.listView());
    QCOMPARE(browser.listView()->viewMode(), QListView::ListMode);

    browser.setViewMode(FileBrowserWidget::ViewMode::Tiles);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::Tiles);
    QCOMPARE(browser.activeView(), browser.tilesView());
    QCOMPARE(browser.tilesView()->viewMode(), QListView::IconMode);
}

void FileBrowserWidgetTest::viewModeSwitchPreservesCurrentPath() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(directory.path()));

    browser.setViewMode(FileBrowserWidget::ViewMode::Tiles);
    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(browser.activeView()->rootIndex().isValid());

    browser.setViewMode(FileBrowserWidget::ViewMode::Details);
    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(browser.activeView()->rootIndex().isValid());
}

void FileBrowserWidgetTest::viewModeButtonsChangeActiveView() {
    FileBrowserWidget browser;

    auto *listButton = browser.findChild<QToolButton *>("listViewButton");
    auto *detailsButton = browser.findChild<QToolButton *>("detailsViewButton");
    auto *tilesButton = browser.findChild<QToolButton *>("tilesViewButton");
    QVERIFY(listButton != nullptr);
    QVERIFY(detailsButton != nullptr);
    QVERIFY(tilesButton != nullptr);

    QTest::mouseClick(listButton, Qt::LeftButton);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::List);

    QTest::mouseClick(tilesButton, Qt::LeftButton);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::Tiles);

    QTest::mouseClick(detailsButton, Qt::LeftButton);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::Details);
}

void FileBrowserWidgetTest::showsBreadcrumbByDefaultAndPathEditorOnCtrlL() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileBrowserWidget browser;
    browser.show();
    QVERIFY(QTest::qWaitForWindowExposed(&browser));
    QVERIFY(browser.setCurrentPath(directory.path()));

    QVERIFY(browser.breadcrumbContainer()->isVisible());
    QVERIFY(!browser.addressBar()->isVisible());

    QTest::keyClick(&browser, Qt::Key_L, Qt::ControlModifier);
    QVERIFY(browser.addressBar()->isVisible());
    QCOMPARE(browser.addressBar()->text(), directory.path());
    QVERIFY(!browser.breadcrumbContainer()->isVisible());
}

void FileBrowserWidgetTest::breadcrumbButtonNavigatesToAncestor() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("parent/child"));
    const QString parentPath = QDir(root.path()).filePath("parent");
    const QString childPath = QDir(root.path()).filePath("parent/child");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(childPath));

    QToolButton *parentButton = nullptr;
    const auto buttons = browser.breadcrumbContainer()->findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button->property("path").toString() == parentPath) {
            parentButton = button;
            break;
        }
    }
    QVERIFY(parentButton != nullptr);

    QTest::mouseClick(parentButton, Qt::LeftButton);
    QCOMPARE(browser.currentPath(), parentPath);
}

void FileBrowserWidgetTest::escapeLeavesPathEditModeWithoutNavigation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString missing = QDir(directory.path()).filePath("missing");

    FileBrowserWidget browser;
    browser.show();
    QVERIFY(QTest::qWaitForWindowExposed(&browser));
    QVERIFY(browser.setCurrentPath(directory.path()));

    QTest::keyClick(&browser, Qt::Key_L, Qt::ControlModifier);
    browser.addressBar()->setText(missing);
    QTest::keyClick(browser.addressBar(), Qt::Key_Escape);

    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(browser.breadcrumbContainer()->isVisible());
    QVERIFY(!browser.addressBar()->isVisible());
}

QTEST_MAIN(FileBrowserWidgetTest)
#include "test_filebrowser.moc"
