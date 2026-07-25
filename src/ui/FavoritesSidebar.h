#pragma once

#include <QWidget>

class FavoritesModel;
class QListView;

class FavoritesSidebar : public QWidget {
    Q_OBJECT

public:
    explicit FavoritesSidebar(QWidget *parent = nullptr);
    void setModel(FavoritesModel *model);
    FavoritesModel *model() const;
    QListView *listView() const;

signals:
    void favoriteActivated(const QString &path);

private slots:
    void activateFavorite(const QModelIndex &index);
    void showContextMenu(const QPoint &position);

private:
    FavoritesModel *model_ = nullptr;
    QListView *listView_ = nullptr;
};
