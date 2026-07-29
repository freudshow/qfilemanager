#include "MainWindow.h"

#include "models/FileMetadata.h"
#include "models/FavoritesModel.h"
#include "services/SettingsStore.h"
#include "services/TabManager.h"
#include "ui/FileBrowserWidget.h"
#include "ui/FavoritesSidebar.h"
#include "ui/MetadataPanel.h"
#include "ui/TabStrip.h"

#include <QTabWidget>
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tabManager_(new TabManager(this))
    , favoritesModel_(new FavoritesModel(this)) {
    setWindowTitle("Qt File Manager");

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
