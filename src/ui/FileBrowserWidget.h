#pragma once

#include <QWidget>

class QFileSystemModel;
class QAbstractItemView;
class QLineEdit;
class QListView;
class QModelIndex;
class QItemSelection;
class QPoint;
class QStackedWidget;
class QTableView;
class QToolButton;

class FileBrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileBrowserWidget(QWidget *parent = nullptr);

    enum class ViewMode {
        Details,
        List,
        Tiles
    };

    QString currentPath() const;
    bool setCurrentPath(const QString &path);
    QLineEdit *addressBar() const;
    ViewMode viewMode() const;
    void setViewMode(ViewMode mode);
    QAbstractItemView *activeView() const;
    QTableView *detailsView() const;
    QListView *listView() const;
    QListView *tilesView() const;
    QTableView *view() const;

signals:
    void pathChanged(const QString &path);
    void selectedPathChanged(const QString &path);
    void openPathInNewTabRequested(const QString &path);

private slots:
    void navigateFromAddressBar();
    void navigateToParentDirectory();
    void openIndex(const QModelIndex &index);
    void emitSelectedPath(const QItemSelection &selected, const QItemSelection &deselected);
    void showContextMenu(const QPoint &position);

private:
    QFileSystemModel *model_ = nullptr;
    QStackedWidget *viewStack_ = nullptr;
    QTableView *detailsView_ = nullptr;
    QListView *listView_ = nullptr;
    QListView *tilesView_ = nullptr;
    ViewMode viewMode_ = ViewMode::Details;
    QLineEdit *addressBar_ = nullptr;
    QToolButton *upButton_ = nullptr;
    QString currentPath_;
};
