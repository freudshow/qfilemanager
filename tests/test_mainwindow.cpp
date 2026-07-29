#include <QtTest/QtTest>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include "MainWindow.h"
#include "models/FavoritesModel.h"
#include "services/SettingsStore.h"
#include "ui/FavoritesSidebar.h"
#include "ui/FileBrowserWidget.h"
#include "ui/MetadataPanel.h"
#include "ui/TabStrip.h"

class MainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void focusedWorkspaceShellHasThreeNamedPanes();
    void restoresSavedTabsFromSettingsStore();
    void restoresSavedFavoritesAndActivatesCurrentTab();
    void persistsFavoriteRemovalWithoutDroppingSavedTabs();
    void addsCurrentFolderToFavoritesFromSidebarRequest();
    void tabStripNewTabButtonCreatesHomeTab();
    void metadataPanelUpdatesFromCurrentBrowserSelection();
    void openWithDefaultsRestorePersistAndPropagateAcrossTabs();
    void toolbarControlsActiveTabAndSynchronizesHistory();
    void toolbarViewActionsSynchronizeWithActiveTab();
};

namespace {

void configureTestAppIdentity(const char *testName) {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("FileManagerQt");
    QCoreApplication::setApplicationName(QStringLiteral("FileManager_%1").arg(QString::fromLatin1(testName)));
}

void removeTestSettings() {
    SettingsStore store;
    QFile::remove(store.settingsPath());
    QFile::remove(store.settingsPath() + ".bak");
}

QTabWidget *findTabWidget(MainWindow &window) {
    auto *splitter = window.centralWidget()->findChild<QSplitter *>("mainWorkspaceSplitter");
    if (splitter == nullptr) {
        return nullptr;
    }
    auto *tabStrip = qobject_cast<TabStrip *>(splitter->widget(1));
    if (tabStrip == nullptr) {
        return nullptr;
    }
    return tabStrip->findChild<QTabWidget *>("tabPlaceholder");
}

FavoritesSidebar *findFavoritesSidebar(MainWindow &window) {
    auto *splitter = window.centralWidget()->findChild<QSplitter *>("mainWorkspaceSplitter");
    if (splitter == nullptr) {
        return nullptr;
    }
    return qobject_cast<FavoritesSidebar *>(splitter->widget(0));
}

MetadataPanel *findMetadataPanel(MainWindow &window) {
    auto *splitter = window.centralWidget()->findChild<QSplitter *>("mainWorkspaceSplitter");
    if (splitter == nullptr) {
        return nullptr;
    }
    return qobject_cast<MetadataPanel *>(splitter->widget(2));
}

} // namespace

void MainWindowTest::init() {
    configureTestAppIdentity(QTest::currentTestFunction());
    removeTestSettings();
}

void MainWindowTest::cleanup() {
    removeTestSettings();
}

void MainWindowTest::focusedWorkspaceShellHasThreeNamedPanes() {
    MainWindow window;

    QCOMPARE(window.windowTitle(), QString("Qt File Manager"));

    QVERIFY(window.centralWidget() != nullptr);

    auto *splitter = window.centralWidget()->findChild<QSplitter *>("mainWorkspaceSplitter");
    QVERIFY(splitter != nullptr);
    QCOMPARE(splitter->count(), 3);

    auto *favoritesSidebar = qobject_cast<FavoritesSidebar *>(splitter->widget(0));
    auto *tabStrip = qobject_cast<TabStrip *>(splitter->widget(1));
    auto *metadataPanel = qobject_cast<MetadataPanel *>(splitter->widget(2));

    QVERIFY(favoritesSidebar != nullptr);
    QVERIFY(tabStrip != nullptr);
    QVERIFY(metadataPanel != nullptr);
    QCOMPARE(favoritesSidebar->objectName(), QString("favoritesSidebar"));
    QCOMPARE(tabStrip->objectName(), QString("tabStrip"));
    QCOMPARE(metadataPanel->objectName(), QString("metadataPanel"));

    auto *tabWidget = tabStrip->findChild<QTabWidget *>("tabPlaceholder");
    QVERIFY(tabWidget != nullptr);
    QVERIFY(tabWidget->count() >= 1);

    auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget->widget(0));
    QVERIFY(browser != nullptr);
    QCOMPARE(browser->currentPath(), QDir::homePath());
}

void MainWindowTest::restoresSavedTabsFromSettingsStore() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    AppSettings settings;
    settings.tabs.append({first.path(), {}, {}});
    settings.tabs.append({second.path(), {}, {}});

    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    MainWindow window;

    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    QCOMPARE(tabWidget->count(), 2);

    auto *firstBrowser = qobject_cast<FileBrowserWidget *>(tabWidget->widget(0));
    auto *secondBrowser = qobject_cast<FileBrowserWidget *>(tabWidget->widget(1));
    QVERIFY(firstBrowser != nullptr);
    QVERIFY(secondBrowser != nullptr);
    QCOMPARE(firstBrowser->currentPath(), first.path());
    QCOMPARE(secondBrowser->currentPath(), second.path());
}

void MainWindowTest::restoresSavedFavoritesAndActivatesCurrentTab() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    AppSettings settings;
    settings.tabs.append({first.path(), {}, {}});
    settings.favorites.append({"Second", second.path()});

    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    MainWindow window;

    auto *sidebar = findFavoritesSidebar(window);
    QVERIFY(sidebar != nullptr);
    QVERIFY(sidebar->model() != nullptr);
    QCOMPARE(sidebar->model()->rowCount(), 1);
    QCOMPARE(sidebar->model()->favoriteAt(0).path, second.path());

    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    QVERIFY(browser != nullptr);
    QCOMPARE(browser->currentPath(), first.path());

    emit sidebar->favoriteActivated(second.path());
    QCOMPARE(browser->currentPath(), second.path());
}

void MainWindowTest::persistsFavoriteRemovalWithoutDroppingSavedTabs() {
    QTemporaryDir first;
    QTemporaryDir favorite;
    QVERIFY(first.isValid());
    QVERIFY(favorite.isValid());

    AppSettings settings;
    settings.tabs.append({first.path(), "name", "ascending"});
    settings.favorites.append({"Favorite", favorite.path()});
    settings.showHiddenFiles = true;

    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    {
        MainWindow window;
        auto *sidebar = findFavoritesSidebar(window);
        QVERIFY(sidebar != nullptr);
        QVERIFY(sidebar->model() != nullptr);
        QCOMPARE(sidebar->model()->rowCount(), 1);
        QVERIFY(sidebar->model()->removeFavorite(0));
        QCOMPARE(sidebar->model()->rowCount(), 0);
    }

    AppSettings loaded;
    QVERIFY2(store.load(loaded, &error), qPrintable(error));
    QVERIFY(loaded.favorites.isEmpty());
    QCOMPARE(loaded.tabs.size(), 1);
    QCOMPARE(loaded.tabs[0].path, first.path());
    QCOMPARE(loaded.tabs[0].sortColumn, QString("name"));
    QCOMPARE(loaded.tabs[0].sortOrder, QString("ascending"));
    QVERIFY(loaded.showHiddenFiles);

    MainWindow reloadedWindow;
    auto *reloadedSidebar = findFavoritesSidebar(reloadedWindow);
    QVERIFY(reloadedSidebar != nullptr);
    QVERIFY(reloadedSidebar->model() != nullptr);
    QCOMPARE(reloadedSidebar->model()->rowCount(), 0);
}

void MainWindowTest::addsCurrentFolderToFavoritesFromSidebarRequest() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    AppSettings settings;
    settings.tabs.append({root.path(), {}, {}});
    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    MainWindow window;
    auto *sidebar = findFavoritesSidebar(window);
    QVERIFY(sidebar != nullptr);

    emit sidebar->addCurrentFolderRequested();

    QCOMPARE(sidebar->model()->rowCount(), 1);
    QCOMPARE(sidebar->model()->favoriteAt(0).path, root.path());
}

void MainWindowTest::tabStripNewTabButtonCreatesHomeTab() {
    MainWindow window;
    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    const int originalCount = tabWidget->count();
    auto *newTabButton = window.findChild<QToolButton *>("newTabButton");
    QVERIFY(newTabButton != nullptr);

    QTest::mouseClick(newTabButton, Qt::LeftButton);

    QCOMPARE(tabWidget->count(), originalCount + 1);
    auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    QVERIFY(browser != nullptr);
    QCOMPARE(browser->currentPath(), QDir::homePath());
}

void MainWindowTest::metadataPanelUpdatesFromCurrentBrowserSelection() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("selected.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("selected");
    file.close();

    AppSettings settings;
    settings.tabs.append({root.path(), {}, {}});

    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    MainWindow window;
    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    QVERIFY(browser != nullptr);
    auto *metadataPanel = findMetadataPanel(window);
    QVERIFY(metadataPanel != nullptr);

    auto *model = qobject_cast<QFileSystemModel *>(browser->view()->model());
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = model->index(filePath);
    QVERIFY(fileIndex.isValid());

    browser->view()->selectionModel()->select(fileIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    QCOMPARE(metadataPanel->displayedValue("Name"), QString("selected.txt"));
    QCOMPARE(metadataPanel->displayedValue("Path"), filePath);
    QCOMPARE(metadataPanel->displayedValue("Size"), QString("8 B"));
}

void MainWindowTest::openWithDefaultsRestorePersistAndPropagateAcrossTabs() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    AppSettings settings;
    settings.tabs.append({first.path(), {}, {}});
    settings.tabs.append({second.path(), {}, {}});
    settings.openWithDefaults.insert(QStringLiteral(".txt"), QStringLiteral("/usr/bin/editor"));

    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    MainWindow window;
    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    QCOMPARE(tabWidget->count(), 2);

    auto *firstBrowser = qobject_cast<FileBrowserWidget *>(tabWidget->widget(0));
    auto *secondBrowser = qobject_cast<FileBrowserWidget *>(tabWidget->widget(1));
    QVERIFY(firstBrowser != nullptr);
    QVERIFY(secondBrowser != nullptr);
    QCOMPARE(firstBrowser->openWithDefaults().value(QStringLiteral(".txt")), QString("/usr/bin/editor"));
    QCOMPARE(secondBrowser->openWithDefaults().value(QStringLiteral(".txt")), QString("/usr/bin/editor"));

    const QHash<QString, QString> updatedDefaults{{QStringLiteral(".png"), QStringLiteral("/usr/bin/viewer")}};
    emit firstBrowser->openWithDefaultsChanged(updatedDefaults);

    AppSettings loaded;
    QVERIFY2(store.load(loaded, &error), qPrintable(error));
    QCOMPARE(loaded.openWithDefaults.value(QStringLiteral(".png")), QString("/usr/bin/viewer"));
    QVERIFY(!loaded.openWithDefaults.contains(QStringLiteral(".txt")));
    QCOMPARE(secondBrowser->openWithDefaults().value(QStringLiteral(".png")), QString("/usr/bin/viewer"));

    auto *newTabButton = window.findChild<QToolButton *>("newTabButton");
    QVERIFY(newTabButton != nullptr);
    QTest::mouseClick(newTabButton, Qt::LeftButton);
    auto *newBrowser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    QVERIFY(newBrowser != nullptr);
    QCOMPARE(newBrowser->openWithDefaults().value(QStringLiteral(".png")), QString("/usr/bin/viewer"));
}

void MainWindowTest::toolbarControlsActiveTabAndSynchronizesHistory() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(QDir(first.path()).mkdir("child"));

    AppSettings settings;
    settings.tabs.append({first.path(), {}, {}});
    settings.tabs.append({second.path(), {}, {}});
    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    MainWindow window;
    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    auto *back = window.findChild<QAction *>("backAction");
    auto *forward = window.findChild<QAction *>("forwardAction");
    QVERIFY(back != nullptr);
    QVERIFY(forward != nullptr);

    auto *firstBrowser = qobject_cast<FileBrowserWidget *>(tabWidget->widget(0));
    QVERIFY(firstBrowser != nullptr);
    QVERIFY(firstBrowser->setCurrentPath(QDir(first.path()).filePath("child")));
    QVERIFY(back->isEnabled());

    tabWidget->setCurrentIndex(1);
    QVERIFY(!back->isEnabled());
    QVERIFY(!forward->isEnabled());

    tabWidget->setCurrentIndex(0);
    QVERIFY(back->isEnabled());
    back->trigger();
    QCOMPARE(firstBrowser->currentPath(), first.path());
}

void MainWindowTest::toolbarViewActionsSynchronizeWithActiveTab() {
    MainWindow window;
    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    auto *toolbar = window.findChild<QToolBar *>("mainToolbar");
    QVERIFY(toolbar != nullptr);
    QVERIFY(!toolbar->isMovable());
    auto *details = window.findChild<QAction *>("detailsViewAction");
    auto *list = window.findChild<QAction *>("listViewAction");
    auto *tiles = window.findChild<QAction *>("tilesViewAction");
    QVERIFY(details != nullptr);
    QVERIFY(list != nullptr);
    QVERIFY(tiles != nullptr);

    auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    QVERIFY(browser != nullptr);
    list->trigger();
    QCOMPARE(browser->viewMode(), FileBrowserWidget::ViewMode::List);
    QVERIFY(list->isChecked());

    tiles->trigger();
    QCOMPARE(browser->viewMode(), FileBrowserWidget::ViewMode::Tiles);
    QVERIFY(tiles->isChecked());

    auto *newTabButton = window.findChild<QToolButton *>("newTabButton");
    QVERIFY(newTabButton != nullptr);
    QTest::mouseClick(newTabButton, Qt::LeftButton);
    auto *newBrowser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    QVERIFY(newBrowser != nullptr);
    QVERIFY(newBrowser != browser);
    QCOMPARE(newBrowser->viewMode(), FileBrowserWidget::ViewMode::Details);
    QVERIFY(details->isChecked());

    tabWidget->setCurrentWidget(browser);
    QCOMPARE(browser->viewMode(), FileBrowserWidget::ViewMode::Tiles);
    QVERIFY(tiles->isChecked());
}

QTEST_MAIN(MainWindowTest)
#include "test_mainwindow.moc"
