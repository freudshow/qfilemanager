#pragma once

#include <QMainWindow>

#include "services/SettingsStore.h"

#include <functional>

class QSplitter;
class QToolBar;
class QAction;
class QActionGroup;
class GitService;
struct GitCommandResult;
class QMenu;
class TabManager;
class FavoritesModel;
class MetadataPanel;
class FileBrowserWidget;
class MainWindowTest;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    using ConfirmationProvider = std::function<bool(const QString &title, const QString &message)>;
    using BranchPicker = std::function<QString(const QStringList &branches, bool *accepted)>;
    using ResultPresenter = std::function<void(const QString &title, const GitCommandResult &result, const QString &emptyMessage)>;

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
    void connectBrowserGitMenu(FileBrowserWidget *browser);
    void populateGitMenu(QMenu *parentMenu, const QString &targetPath, bool backgroundTarget);
    void refreshGitMenuTitle(QMenu *gitMenu, const QString &repositoryRoot);
    void runGitAction(const QString &title, const QString &repositoryRoot, const QStringList &arguments, bool requiresConfirmation, QMenu *gitMenu);
    void showGitOutput(const QString &title, const GitCommandResult &result, const QString &emptyMessage = QString());
    void switchGitBranch(const QString &repositoryRoot, QMenu *gitMenu);

    friend class MainWindowTest;

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
    GitService *gitService_ = nullptr;
    ConfirmationProvider confirmationProvider_;
    BranchPicker branchPicker_;
    ResultPresenter resultPresenter_;
};
