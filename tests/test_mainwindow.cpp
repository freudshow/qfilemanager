#include <QtTest/QtTest>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QMenu>
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
#include "services/GitService.h"
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
    void toolbarThemesMenuOffersThreeSelectableThemes();
    void selectingThemePersistsImmediately();
    void addsGitSubmenuOnlyForRepositoryTargets();
    void gitMenuShowsDirtyStateAndUsesTargetPathspec();
    void gitMenuActionsUseSafeArgumentsAndCancelledCommandsDoNotRun();
    void gitBranchListingFailurePreventsSwitchAndPresentsError();
    void gitBranchPickerRejectsOptionLikeInputAndSwitchesValidBranch();
    void gitActionCompletionRefreshesKnownMenuDirtyState();
    void gitActionResultsUseOutputAndErrorPresentation();
    void mainWindowUsesBundledApplicationIcon();
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

    auto *model = browser->fileModel();
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = browser->viewIndexForPath(filePath);
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

void MainWindowTest::toolbarThemesMenuOffersThreeSelectableThemes() {
    MainWindow window;

    auto *themesMenu = window.findChild<QMenu *>("themesMenu");
    auto *skinsMenu = window.findChild<QMenu *>("skinsMenu");
    QVERIFY(themesMenu != nullptr);
    QVERIFY(skinsMenu != nullptr);
    QCOMPARE(themesMenu->title(), QStringLiteral("Themes"));
    QCOMPARE(skinsMenu->title(), QStringLiteral("Skins"));

    const QStringList themeNames = skinsMenu->actions().value(0)->text().isEmpty()
        ? QStringList()
        : QStringList({skinsMenu->actions().at(0)->text(), skinsMenu->actions().at(1)->text(), skinsMenu->actions().at(2)->text()});
    QCOMPARE(themeNames, QStringList({QStringLiteral("Aurora Garden"), QStringLiteral("Graphite Ember"), QStringLiteral("Clearwater")}));
    QVERIFY(skinsMenu->actions().at(0)->isCheckable());
    QVERIFY(skinsMenu->actions().at(0)->isChecked());

    skinsMenu->actions().at(1)->trigger();
    QVERIFY(skinsMenu->actions().at(1)->isChecked());
    QVERIFY(!skinsMenu->actions().at(0)->isChecked());
}

void MainWindowTest::selectingThemePersistsImmediately() {
    MainWindow window;

    auto *skinsMenu = window.findChild<QMenu *>("skinsMenu");
    QVERIFY(skinsMenu != nullptr);
    QCOMPARE(skinsMenu->actions().size(), 3);
    skinsMenu->actions().at(1)->trigger();

    AppSettings loaded;
    SettingsStore store;
    QString error;
    QVERIFY2(store.load(loaded, &error), qPrintable(error));
    QCOMPARE(loaded.theme, QStringLiteral("graphite"));
}

void MainWindowTest::addsGitSubmenuOnlyForRepositoryTargets() {
    QTemporaryDir repository;
    QTemporaryDir outside;
    QVERIFY(repository.isValid());
    QVERIFY(outside.isValid());
    QVERIFY(QDir(repository.path()).mkdir(".git"));

    MainWindow window;
    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QVERIFY(browser != nullptr);

    QMenu repositoryMenu;
    emit browser->gitMenuRequested(&repositoryMenu, repository.path(), true);
    const auto gitMenus = repositoryMenu.findChildren<QMenu *>();
    QCOMPARE(gitMenus.size(), 1);
    auto *gitMenu = gitMenus.constFirst();
    QCOMPARE(gitMenu->title(), QString("Git"));
    QCOMPARE(gitMenu->actions().size(), 10);
    QCOMPARE(gitMenu->actions().at(0)->text(), QString("Pull"));
    QCOMPARE(gitMenu->actions().at(1)->text(), QString("Push"));
    QCOMPARE(gitMenu->actions().at(3)->text(), QString("Stash"));
    QCOMPARE(gitMenu->actions().at(4)->text(), QString("Stash Pop"));
    QCOMPARE(gitMenu->actions().at(6)->text(), QString("Diff"));
    QCOMPARE(gitMenu->actions().at(7)->text(), QString("Show Log"));
    QCOMPARE(gitMenu->actions().at(8)->text(), QString("Switch Branch..."));
    QCOMPARE(gitMenu->actions().at(9)->text(), QString("Status"));

    QMenu outsideMenu;
    emit browser->gitMenuRequested(&outsideMenu, outside.path(), true);
    QVERIFY(outsideMenu.findChildren<QMenu *>().isEmpty());
}

void MainWindowTest::gitMenuShowsDirtyStateAndUsesTargetPathspec() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(".git"));
    QVERIFY(QDir(root.path()).mkpath("src"));
    const QString filePath = QDir(root.path()).filePath("src/main.cpp");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    MainWindow window;
    auto *service = window.findChild<GitService *>("gitService");
    QVERIFY(service != nullptr);
    QStringList capturedArguments;
    service->setCommandRunner([&capturedArguments](const QString &, const QStringList &arguments, GitService::CommandCallback callback) {
        capturedArguments = arguments;
        if (arguments == QStringList({QStringLiteral("status"), QStringLiteral("--porcelain")})) {
            callback({true, 0, QStringLiteral(" M src/main.cpp\n"), {}, {}});
            return;
        }
        callback({true, 0, {}, {}, {}});
    });

    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QVERIFY(browser != nullptr);
    QMenu menu;
    emit browser->gitMenuRequested(&menu, filePath, false);
    auto *gitMenu = menu.findChild<QMenu *>("gitContextMenu");
    QVERIFY(gitMenu != nullptr);
    emit gitMenu->aboutToShow();
    QTRY_COMPARE(gitMenu->title(), QString("Git (modified)"));
    QVERIFY(!gitMenu->icon().isNull());

    auto *diffAction = gitMenu->findChild<QAction *>("gitDiffAction");
    QVERIFY(diffAction != nullptr);
    diffAction->trigger();
    QTRY_COMPARE(capturedArguments, QStringList({QStringLiteral("diff"), QStringLiteral("--"), QStringLiteral("src/main.cpp")}));

    QMenu backgroundMenu;
    emit browser->gitMenuRequested(&backgroundMenu, root.path(), true);
    auto *backgroundGitMenu = backgroundMenu.findChild<QMenu *>("gitContextMenu");
    QVERIFY(backgroundGitMenu != nullptr);
    auto *backgroundDiffAction = backgroundGitMenu->findChild<QAction *>("gitDiffAction");
    QVERIFY(backgroundDiffAction != nullptr);
    backgroundDiffAction->trigger();
    QTRY_COMPARE(capturedArguments, QStringList({QStringLiteral("diff")}));
}

void MainWindowTest::gitMenuActionsUseSafeArgumentsAndCancelledCommandsDoNotRun() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(".git"));
    const QString filePath = QDir(root.path()).filePath("tracked.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    MainWindow window;
    window.confirmationProvider_ = [](const QString &, const QString &) { return false; };
    window.branchPicker_ = [](const QStringList &, bool *accepted) {
        *accepted = true;
        return QStringLiteral("topic");
    };
    QStringList arguments;
    QList<QStringList> calls;
    window.resultPresenter_ = [](const QString &, const GitCommandResult &, const QString &) {};
    auto *service = window.findChild<GitService *>("gitService");
    QVERIFY(service != nullptr);
    service->setCommandRunner([&](const QString &, const QStringList &commandArguments, GitService::CommandCallback callback) {
        calls.append(commandArguments);
        arguments = commandArguments;
        if (commandArguments == QStringList({QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)")})) {
            callback({true, 0, QStringLiteral("main\ntopic\n"), {}, {}});
            return;
        }
        callback({true, 0, QStringLiteral("output"), {}, {}});
    });

    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QVERIFY(browser != nullptr);
    QMenu menu;
    emit browser->gitMenuRequested(&menu, filePath, false);
    auto *gitMenu = menu.findChild<QMenu *>("gitContextMenu");
    QVERIFY(gitMenu != nullptr);

    const QList<QPair<QString, QString>> actions{{"gitPullAction", "Pull"}, {"gitPushAction", "Push"},
        {"gitStashAction", "Stash"}, {"gitStashPopAction", "Stash Pop"}, {"gitDiffAction", "Diff"},
        {"gitLogAction", "Show Log"}, {"gitSwitchBranchAction", "Switch Branch..."}, {"gitStatusAction", "Status"}};
    for (const auto &[objectName, text] : actions) {
        auto *action = gitMenu->findChild<QAction *>(objectName);
        QVERIFY2(action != nullptr, qPrintable(objectName));
        QCOMPARE(action->text(), text);
    }

    gitMenu->findChild<QAction *>("gitPullAction")->trigger();
    gitMenu->findChild<QAction *>("gitPushAction")->trigger();
    gitMenu->findChild<QAction *>("gitStashAction")->trigger();
    gitMenu->findChild<QAction *>("gitStashPopAction")->trigger();
    QTRY_COMPARE(calls.size(), 0);

    gitMenu->findChild<QAction *>("gitDiffAction")->trigger();
    QTRY_COMPARE(arguments, QStringList({QStringLiteral("diff"), QStringLiteral("--"), QStringLiteral("tracked.txt")}));
    gitMenu->findChild<QAction *>("gitLogAction")->trigger();
    QTRY_COMPARE(arguments, QStringList({QStringLiteral("log"), QStringLiteral("--decorate"), QStringLiteral("--oneline"), QStringLiteral("-n"), QStringLiteral("100"), QStringLiteral("--"), QStringLiteral("tracked.txt")}));
    gitMenu->findChild<QAction *>("gitStatusAction")->trigger();
    QTRY_COMPARE(arguments, QStringList({QStringLiteral("status"), QStringLiteral("--short"), QStringLiteral("--branch")}));

    gitMenu->findChild<QAction *>("gitSwitchBranchAction")->trigger();
    QTRY_VERIFY(calls.contains(QStringList({QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)")})));
    QVERIFY(!calls.contains(QStringList({QStringLiteral("switch"), QStringLiteral("topic")})));
}

void MainWindowTest::gitBranchListingFailurePreventsSwitchAndPresentsError() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(".git"));

    MainWindow window;
    QStringList presentedTitles;
    window.resultPresenter_ = [&presentedTitles](const QString &title, const GitCommandResult &, const QString &) {
        presentedTitles.append(title);
    };
    auto *service = window.findChild<GitService *>("gitService");
    QVERIFY(service != nullptr);
    QList<QStringList> calls;
    service->setCommandRunner([&calls](const QString &, const QStringList &arguments, GitService::CommandCallback callback) {
        calls.append(arguments);
        if (arguments == QStringList({QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)")})) {
            callback({true, 1, {}, QStringLiteral("branch failed"), {}});
            return;
        }
        callback({true, 0, {}, {}, {}});
    });

    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QVERIFY(browser != nullptr);
    QMenu menu;
    emit browser->gitMenuRequested(&menu, root.path(), true);
    auto *gitMenu = menu.findChild<QMenu *>("gitContextMenu");
    QVERIFY(gitMenu != nullptr);
    gitMenu->findChild<QAction *>("gitSwitchBranchAction")->trigger();

    QTRY_COMPARE(presentedTitles, QStringList({QStringLiteral("Switch Branch")}));
    QVERIFY(!calls.contains(QStringList({QStringLiteral("switch"), QStringLiteral("main")})));
}

void MainWindowTest::gitBranchPickerRejectsOptionLikeInputAndSwitchesValidBranch() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(".git"));

    MainWindow window;
    QStringList presentedTitles;
    window.resultPresenter_ = [&presentedTitles](const QString &title, const GitCommandResult &, const QString &) {
        presentedTitles.append(title);
    };
    QString selectedBranch = QStringLiteral("--detach");
    window.branchPicker_ = [&selectedBranch](const QStringList &, bool *accepted) {
        *accepted = true;
        return selectedBranch;
    };
    window.confirmationProvider_ = [](const QString &, const QString &) { return true; };
    auto *service = window.findChild<GitService *>("gitService");
    QVERIFY(service != nullptr);
    QList<QStringList> calls;
    service->setCommandRunner([&calls](const QString &, const QStringList &arguments, GitService::CommandCallback callback) {
        calls.append(arguments);
        if (arguments == QStringList({QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)")})) {
            callback({true, 0, QStringLiteral("main\ntopic\n"), {}, {}});
            return;
        }
        callback({true, 0, {}, {}, {}});
    });

    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QVERIFY(browser != nullptr);
    QMenu menu;
    emit browser->gitMenuRequested(&menu, root.path(), true);
    auto *gitMenu = menu.findChild<QMenu *>("gitContextMenu");
    QVERIFY(gitMenu != nullptr);

    gitMenu->findChild<QAction *>("gitSwitchBranchAction")->trigger();
    QTRY_COMPARE(presentedTitles, QStringList({QStringLiteral("Switch Branch")}));
    QVERIFY(!calls.contains(QStringList({QStringLiteral("switch"), QStringLiteral("--detach")})));

    const int callsBeforeValidBranch = calls.size();

    selectedBranch = QStringLiteral("topic");
    gitMenu->findChild<QAction *>("gitSwitchBranchAction")->trigger();
    QTRY_VERIFY(calls.mid(callsBeforeValidBranch).contains(QStringList({QStringLiteral("switch"), QStringLiteral("topic")})));
}

void MainWindowTest::gitActionCompletionRefreshesKnownMenuDirtyState() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(".git"));

    MainWindow window;
    window.resultPresenter_ = [](const QString &, const GitCommandResult &, const QString &) {};
    auto *service = window.findChild<GitService *>("gitService");
    QVERIFY(service != nullptr);
    int porcelainRuns = 0;
    service->setCommandRunner([&porcelainRuns](const QString &, const QStringList &arguments, GitService::CommandCallback callback) {
        if (arguments == QStringList({QStringLiteral("status"), QStringLiteral("--porcelain")})) {
            ++porcelainRuns;
            callback({true, 0, porcelainRuns == 1 ? QString() : QStringLiteral(" M changed.txt\n"), {}, {}});
            return;
        }
        callback({true, 0, QStringLiteral("diff output"), {}, {}});
    });

    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QVERIFY(browser != nullptr);
    QMenu menu;
    emit browser->gitMenuRequested(&menu, root.path(), true);
    auto *gitMenu = menu.findChild<QMenu *>("gitContextMenu");
    QVERIFY(gitMenu != nullptr);
    emit gitMenu->aboutToShow();
    QTRY_COMPARE(gitMenu->title(), QStringLiteral("Git"));

    gitMenu->findChild<QAction *>("gitDiffAction")->trigger();
    QTRY_COMPARE(porcelainRuns, 2);
    QTRY_COMPARE(gitMenu->title(), QStringLiteral("Git (modified)"));
}

void MainWindowTest::gitActionResultsUseOutputAndErrorPresentation() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(".git"));

    MainWindow window;
    struct PresentedResult {
        QString title;
        GitCommandResult result;
        QString emptyMessage;
    };
    QList<PresentedResult> presented;
    window.resultPresenter_ = [&presented](const QString &title, const GitCommandResult &result, const QString &emptyMessage) {
        presented.append({title, result, emptyMessage});
    };
    auto *service = window.findChild<GitService *>("gitService");
    QVERIFY(service != nullptr);
    service->setCommandRunner([](const QString &, const QStringList &arguments, GitService::CommandCallback callback) {
        if (arguments == QStringList({QStringLiteral("status"), QStringLiteral("--short"), QStringLiteral("--branch")})) {
            callback({true, 1, {}, QStringLiteral("status failed"), {}});
            return;
        }
        callback({true, 0, {}, {}, {}});
    });

    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QVERIFY(browser != nullptr);
    QMenu menu;
    emit browser->gitMenuRequested(&menu, root.path(), true);
    auto *gitMenu = menu.findChild<QMenu *>("gitContextMenu");
    QVERIFY(gitMenu != nullptr);

    gitMenu->findChild<QAction *>("gitDiffAction")->trigger();
    QTRY_COMPARE(presented.size(), 1);
    QCOMPARE(presented.at(0).title, QStringLiteral("Diff"));
    QVERIFY(presented.at(0).result.succeeded());
    QCOMPARE(presented.at(0).emptyMessage, QStringLiteral("No differences."));

    gitMenu->findChild<QAction *>("gitStatusAction")->trigger();
    QTRY_COMPARE(presented.size(), 2);
    QCOMPARE(presented.at(1).title, QStringLiteral("Status"));
    QVERIFY(!presented.at(1).result.succeeded());
    QCOMPARE(presented.at(1).result.standardError, QStringLiteral("status failed"));
}

void MainWindowTest::mainWindowUsesBundledApplicationIcon() {
    MainWindow window;

    QVERIFY(!window.windowIcon().isNull());
}

QTEST_MAIN(MainWindowTest)
#include "test_mainwindow.moc"
