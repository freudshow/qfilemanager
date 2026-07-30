#include "MainWindow.h"

#include "models/FileMetadata.h"
#include "models/FavoritesModel.h"
#include "services/SettingsStore.h"
#include "services/GitService.h"
#include "services/TabManager.h"
#include "ui/FileBrowserWidget.h"
#include "ui/FavoritesSidebar.h"
#include "ui/MetadataPanel.h"
#include "ui/TabStrip.h"

#include <QIcon>
#include <QTabWidget>
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QSplitter>
#include <QStyle>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tabManager_(new TabManager(this))
    , favoritesModel_(new FavoritesModel(this))
    , gitService_(new GitService(this)) {
    setWindowTitle("Qt File Manager");
    setWindowIcon(QIcon(QStringLiteral(":/icons/filemanager.png")));
    gitService_->setObjectName("gitService");

    toolbar_ = addToolBar(tr("Browse"));
    toolbar_->setObjectName("mainToolbar");
    toolbar_->setMovable(false);

    backAction_ = toolbar_->addAction(tr("Back"));
    backAction_->setObjectName("backAction");
    backAction_->setToolTip(tr("Go back in this tab"));
    forwardAction_ = toolbar_->addAction(tr("Forward"));
    forwardAction_->setObjectName("forwardAction");
    forwardAction_->setToolTip(tr("Go forward in this tab"));
    toolbar_->addSeparator();

    viewModeActionGroup_ = new QActionGroup(this);
    viewModeActionGroup_->setExclusive(true);
    detailsViewAction_ = toolbar_->addAction(tr("Details"));
    detailsViewAction_->setObjectName("detailsViewAction");
    listViewAction_ = toolbar_->addAction(tr("List"));
    listViewAction_->setObjectName("listViewAction");
    tilesViewAction_ = toolbar_->addAction(tr("Tiles"));
    tilesViewAction_->setObjectName("tilesViewAction");
    for (QAction *action : {detailsViewAction_, listViewAction_, tilesViewAction_}) {
        action->setCheckable(true);
        viewModeActionGroup_->addAction(action);
    }

    connect(backAction_, &QAction::triggered, this, [this] {
        if (FileBrowserWidget *browser = currentBrowser()) {
            browser->goBack();
        }
    });
    connect(forwardAction_, &QAction::triggered, this, [this] {
        if (FileBrowserWidget *browser = currentBrowser()) {
            browser->goForward();
        }
    });
    connect(detailsViewAction_, &QAction::triggered, this, [this] {
        if (FileBrowserWidget *browser = currentBrowser()) {
            browser->setViewMode(FileBrowserWidget::ViewMode::Details);
        }
    });
    connect(listViewAction_, &QAction::triggered, this, [this] {
        if (FileBrowserWidget *browser = currentBrowser()) {
            browser->setViewMode(FileBrowserWidget::ViewMode::List);
        }
    });
    connect(tilesViewAction_, &QAction::triggered, this, [this] {
        if (FileBrowserWidget *browser = currentBrowser()) {
            browser->setViewMode(FileBrowserWidget::ViewMode::Tiles);
        }
    });

    auto *workspace = new QWidget(this);
    auto *layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(0, 0, 0, 0);

    splitter_ = new QSplitter(Qt::Horizontal, workspace);
    splitter_->setObjectName("mainWorkspaceSplitter");
    auto *tabStrip = new TabStrip(splitter_);
    tabStrip->setTabManager(tabManager_);

    AppSettings settings;
    SettingsStore store;
    store.load(settings);
    favoritesModel_->setFavorites(settings.favorites);
    tabManager_->setOpenWithDefaults(settings.openWithDefaults);
    tabManager_->restoreTabs(settings);
    connect(tabManager_, &TabManager::tabAdded, this, [this](FileBrowserWidget *browser) {
        connectBrowserSettings(browser);
        connectBrowserMetadata(browser);
        connectBrowserToolbar(browser);
        connectBrowserGitMenu(browser);
        updateToolbar();
    });

    auto *favoritesSidebar = new FavoritesSidebar(splitter_);
    favoritesSidebar->setModel(favoritesModel_);
    connect(favoritesSidebar, &FavoritesSidebar::favoriteActivated, this, &MainWindow::navigateCurrentTabToFavorite);
    connect(favoritesSidebar, &FavoritesSidebar::addCurrentFolderRequested, this, &MainWindow::addCurrentFolderToFavorites);
    connect(favoritesModel_, &QAbstractItemModel::rowsInserted, this, [this] { persistFavorites(); });
    connect(favoritesModel_, &QAbstractItemModel::rowsRemoved, this, [this] { persistFavorites(); });
    connect(favoritesModel_, &QAbstractItemModel::modelReset, this, [this] { persistFavorites(); });

    metadataPanel_ = new MetadataPanel(splitter_);
    for (int i = 0; i < tabManager_->count(); ++i) {
        connectBrowserSettings(tabManager_->browserAt(i));
        connectBrowserMetadata(tabManager_->browserAt(i));
        connectBrowserToolbar(tabManager_->browserAt(i));
        connectBrowserGitMenu(tabManager_->browserAt(i));
    }

    connect(tabManager_->tabWidget(), &QTabWidget::currentChanged, this, [this](int) {
        updateToolbar();
    });
    updateToolbar();

    splitter_->addWidget(favoritesSidebar);
    splitter_->addWidget(tabStrip);
    splitter_->addWidget(metadataPanel_);
    splitter_->setSizes(settings.splitterSizes.size() == 3 ? QList<int>(settings.splitterSizes.begin(), settings.splitterSizes.end()) : QList<int>({180, 640, 260}));

    layout->addWidget(splitter_);
    setCentralWidget(workspace);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::navigateCurrentTabToFavorite(const QString &path) {
    QTabWidget *tabWidget = tabManager_->tabWidget();
    if (tabWidget == nullptr) {
        return;
    }

    auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    if (browser != nullptr) {
        browser->setCurrentPath(path);
    }
}

void MainWindow::persistFavorites() {
    saveSettings();
}

void MainWindow::addCurrentFolderToFavorites() {
    QTabWidget *tabWidget = tabManager_->tabWidget();
    if (tabWidget == nullptr || favoritesModel_ == nullptr) {
        return;
    }

    auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    if (browser == nullptr || browser->currentPath().isEmpty()) {
        return;
    }

    const QFileInfo info(browser->currentPath());
    const QString name = info.fileName().isEmpty() ? QDir::cleanPath(browser->currentPath()) : info.fileName();
    favoritesModel_->addFavorite(name, browser->currentPath());
}

AppSettings MainWindow::collectSettings() const {
    AppSettings settings;
    SettingsStore store;
    store.load(settings);
    settings.windowGeometry = saveGeometry();
    settings.windowState = saveState();
    if (splitter_ == nullptr) {
        settings.splitterSizes.clear();
    } else {
        const QList<int> sizes = splitter_->sizes();
        settings.splitterSizes = QVector<int>(sizes.begin(), sizes.end());
    }
    settings.tabs = tabManager_ == nullptr ? QVector<TabState>() : tabManager_->tabStates();
    settings.favorites = favoritesModel_->favorites();
    if (tabManager_ != nullptr && tabManager_->count() > 0 && tabManager_->browserAt(0) != nullptr) {
        settings.openWithDefaults = tabManager_->browserAt(0)->openWithDefaults();
    }
    return settings;
}

void MainWindow::saveSettings() {
    SettingsStore store;
    const AppSettings settings = collectSettings();
    store.save(settings);
}

void MainWindow::applyOpenWithDefaults(const QHash<QString, QString> &defaults) {
    if (tabManager_ != nullptr) {
        tabManager_->setOpenWithDefaults(defaults);
    }
    saveSettings();
}

void MainWindow::connectBrowserSettings(FileBrowserWidget *browser) {
    if (browser == nullptr) {
        return;
    }

    connect(browser, &FileBrowserWidget::openWithDefaultsChanged, this, &MainWindow::applyOpenWithDefaults, Qt::UniqueConnection);
}

void MainWindow::connectBrowserMetadata(FileBrowserWidget *browser) {
    if (browser == nullptr || metadataPanel_ == nullptr) {
        return;
    }

    connect(browser, &FileBrowserWidget::selectedPathChanged, this, [this](const QString &path) {
        if (path.isEmpty()) {
            metadataPanel_->clear();
            return;
        }
        metadataPanel_->setMetadata(FileMetadata::fromPath(path));
    });
}

FileBrowserWidget *MainWindow::currentBrowser() const {
    return tabManager_ == nullptr || tabManager_->tabWidget() == nullptr
        ? nullptr
        : qobject_cast<FileBrowserWidget *>(tabManager_->tabWidget()->currentWidget());
}

void MainWindow::updateToolbar() {
    FileBrowserWidget *browser = currentBrowser();
    const bool available = browser != nullptr;
    backAction_->setEnabled(available && browser->canGoBack());
    forwardAction_->setEnabled(available && browser->canGoForward());
    detailsViewAction_->setEnabled(available);
    listViewAction_->setEnabled(available);
    tilesViewAction_->setEnabled(available);
    detailsViewAction_->setChecked(available && browser->viewMode() == FileBrowserWidget::ViewMode::Details);
    listViewAction_->setChecked(available && browser->viewMode() == FileBrowserWidget::ViewMode::List);
    tilesViewAction_->setChecked(available && browser->viewMode() == FileBrowserWidget::ViewMode::Tiles);
}

void MainWindow::connectBrowserToolbar(FileBrowserWidget *browser) {
    if (browser == nullptr) {
        return;
    }

    connect(browser, &FileBrowserWidget::historyChanged, this, [this](bool, bool) {
        updateToolbar();
    });
    connect(browser, &FileBrowserWidget::viewModeChanged, this, [this](FileBrowserWidget::ViewMode) {
        updateToolbar();
    });
}

void MainWindow::connectBrowserGitMenu(FileBrowserWidget *browser) {
    if (browser == nullptr) {
        return;
    }

    connect(browser, &FileBrowserWidget::gitMenuRequested, this, &MainWindow::populateGitMenu, Qt::UniqueConnection);
}

void MainWindow::populateGitMenu(QMenu *parentMenu, const QString &targetPath, bool backgroundTarget) {
    if (parentMenu == nullptr || gitService_ == nullptr) {
        return;
    }

    const QString repositoryRoot = gitService_->findRepositoryRoot(targetPath);
    if (repositoryRoot.isEmpty()) {
        return;
    }

    auto *gitMenu = parentMenu->addMenu(tr("Git"));
    gitMenu->setObjectName("gitContextMenu");
    QAction *pullAction = gitMenu->addAction(tr("Pull"));
    pullAction->setObjectName("gitPullAction");
    QAction *pushAction = gitMenu->addAction(tr("Push"));
    pushAction->setObjectName("gitPushAction");
    gitMenu->addSeparator();
    QAction *stashAction = gitMenu->addAction(tr("Stash"));
    stashAction->setObjectName("gitStashAction");
    QAction *stashPopAction = gitMenu->addAction(tr("Stash Pop"));
    stashPopAction->setObjectName("gitStashPopAction");
    gitMenu->addSeparator();
    QAction *diffAction = gitMenu->addAction(tr("Diff"));
    diffAction->setObjectName("gitDiffAction");
    QAction *logAction = gitMenu->addAction(tr("Show Log"));
    logAction->setObjectName("gitLogAction");
    QAction *switchAction = gitMenu->addAction(tr("Switch Branch..."));
    switchAction->setObjectName("gitSwitchBranchAction");
    QAction *statusAction = gitMenu->addAction(tr("Status"));
    statusAction->setObjectName("gitStatusAction");

    const QFileInfo targetInfo(targetPath);
    const QString relativeTarget = backgroundTarget ? QString() : QDir(repositoryRoot).relativeFilePath(targetInfo.absoluteFilePath());
    const auto withPathspec = [relativeTarget](QStringList arguments) {
        if (!relativeTarget.isEmpty() && relativeTarget != QStringLiteral(".")) {
            arguments << QStringLiteral("--") << relativeTarget;
        }
        return arguments;
    };

    connect(pullAction, &QAction::triggered, this, [this, repositoryRoot, gitMenu] {
        runGitAction(tr("Pull"), repositoryRoot, {QStringLiteral("pull")}, true, gitMenu);
    });
    connect(pushAction, &QAction::triggered, this, [this, repositoryRoot, gitMenu] {
        runGitAction(tr("Push"), repositoryRoot, {QStringLiteral("push")}, true, gitMenu);
    });
    connect(stashAction, &QAction::triggered, this, [this, repositoryRoot, gitMenu] {
        runGitAction(tr("Stash"), repositoryRoot, {QStringLiteral("stash"), QStringLiteral("push")}, true, gitMenu);
    });
    connect(stashPopAction, &QAction::triggered, this, [this, repositoryRoot, gitMenu] {
        runGitAction(tr("Stash Pop"), repositoryRoot, {QStringLiteral("stash"), QStringLiteral("pop")}, true, gitMenu);
    });
    connect(diffAction, &QAction::triggered, this, [this, repositoryRoot, withPathspec, gitMenu] {
        runGitAction(tr("Diff"), repositoryRoot, withPathspec({QStringLiteral("diff")}), false, gitMenu);
    });
    connect(logAction, &QAction::triggered, this, [this, repositoryRoot, withPathspec, gitMenu] {
        runGitAction(tr("Show Log"), repositoryRoot,
                     withPathspec({QStringLiteral("log"), QStringLiteral("--decorate"), QStringLiteral("--oneline"), QStringLiteral("-n"), QStringLiteral("100")}), false, gitMenu);
    });
    connect(switchAction, &QAction::triggered, this, [this, repositoryRoot, gitMenu] {
        switchGitBranch(repositoryRoot, gitMenu);
    });
    connect(statusAction, &QAction::triggered, this, [this, repositoryRoot, gitMenu] {
        runGitAction(tr("Status"), repositoryRoot, {QStringLiteral("status"), QStringLiteral("--short"), QStringLiteral("--branch")}, false, gitMenu);
    });
    connect(gitMenu, &QMenu::aboutToShow, this, [this, gitMenu, repositoryRoot] {
        refreshGitMenuTitle(gitMenu, repositoryRoot);
    });
}

void MainWindow::refreshGitMenuTitle(QMenu *gitMenu, const QString &repositoryRoot) {
    if (gitService_ == nullptr) {
        return;
    }

    QPointer<QMenu> menu = gitMenu;
    gitService_->run(repositoryRoot, {QStringLiteral("status"), QStringLiteral("--porcelain")}, [this, menu](const GitCommandResult &result) {
        if (menu == nullptr || !result.succeeded()) {
            return;
        }

        const bool dirty = GitService::isDirtyPorcelainOutput(result.standardOutput);
        menu->setTitle(dirty ? tr("Git (modified)") : tr("Git"));
        menu->setIcon(dirty ? style()->standardIcon(QStyle::SP_MessageBoxWarning) : QIcon());
    });
}

void MainWindow::runGitAction(const QString &title, const QString &repositoryRoot, const QStringList &arguments, bool requiresConfirmation, QMenu *gitMenu) {
    if (gitService_ == nullptr) {
        return;
    }

    if (requiresConfirmation) {
        const QString message = tr("Run git %1 in %2? This may change files, repository state, or a remote.")
                                    .arg(arguments.join(QLatin1Char(' ')), repositoryRoot);
        const bool accepted = confirmationProvider_ ? confirmationProvider_(title, message)
                                                     : QMessageBox::question(this, title, message) == QMessageBox::Yes;
        if (!accepted) {
            return;
        }
    }

    QPointer<QMenu> menu = gitMenu;
    gitService_->run(repositoryRoot, arguments, [this, title, repositoryRoot, menu](const GitCommandResult &result) {
        const QString emptyMessage = title == tr("Diff") ? tr("No differences.")
            : title == tr("Show Log") ? tr("No commits found.") : QString();
        if (resultPresenter_) {
            resultPresenter_(title, result, emptyMessage);
        } else {
            showGitOutput(title, result, emptyMessage);
        }
        if (menu != nullptr) {
            refreshGitMenuTitle(menu, repositoryRoot);
        }
    });
}

void MainWindow::showGitOutput(const QString &title, const GitCommandResult &result, const QString &emptyMessage) {
    if (!result.succeeded()) {
        const QString details = !result.startError.isEmpty() ? result.startError : result.standardError;
        QMessageBox::critical(this, title, tr("Git command failed.\n%1").arg(details.isEmpty() ? tr("No error output was returned.") : details));
        return;
    }

    QString output = result.standardOutput;
    if (output.trimmed().isEmpty() && !emptyMessage.isEmpty()) {
        output = emptyMessage;
    }
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(title);
    auto *layout = new QVBoxLayout(dialog);
    auto *text = new QTextEdit(dialog);
    text->setReadOnly(true);
    text->setPlainText(output);
    layout->addWidget(text);
    dialog->resize(720, 480);
    dialog->show();
}

void MainWindow::switchGitBranch(const QString &repositoryRoot, QMenu *gitMenu) {
    if (gitService_ == nullptr) {
        return;
    }

    QPointer<QMenu> menu = gitMenu;
    gitService_->run(repositoryRoot, {QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)")}, [this, repositoryRoot, menu](const GitCommandResult &result) {
        if (!result.succeeded()) {
            if (resultPresenter_) {
                resultPresenter_(tr("Switch Branch"), result, {});
            } else {
                showGitOutput(tr("Switch Branch"), result);
            }
            if (menu != nullptr) {
                refreshGitMenuTitle(menu, repositoryRoot);
            }
            return;
        }

        const QStringList branches = result.standardOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        bool accepted = false;
        const QString branch = (branchPicker_ ? branchPicker_(branches, &accepted)
                                               : QInputDialog::getItem(this, tr("Switch Branch"), tr("Local branch:"), branches, 0, true, &accepted)).trimmed();
        if (!accepted || branch.isEmpty()) {
            if (menu != nullptr) {
                refreshGitMenuTitle(menu, repositoryRoot);
            }
            return;
        }
        if (branch.startsWith(QLatin1Char('-'))) {
            GitCommandResult invalidBranch;
            invalidBranch.startError = tr("Branch names cannot begin with '-'.");
            if (resultPresenter_) {
                resultPresenter_(tr("Switch Branch"), invalidBranch, {});
            } else {
                showGitOutput(tr("Switch Branch"), invalidBranch);
            }
            if (menu != nullptr) {
                refreshGitMenuTitle(menu, repositoryRoot);
            }
            return;
        }
        runGitAction(tr("Switch Branch"), repositoryRoot, {QStringLiteral("switch"), branch}, true, menu);
    });
}
