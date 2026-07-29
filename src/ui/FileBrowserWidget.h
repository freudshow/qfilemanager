#pragma once

#include <QHash>
#include <QStringList>
#include <QWidget>

#include <functional>

class QEvent;
class QFileSystemModel;
class QAbstractItemView;
class QHBoxLayout;
class QLineEdit;
class QListView;
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
    QLineEdit *addressBar() const;
    QWidget *breadcrumbContainer() const;
    ViewMode viewMode() const;
    void setViewMode(ViewMode mode);
    QAbstractItemView *activeView() const;
    QTableView *detailsView() const;
    QListView *listView() const;
    QListView *tilesView() const;
    QTableView *view() const;
    void setOpenWithDefaults(const QHash<QString, QString> &defaults);
    QHash<QString, QString> openWithDefaults() const;
    void setOpenWithLauncherForTests(OpenWithLauncher launcher);

signals:
    void pathChanged(const QString &path);
    void historyChanged(bool canGoBack, bool canGoForward);
    void selectedPathChanged(const QString &path);
    void viewModeChanged(FileBrowserWidget::ViewMode mode);
    void openPathInNewTabRequested(const QString &path);
    void openWithDefaultsChanged(const QHash<QString, QString> &defaults);
    void errorOccurred(const QString &message);

private slots:
    void navigateFromAddressBar();
    void navigateToParentDirectory();
    void enterAddressEditMode();
    void leaveAddressEditMode();
    void openIndex(const QModelIndex &index);
    void emitSelectedPath(const QItemSelection &selected, const QItemSelection &deselected);
    void showContextMenu(const QPoint &position);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool applyPath(const QString &path, bool recordHistory);
    void recordHistoryPath(const QString &path);
    void rebuildBreadcrumbs();
    QStringList pathSegments(const QString &path) const;
    QString extensionForPath(const QString &path) const;
    bool openFilePath(const QString &path);
    bool launchConfiguredApplication(const QString &applicationPath, const QString &filePath);

    QFileSystemModel *model_ = nullptr;
    QStackedWidget *viewStack_ = nullptr;
    QTableView *detailsView_ = nullptr;
    QListView *listView_ = nullptr;
    QListView *tilesView_ = nullptr;
    ViewMode viewMode_ = ViewMode::Details;
    QLineEdit *addressBar_ = nullptr;
    QWidget *breadcrumbContainer_ = nullptr;
    QHBoxLayout *breadcrumbLayout_ = nullptr;
    QShortcut *focusAddressShortcut_ = nullptr;
    QToolButton *upButton_ = nullptr;
    QString currentPath_;
    QStringList history_;
    int historyIndex_ = -1;
    QHash<QString, QString> openWithDefaults_;
    OpenWithLauncher openWithLauncher_;
};
