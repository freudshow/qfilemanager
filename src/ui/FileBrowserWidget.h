#pragma once

#include <QWidget>

class QFileSystemModel;
class QLineEdit;
class QModelIndex;
class QItemSelection;
class QPoint;
class QTableView;
class QToolButton;

class FileBrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileBrowserWidget(QWidget *parent = nullptr);

    QString currentPath() const;
    bool setCurrentPath(const QString &path);
    QLineEdit *addressBar() const;
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
    QTableView *view_ = nullptr;
    QLineEdit *addressBar_ = nullptr;
    QToolButton *upButton_ = nullptr;
    QString currentPath_;
};
