#pragma once

#include <QMainWindow>

#include "services/SettingsStore.h"

class QSplitter;
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
    void connectBrowserMetadata(FileBrowserWidget *browser);

    TabManager *tabManager_ = nullptr;
    FavoritesModel *favoritesModel_ = nullptr;
    MetadataPanel *metadataPanel_ = nullptr;
    QSplitter *splitter_ = nullptr;
};
