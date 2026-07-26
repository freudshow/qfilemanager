#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
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

QTEST_MAIN(MainWindowTest)
#include "test_mainwindow.moc"
