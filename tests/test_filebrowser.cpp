#include <QtTest/QtTest>

#include <QDesktopServices>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QShortcut>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
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
    void restoresPerTabSortState();
    void setCurrentPathUpdatesViewAndEmits();
    void tracksBackAndForwardHistory();
    void recordsOnlyDistinctSuccessfulNavigations();
    void discardsForwardHistoryAfterNewNavigation();
    void invalidNavigationDoesNotChangeHistory();
    void failedHistoryNavigationPreservesHistoryIndex();
    void addressBarNavigatesToExistingDirectoryOnly();
    void upButtonNavigatesToParentDirectory();
    void refreshButtonReloadsCurrentDirectoryWithoutNavigation();
    void changesInCurrentDirectoryTriggerRefresh();
    void addressBarUsesModernNavigationChrome();
    void doubleClickingFolderChangesCurrentPath();
    void doubleClickingFileOpensDesktopUrl();
    void configuredDefaultAppOpensFileInsteadOfDesktopUrl();
    void missingConfiguredDefaultFallsBackToDesktopUrl();
    void selectingEntryEmitsSelectedPath();
    void contextMenuContainsSortNewCopyPasteAndTerminalActions();
    void keyboardCopyAndPasteCopiesSelectedFile();
    void clipboardReplacementDisablesPasteFallback();
    void changesSortColumnAndOrder();
    void sortsFilesByEachSupportedColumn();
    void headerClickTogglesSortOrder();
    void terminalActionUsesSelectedFileParentAndReportsFailure();
    void gitMenuRequestedFromContextMenuCarriesTargetDetails_data();
    void gitMenuRequestedFromContextMenuCarriesTargetDetails();
    void enablesDragAndDropFileOperations();
    void switchesBetweenDetailsListAndTilesViews();
    void viewModeSwitchPreservesCurrentPath();
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

void FileBrowserWidgetTest::restoresPerTabSortState() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    AppSettings settings;
    settings.tabs.append({first.path(), QStringLiteral("size"), QStringLiteral("descending")});
    settings.tabs.append({second.path(), QStringLiteral("created"), QStringLiteral("ascending")});

    QTabWidget tabs;
    TabManager manager;
    manager.setTabWidget(&tabs);
    manager.restoreTabs(settings);

    QCOMPARE(manager.browserAt(0)->sortColumnKey(), QStringLiteral("size"));
    QCOMPARE(manager.browserAt(0)->sortOrderKey(), QStringLiteral("descending"));
    QCOMPARE(manager.browserAt(1)->sortColumnKey(), QStringLiteral("created"));
    QCOMPARE(manager.browserAt(1)->sortOrderKey(), QStringLiteral("ascending"));

    const QVector<TabState> saved = manager.tabStates();
    QCOMPARE(saved.at(0).sortColumn, QStringLiteral("size"));
    QCOMPARE(saved.at(0).sortOrder, QStringLiteral("descending"));
    QCOMPARE(saved.at(1).sortColumn, QStringLiteral("created"));
    QCOMPARE(saved.at(1).sortOrder, QStringLiteral("ascending"));
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

void FileBrowserWidgetTest::tracksBackAndForwardHistory() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("one/two"));
    const QString onePath = QDir(root.path()).filePath("one");
    const QString twoPath = QDir(onePath).filePath("two");

    FileBrowserWidget browser;
    QSignalSpy historyChangedSpy(&browser, &FileBrowserWidget::historyChanged);
    QVERIFY(browser.setCurrentPath(root.path()));
    QVERIFY(browser.setCurrentPath(onePath));
    QVERIFY(browser.setCurrentPath(twoPath));
    QVERIFY(browser.canGoBack());
    QVERIFY(!browser.canGoForward());

    QVERIFY(browser.goBack());
    QCOMPARE(browser.currentPath(), onePath);
    QVERIFY(browser.canGoBack());
    QVERIFY(browser.canGoForward());

    QVERIFY(browser.goBack());
    QCOMPARE(browser.currentPath(), root.path());
    QVERIFY(!browser.canGoBack());
    QVERIFY(browser.canGoForward());

    QVERIFY(browser.goForward());
    QCOMPARE(browser.currentPath(), onePath);
    QVERIFY(browser.canGoBack());
    QVERIFY(browser.canGoForward());

    QVERIFY(browser.goForward());
    QCOMPARE(browser.currentPath(), twoPath);
    QVERIFY(browser.canGoBack());
    QVERIFY(!browser.canGoForward());
    QVERIFY(!browser.goForward());
    QCOMPARE(historyChangedSpy.count(), 7);
    QCOMPARE(historyChangedSpy.at(0).at(0).toBool(), false);
    QCOMPARE(historyChangedSpy.at(0).at(1).toBool(), false);
    QCOMPARE(historyChangedSpy.at(2).at(0).toBool(), true);
    QCOMPARE(historyChangedSpy.at(2).at(1).toBool(), false);
    QCOMPARE(historyChangedSpy.at(3).at(0).toBool(), true);
    QCOMPARE(historyChangedSpy.at(3).at(1).toBool(), true);
    QCOMPARE(historyChangedSpy.at(4).at(0).toBool(), false);
    QCOMPARE(historyChangedSpy.at(4).at(1).toBool(), true);
}

void FileBrowserWidgetTest::recordsOnlyDistinctSuccessfulNavigations() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir("child"));
    const QString childPath = QDir(root.path()).filePath("child");

    FileBrowserWidget browser;
    QSignalSpy historyChangedSpy(&browser, &FileBrowserWidget::historyChanged);
    QVERIFY(browser.setCurrentPath(root.path()));
    QVERIFY(browser.setCurrentPath(childPath));
    QVERIFY(browser.setCurrentPath(childPath));

    QCOMPARE(historyChangedSpy.count(), 2);
    QVERIFY(browser.goBack());
    QCOMPARE(browser.currentPath(), root.path());
    QVERIFY(!browser.canGoBack());
    QVERIFY(browser.canGoForward());
}

void FileBrowserWidgetTest::discardsForwardHistoryAfterNewNavigation() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("one/two"));
    QVERIFY(QDir(root.path()).mkdir("three"));
    const QString onePath = QDir(root.path()).filePath("one");
    const QString twoPath = QDir(onePath).filePath("two");
    const QString threePath = QDir(root.path()).filePath("three");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));
    QVERIFY(browser.setCurrentPath(onePath));
    QVERIFY(browser.setCurrentPath(twoPath));
    QVERIFY(browser.goBack());
    QCOMPARE(browser.currentPath(), onePath);

    QVERIFY(browser.setCurrentPath(threePath));
    QCOMPARE(browser.currentPath(), threePath);
    QVERIFY(browser.canGoBack());
    QVERIFY(!browser.canGoForward());

    QVERIFY(browser.goBack());
    QCOMPARE(browser.currentPath(), onePath);
    QVERIFY(browser.goForward());
    QCOMPARE(browser.currentPath(), threePath);
}

void FileBrowserWidgetTest::invalidNavigationDoesNotChangeHistory() {
    QTemporaryDir root;
    QVERIFY(root.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));
    QVERIFY(!browser.canGoBack());
    QVERIFY(!browser.canGoForward());

    QVERIFY(!browser.setCurrentPath(QDir(root.path()).filePath("missing")));
    QCOMPARE(browser.currentPath(), root.path());
    QVERIFY(!browser.canGoBack());
    QVERIFY(!browser.canGoForward());
}

void FileBrowserWidgetTest::failedHistoryNavigationPreservesHistoryIndex() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir("one"));
    QVERIFY(QDir(root.path()).mkdir("two"));
    const QString onePath = QDir(root.path()).filePath("one");
    const QString twoPath = QDir(root.path()).filePath("two");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));
    QVERIFY(browser.setCurrentPath(onePath));
    QVERIFY(browser.setCurrentPath(twoPath));
    QVERIFY(browser.goBack());
    QCOMPARE(browser.currentPath(), onePath);

    QVERIFY(QDir(twoPath).removeRecursively());
    QVERIFY(!browser.goForward());
    QCOMPARE(browser.currentPath(), onePath);
    QVERIFY(browser.canGoBack());
    QVERIFY(browser.canGoForward());
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

void FileBrowserWidgetTest::refreshButtonReloadsCurrentDirectoryWithoutNavigation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(directory.path()));
    QSignalSpy refreshedSpy(&browser, &FileBrowserWidget::directoryRefreshed);
    auto *refreshButton = browser.findChild<QToolButton *>("refreshButton");
    QVERIFY(refreshButton != nullptr);

    QTest::mouseClick(refreshButton, Qt::LeftButton);

    QCOMPARE(refreshedSpy.count(), 1);
    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(!browser.canGoBack());
    QVERIFY(!browser.canGoForward());
}

void FileBrowserWidgetTest::changesInCurrentDirectoryTriggerRefresh() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(directory.path()));
    QSignalSpy refreshedSpy(&browser, &FileBrowserWidget::directoryRefreshed);
    const QString createdPath = QDir(directory.path()).filePath("created-by-another-process.txt");
    QFile createdFile(createdPath);
    QVERIFY(createdFile.open(QIODevice::WriteOnly));
    createdFile.write("new file");
    createdFile.close();

    QTRY_VERIFY(refreshedSpy.count() >= 1);
    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(!browser.canGoBack());
    QVERIFY(!browser.canGoForward());
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

    auto *model = browser.fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex childIndex = browser.viewIndexForPath(childPath);
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

    auto *model = browser.fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = browser.viewIndexForPath(filePath);
    QVERIFY(fileIndex.isValid());

    QMetaObject::invokeMethod(browser.view(), "doubleClicked", Q_ARG(QModelIndex, fileIndex));

    QDesktopServices::unsetUrlHandler("file");
    QCOMPARE(browser.currentPath(), root.path());
    QCOMPARE(handler.openedUrls.size(), 1);
    QCOMPARE(handler.openedUrls.first(), QUrl::fromLocalFile(filePath));
}

void FileBrowserWidgetTest::configuredDefaultAppOpensFileInsteadOfDesktopUrl() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("document.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("content");
    file.close();

    const QString appPath = QDir(root.path()).filePath("fake-editor");
    QFile app(appPath);
    QVERIFY(app.open(QIODevice::WriteOnly));
    app.write("#!/bin/sh\n");
    app.close();
    QVERIFY(app.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    FileBrowserWidget browser;
    browser.setOpenWithDefaults({{QStringLiteral(".txt"), appPath}});

    QString launchedApp;
    QString launchedFile;
    browser.setOpenWithLauncherForTests([&](const QString &program, const QStringList &arguments) {
        launchedApp = program;
        launchedFile = arguments.value(0);
        return true;
    });

    QVERIFY(browser.setCurrentPath(root.path()));
    auto *model = browser.fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = browser.viewIndexForPath(filePath);
    QVERIFY(fileIndex.isValid());

    QMetaObject::invokeMethod(browser.view(), "doubleClicked", Q_ARG(QModelIndex, fileIndex));

    QCOMPARE(launchedApp, appPath);
    QCOMPARE(launchedFile, filePath);
}

void FileBrowserWidgetTest::missingConfiguredDefaultFallsBackToDesktopUrl() {
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
    browser.setOpenWithDefaults({{QStringLiteral(".txt"), QDir(root.path()).filePath("missing-editor")}});
    QVERIFY(browser.setCurrentPath(root.path()));

    auto *model = browser.fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = browser.viewIndexForPath(filePath);
    QVERIFY(fileIndex.isValid());

    QMetaObject::invokeMethod(browser.view(), "doubleClicked", Q_ARG(QModelIndex, fileIndex));

    QDesktopServices::unsetUrlHandler("file");
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

    auto *model = browser.fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = browser.viewIndexForPath(filePath);
    QVERIFY(fileIndex.isValid());

    browser.view()->selectionModel()->select(fileIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    QCOMPARE(selectedPathSpy.count(), 1);
    QCOMPARE(selectedPathSpy.takeFirst().at(0).toString(), filePath);
}

void FileBrowserWidgetTest::contextMenuContainsSortNewCopyPasteAndTerminalActions() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("document.txt");
    QVERIFY2(QFile(filePath).open(QIODevice::WriteOnly), "source file should be created");

    FileBrowserWidget browser;
    browser.resize(800, 600);
    browser.show();
    QVERIFY(QTest::qWaitForWindowExposed(&browser));
    QVERIFY(browser.setCurrentPath(root.path()));

    auto *model = browser.fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = browser.viewIndexForPath(filePath);
    QVERIFY(fileIndex.isValid());
    QTRY_VERIFY(browser.view()->visualRect(fileIndex).isValid());

    QStringList menuTitles;
    bool hasTerminalAction = false;
    bool hasCopyAction = false;
    bool hasPasteAction = false;
    bool hasNewFolderAction = false;
    bool hasNewTextFileAction = false;
    connect(&browser, &FileBrowserWidget::gitMenuRequested, &browser,
            [&](QMenu *parentMenu, const QString &, bool) {
                for (QAction *action : parentMenu->actions()) {
                    if (action->menu() != nullptr) {
                        menuTitles.append(action->text());
                    }
                }
                hasTerminalAction = parentMenu->findChild<QAction *>("openInTerminalAction") != nullptr;
                hasCopyAction = parentMenu->findChild<QAction *>("copyAction") != nullptr;
                hasPasteAction = parentMenu->findChild<QAction *>("pasteAction") != nullptr;
                hasNewFolderAction = parentMenu->findChild<QAction *>("newFolderAction") != nullptr;
                hasNewTextFileAction = parentMenu->findChild<QAction *>("newTextFileAction") != nullptr;
                QTimer::singleShot(0, parentMenu, &QMenu::close);
            });

    QVERIFY(QMetaObject::invokeMethod(&browser, "showContextMenu", Qt::DirectConnection,
                                      Q_ARG(QPoint, browser.view()->visualRect(fileIndex).center())));

    QVERIFY(menuTitles.contains(QStringLiteral("Sort by")));
    QVERIFY(menuTitles.contains(QStringLiteral("Sort Order")));
    QVERIFY(menuTitles.contains(QStringLiteral("New")));
    QVERIFY(hasTerminalAction);
    QVERIFY(hasCopyAction);
    QVERIFY(hasPasteAction);
    QVERIFY(hasNewFolderAction);
    QVERIFY(hasNewTextFileAction);
}

void FileBrowserWidgetTest::keyboardCopyAndPasteCopiesSelectedFile() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());
    const QString sourcePath = QDir(sourceDir.path()).filePath("document.txt");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("clipboard"), qint64(9));
    source.close();

    FileBrowserWidget browser;
    browser.resize(800, 600);
    browser.show();
    QVERIFY(QTest::qWaitForWindowExposed(&browser));
    QVERIFY(browser.setCurrentPath(sourceDir.path()));
    auto *model = browser.fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex sourceIndex = browser.viewIndexForPath(sourcePath);
    QVERIFY(sourceIndex.isValid());
    browser.view()->selectionModel()->select(sourceIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    browser.view()->setFocus();

    QTest::keyClick(browser.view(), Qt::Key_C, Qt::ControlModifier);
    QVERIFY(browser.setCurrentPath(destinationDir.path()));
    QTest::keyClick(browser.view(), Qt::Key_V, Qt::ControlModifier);

    QTRY_VERIFY(QFileInfo::exists(QDir(destinationDir.path()).filePath("document.txt")));
    QFile copied(QDir(destinationDir.path()).filePath("document.txt"));
    QVERIFY(copied.open(QIODevice::ReadOnly));
    QCOMPARE(copied.readAll(), QByteArray("clipboard"));
}

void FileBrowserWidgetTest::clipboardReplacementDisablesPasteFallback() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());
    const QString sourcePath = QDir(sourceDir.path()).filePath("document.txt");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QVERIFY(source.write("clipboard") == 9);
    source.close();

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(sourceDir.path()));
    const QModelIndex sourceIndex = browser.viewIndexForPath(sourcePath);
    QVERIFY(sourceIndex.isValid());
    browser.view()->selectionModel()->select(sourceIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    browser.view()->setFocus();
    QTest::keyClick(browser.view(), Qt::Key_C, Qt::ControlModifier);
    QGuiApplication::clipboard()->setText(QStringLiteral("not a local file"));

    QVERIFY(browser.setCurrentPath(destinationDir.path()));
    QTest::keyClick(browser.view(), Qt::Key_V, Qt::ControlModifier);
    QTest::qWait(150);
    QVERIFY(!QFileInfo::exists(QDir(destinationDir.path()).filePath("document.txt")));
}

void FileBrowserWidgetTest::changesSortColumnAndOrder() {
    FileBrowserWidget browser;

    browser.setSort(QStringLiteral("modified"), Qt::DescendingOrder);

    QCOMPARE(browser.sortColumnKey(), QStringLiteral("modified"));
    QCOMPARE(browser.sortOrderKey(), QStringLiteral("descending"));

    browser.setSort(QStringLiteral("created"), Qt::AscendingOrder);
    QCOMPARE(browser.sortColumnKey(), QStringLiteral("created"));
    QCOMPARE(browser.sortOrderKey(), QStringLiteral("ascending"));
}

void FileBrowserWidgetTest::sortsFilesByEachSupportedColumn() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("name"));
    QVERIFY(QDir(root.path()).mkpath("type"));
    QVERIFY(QDir(root.path()).mkpath("size"));
    QVERIFY(QDir(root.path()).mkpath("modified"));
    QVERIFY(QDir(root.path()).mkpath("created"));

    const auto writeSizedFile = [](const QString &path, int size) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(QByteArray(size, 'x')) == size;
    };
    const auto firstVisiblePath = [](FileBrowserWidget &browser) {
        auto *proxy = qobject_cast<QSortFilterProxyModel *>(browser.view()->model());
        const QModelIndex index = proxy->index(0, 0, browser.view()->rootIndex());
        return browser.fileModel()->filePath(proxy->mapToSource(index));
    };
    const auto lastVisiblePath = [](FileBrowserWidget &browser) {
        auto *proxy = qobject_cast<QSortFilterProxyModel *>(browser.view()->model());
        const QModelIndex rootIndex = browser.view()->rootIndex();
        const QModelIndex index = proxy->index(proxy->rowCount(rootIndex) - 1, 0, rootIndex);
        return browser.fileModel()->filePath(proxy->mapToSource(index));
    };
    const auto checkPair = [&](const QString &directory, const QString &column, const QString &ascendingFirst, const QString &descendingFirst) {
        FileBrowserWidget browser;
        QVERIFY(browser.setCurrentPath(directory));
        QTRY_COMPARE(browser.view()->model()->rowCount(browser.view()->rootIndex()), 2);
        browser.setSort(column, Qt::AscendingOrder);
        QCOMPARE(firstVisiblePath(browser), ascendingFirst);
        browser.setSort(column, Qt::DescendingOrder);
        QCOMPARE(firstVisiblePath(browser), descendingFirst);
    };

    const QString nameFirst = QDir(root.path()).filePath("name/alpha.txt");
    const QString nameSecond = QDir(root.path()).filePath("name/zeta.txt");
    QVERIFY(writeSizedFile(nameFirst, 1));
    QVERIFY(writeSizedFile(nameSecond, 1));
    checkPair(QDir(root.path()).filePath("name"), QStringLiteral("name"), nameFirst, nameSecond);

    const QString typeFirst = QDir(root.path()).filePath("type/zeta.txt");
    const QString typeSecond = QDir(root.path()).filePath("type/alpha.png");
    QVERIFY(writeSizedFile(typeFirst, 1));
    QVERIFY(writeSizedFile(typeSecond, 1));
    {
        FileBrowserWidget browser;
        const QString typeDirectory = QDir(root.path()).filePath("type");
        QVERIFY(browser.setCurrentPath(typeDirectory));
        QTRY_COMPARE(browser.view()->model()->rowCount(browser.view()->rootIndex()), 2);
        const QString firstType = browser.fileModel()->type(browser.fileModel()->index(typeFirst));
        const QString secondType = browser.fileModel()->type(browser.fileModel()->index(typeSecond));
        const int typeComparison = QString::compare(firstType, secondType, Qt::CaseInsensitive);
        const QString ascendingFirst = typeComparison == 0 ? typeSecond : typeComparison < 0 ? typeFirst : typeSecond;
        const QString descendingFirst = typeComparison == 0 ? typeFirst : typeComparison < 0 ? typeSecond : typeFirst;
        browser.setSort(QStringLiteral("type"), Qt::AscendingOrder);
        QCOMPARE(firstVisiblePath(browser), ascendingFirst);
        browser.setSort(QStringLiteral("type"), Qt::DescendingOrder);
        QCOMPARE(firstVisiblePath(browser), descendingFirst);
    }

    const QString sizeFirst = QDir(root.path()).filePath("size/zeta-small.txt");
    const QString sizeSecond = QDir(root.path()).filePath("size/alpha-large.txt");
    QVERIFY(writeSizedFile(sizeFirst, 1));
    QVERIFY(writeSizedFile(sizeSecond, 20));
    checkPair(QDir(root.path()).filePath("size"), QStringLiteral("size"), sizeFirst, sizeSecond);

    const QString modifiedFirst = QDir(root.path()).filePath("modified/zeta-old.txt");
    const QString modifiedSecond = QDir(root.path()).filePath("modified/alpha-new.txt");
    QVERIFY(writeSizedFile(modifiedFirst, 1));
    QTest::qWait(30);
    QVERIFY(writeSizedFile(modifiedSecond, 1));
    QFile oldFile(modifiedFirst);
    QFile newFile(modifiedSecond);
    QVERIFY(oldFile.open(QIODevice::ReadWrite));
    QVERIFY(newFile.open(QIODevice::ReadWrite));
    QVERIFY(oldFile.setFileTime(QDateTime::currentDateTime().addDays(-2), QFileDevice::FileModificationTime));
    QVERIFY(newFile.setFileTime(QDateTime::currentDateTime().addDays(-1), QFileDevice::FileModificationTime));
    oldFile.close();
    newFile.close();
    checkPair(QDir(root.path()).filePath("modified"), QStringLiteral("modified"), modifiedFirst, modifiedSecond);

    const QString createdFirst = QDir(root.path()).filePath("created/zeta-old.txt");
    const QString createdSecond = QDir(root.path()).filePath("created/alpha-new.txt");
    QVERIFY(writeSizedFile(createdFirst, 1));
    QTest::qWait(30);
    QVERIFY(writeSizedFile(createdSecond, 1));
    checkPair(QDir(root.path()).filePath("created"), QStringLiteral("created"), createdFirst, createdSecond);
}

void FileBrowserWidgetTest::headerClickTogglesSortOrder() {
    FileBrowserWidget browser;

    browser.detailsView()->horizontalHeader()->sectionClicked(1);
    QCOMPARE(browser.sortColumnKey(), QStringLiteral("size"));
    QCOMPARE(browser.sortOrderKey(), QStringLiteral("ascending"));
    browser.detailsView()->horizontalHeader()->sectionClicked(1);
    QCOMPARE(browser.sortOrderKey(), QStringLiteral("descending"));
}

void FileBrowserWidgetTest::terminalActionUsesSelectedFileParentAndReportsFailure() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("document.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QString capturedDirectory;
    FileBrowserWidget browser;
    browser.setTerminalLauncherForTests([&](const QString &, const QStringList &, const QString &workingDirectory) {
        capturedDirectory = workingDirectory;
        return true;
    });
    QVERIFY(QMetaObject::invokeMethod(&browser, "openTargetInTerminal", Qt::DirectConnection, Q_ARG(QString, filePath)));
    QCOMPARE(capturedDirectory, QFileInfo(root.path()).absoluteFilePath());

    QSignalSpy errorSpy(&browser, &FileBrowserWidget::errorOccurred);
    QSignalSpy refreshSpy(&browser, &FileBrowserWidget::directoryRefreshed);
    browser.setTerminalLauncherForTests([](const QString &, const QStringList &, const QString &) {
        return false;
    });
    QVERIFY(QMetaObject::invokeMethod(&browser, "openTargetInTerminal", Qt::DirectConnection, Q_ARG(QString, root.path())));
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(refreshSpy.count(), 0);
}

void FileBrowserWidgetTest::gitMenuRequestedFromContextMenuCarriesTargetDetails_data() {
    QTest::addColumn<bool>("backgroundTarget");

    QTest::newRow("file") << false;
    QTest::newRow("background") << true;
}

void FileBrowserWidgetTest::gitMenuRequestedFromContextMenuCarriesTargetDetails() {
    QFETCH(bool, backgroundTarget);

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("document.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    FileBrowserWidget browser;
    browser.resize(800, 600);
    browser.show();
    QVERIFY(QTest::qWaitForWindowExposed(&browser));
    QVERIFY(browser.setCurrentPath(root.path()));

    auto *model = browser.fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = browser.viewIndexForPath(filePath);
    QVERIFY(fileIndex.isValid());
    QTRY_VERIFY(browser.view()->visualRect(fileIndex).isValid());

    const QPoint position = backgroundTarget
        ? QPoint(browser.view()->viewport()->width() - 1, browser.view()->viewport()->height() - 1)
        : browser.view()->visualRect(fileIndex).center();
    QVERIFY(browser.view()->indexAt(position).isValid() != backgroundTarget);

    QMenu *receivedMenu = nullptr;
    QString receivedPath;
    bool receivedBackgroundTarget = false;
    bool separatorPrecedesHook = false;
    connect(&browser, &FileBrowserWidget::gitMenuRequested, &browser,
            [&](QMenu *parentMenu, const QString &targetPath, bool isBackgroundTarget) {
                receivedMenu = parentMenu;
                receivedPath = targetPath;
                receivedBackgroundTarget = isBackgroundTarget;
                const QList<QAction *> actions = parentMenu->actions();
                separatorPrecedesHook = !actions.isEmpty() && actions.constLast()->isSeparator();
                QTimer::singleShot(0, parentMenu, &QMenu::close);
            });

    QVERIFY(QMetaObject::invokeMethod(&browser, "showContextMenu", Qt::DirectConnection,
                                      Q_ARG(QPoint, position)));

    QVERIFY(receivedMenu != nullptr);
    QCOMPARE(receivedPath, backgroundTarget ? root.path() : filePath);
    QCOMPARE(receivedBackgroundTarget, backgroundTarget);
    QVERIFY(separatorPrecedesHook);
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
