#include <QtTest/QtTest>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QUrl>

#include "services/TabManager.h"
#include "ui/FileBrowserWidget.h"

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
    void restoresSavedTabs();
    void setCurrentPathUpdatesViewAndEmits();
    void addressBarNavigatesToExistingDirectoryOnly();
    void doubleClickingFolderChangesCurrentPath();
    void doubleClickingFileOpensDesktopUrl();
    void selectingEntryEmitsSelectedPath();
    void enablesDragAndDropFileOperations();
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

QTEST_MAIN(FileBrowserWidgetTest)
#include "test_filebrowser.moc"
