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
#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tabManager_(new TabManager(this))
    , favoritesModel_(new FavoritesModel(this)) {
    setWindowTitle("Qt File Manager");

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
    tabManager_->restoreTabs(settings);

    auto *favoritesSidebar = new FavoritesSidebar(splitter_);
    favoritesSidebar->setModel(favoritesModel_);
    connect(favoritesSidebar, &FavoritesSidebar::favoriteActivated, this, &MainWindow::navigateCurrentTabToFavorite);
    connect(favoritesSidebar, &FavoritesSidebar::addCurrentFolderRequested, this, &MainWindow::addCurrentFolderToFavorites);
    connect(favoritesModel_, &QAbstractItemModel::rowsInserted, this, [this] { persistFavorites(); });
    connect(favoritesModel_, &QAbstractItemModel::rowsRemoved, this, [this] { persistFavorites(); });
    connect(favoritesModel_, &QAbstractItemModel::modelReset, this, [this] { persistFavorites(); });

    metadataPanel_ = new MetadataPanel(splitter_);
    for (int i = 0; i < tabManager_->count(); ++i) {
        connectBrowserMetadata(tabManager_->browserAt(i));
    }

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
    return settings;
}

void MainWindow::saveSettings() {
    SettingsStore store;
    const AppSettings settings = collectSettings();
    store.save(settings);
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
