#pragma once

#include <QMainWindow>

#include "services/SettingsStore.h"

class QSplitter;
class QToolBar;
class QAction;
class QActionGroup;
class TabManager;
class FavoritesModel;
class MetadataPanel;
class FileBrowserWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void closeEvent(QCloseEvent *event) override;
    void navigateCurrentTabToFavorite(const QString &path);
    void addCurrentFolderToFavorites();
    AppSettings collectSettings() const;
    void saveSettings();
    void persistFavorites();
    void applyOpenWithDefaults(const QHash<QString, QString> &defaults);
    void connectBrowserSettings(FileBrowserWidget *browser);
    void connectBrowserMetadata(FileBrowserWidget *browser);
    FileBrowserWidget *currentBrowser() const;
    void updateToolbar();
    void connectBrowserToolbar(FileBrowserWidget *browser);

    TabManager *tabManager_ = nullptr;
    FavoritesModel *favoritesModel_ = nullptr;
    MetadataPanel *metadataPanel_ = nullptr;
    QSplitter *splitter_ = nullptr;
    QToolBar *toolbar_ = nullptr;
    QAction *backAction_ = nullptr;
    QAction *forwardAction_ = nullptr;
    QAction *detailsViewAction_ = nullptr;
    QAction *listViewAction_ = nullptr;
    QAction *tilesViewAction_ = nullptr;
    QActionGroup *viewModeActionGroup_ = nullptr;
};
