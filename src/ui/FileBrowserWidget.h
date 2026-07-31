#pragma once

#include <QHash>
#include <QModelIndex>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <memory>

class QEvent;
class QFileSystemModel;
class QFileSystemWatcher;
class FileSystemSortProxyModel;
class FileOperationService;
class PlatformServices;
class TerminalService;
class QTimer;
class QAbstractItemView;
class QHBoxLayout;
class QLineEdit;
class QListView;
class QMenu;
class QModelIndex;
class QItemSelection;
class QPoint;
class QShortcut;
class QStackedWidget;
class QTableView;
class QToolButton;

class FileBrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileBrowserWidget(QWidget *parent = nullptr);
    ~FileBrowserWidget() override;

    using OpenWithLauncher = std::function<bool(const QString &program, const QStringList &arguments)>;

    enum class ViewMode {
        Details,
        List,
        Tiles
    };

    QString currentPath() const;
    bool setCurrentPath(const QString &path);
    bool canGoBack() const;
    bool canGoForward() const;
    bool goBack();
    bool goForward();
    bool refreshCurrentDirectory();
    QLineEdit *addressBar() const;
    QWidget *breadcrumbContainer() const;
    ViewMode viewMode() const;
    void setViewMode(ViewMode mode);
    QAbstractItemView *activeView() const;
    QTableView *detailsView() const;
    QListView *listView() const;
    QListView *tilesView() const;
    QTableView *view() const;
    QFileSystemModel *fileModel() const;
    QModelIndex viewIndexForPath(const QString &path) const;
    QString sortColumnKey() const;
    QString sortOrderKey() const;
    void setSort(const QString &column, Qt::SortOrder order);
    void setOpenWithDefaults(const QHash<QString, QString> &defaults);
    QHash<QString, QString> openWithDefaults() const;
    void setOpenWithLauncherForTests(OpenWithLauncher launcher);
    using TerminalLauncher = std::function<bool(const QString &program, const QStringList &arguments, const QString &workingDirectory)>;
    void setTerminalLauncherForTests(TerminalLauncher launcher);
    void setPlatformServicesForTests(PlatformServices *platformServices);

signals:
    void pathChanged(const QString &path);
    void historyChanged(bool canGoBack, bool canGoForward);
    void directoryRefreshed();
    void selectedPathChanged(const QString &path);
    void viewModeChanged(FileBrowserWidget::ViewMode mode);
    void openPathInNewTabRequested(const QString &path);
    void openWithDefaultsChanged(const QHash<QString, QString> &defaults);
    void errorOccurred(const QString &message);
    void gitMenuRequested(QMenu *parentMenu, const QString &targetPath, bool backgroundTarget);

private slots:
    void navigateFromAddressBar();
    void navigateToParentDirectory();
    void enterAddressEditMode();
    void leaveAddressEditMode();
    void openIndex(const QModelIndex &index);
    void emitSelectedPath(const QItemSelection &selected, const QItemSelection &deselected);
    void showContextMenu(const QPoint &position);
    void openTargetInTerminal(const QString &path);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool applyPath(const QString &path, bool recordHistory);
    void updateViewRoots(const QModelIndex &rootIndex);
    void watchCurrentDirectory();
    void recordHistoryPath(const QString &path);
    void rebuildBreadcrumbs();
    QStringList pathSegments(const QString &path) const;
    QString extensionForPath(const QString &path) const;
    bool openFilePath(const QString &path);
    bool launchConfiguredApplication(const QString &applicationPath, const QString &filePath);
    void copySelectionToClipboard(const QString &clickedPath = QString(), bool useSelection = true);
    void pasteFromClipboard(const QString &destination = QString());
    QStringList selectedPaths() const;
    QString targetDirectoryForPath(const QString &path) const;
    void createFolderFromMenu(const QString &parentDir);
    void createTextFileFromMenu(const QString &parentDir);
    void movePathFromMenu(const QString &path);
    void renamePathFromMenu(const QString &path);
    void deletePathsFromMenu(const QString &clickedPath, bool useSelection);
    void showPropertiesFromMenu(const QString &path);
    void showOperationError(const QString &error);
    QModelIndex toSourceIndex(const QModelIndex &index) const;
    QModelIndex toViewIndex(const QModelIndex &index) const;

    QFileSystemModel *model_ = nullptr;
    FileSystemSortProxyModel *sortModel_ = nullptr;
    std::unique_ptr<PlatformServices> ownedPlatformServices_;
    std::unique_ptr<FileOperationService> fileOperationService_;
    PlatformServices *platformServicesForTests_ = nullptr;
    std::unique_ptr<TerminalService> terminalService_;
    QStackedWidget *viewStack_ = nullptr;
    QTableView *detailsView_ = nullptr;
    QListView *listView_ = nullptr;
    QListView *tilesView_ = nullptr;
    ViewMode viewMode_ = ViewMode::Details;
    QLineEdit *addressBar_ = nullptr;
    QWidget *breadcrumbContainer_ = nullptr;
    QHBoxLayout *breadcrumbLayout_ = nullptr;
    QShortcut *focusAddressShortcut_ = nullptr;
    QShortcut *copyShortcut_ = nullptr;
    QShortcut *pasteShortcut_ = nullptr;
    QToolButton *upButton_ = nullptr;
    QToolButton *refreshButton_ = nullptr;
    QFileSystemWatcher *directoryWatcher_ = nullptr;
    QTimer *refreshTimer_ = nullptr;
    QString currentPath_;
    QStringList history_;
    int historyIndex_ = -1;
    QHash<QString, QString> openWithDefaults_;
    QString sortColumnKey_ = QStringLiteral("name");
    Qt::SortOrder sortOrder_ = Qt::AscendingOrder;
    OpenWithLauncher openWithLauncher_;
    TerminalLauncher terminalLauncher_;
};
