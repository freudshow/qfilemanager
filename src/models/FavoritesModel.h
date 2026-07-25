#pragma once

#include "services/SettingsStore.h"

#include <QAbstractListModel>
#include <QVector>

class FavoritesModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        AvailableRole,
        MissingRole
    };
    Q_ENUM(Role)

    explicit FavoritesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool addFavorite(const QString &name, const QString &path);
    bool removeFavorite(int row);
    FavoriteState favoriteAt(int row) const;
    QVector<FavoriteState> favorites() const;
    void setFavorites(const QVector<FavoriteState> &favorites);

private:
    static QString normalizedPath(const QString &path);
    bool containsPath(const QString &path) const;
    bool isAvailable(const QString &path) const;

    QVector<FavoriteState> favorites_;
};
