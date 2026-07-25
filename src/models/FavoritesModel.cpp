#include "models/FavoritesModel.h"

#include <QDir>
#include <QFileInfo>

FavoritesModel::FavoritesModel(QObject *parent)
    : QAbstractListModel(parent) {
}

int FavoritesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return favorites_.size();
}

QVariant FavoritesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= favorites_.size()) {
        return {};
    }

    const FavoriteState &favorite = favorites_.at(index.row());
    const bool available = isAvailable(favorite.path);
    switch (role) {
    case Qt::DisplayRole:
        return available ? favorite.name : tr("%1 (missing)").arg(favorite.name);
    case Qt::ToolTipRole:
        return favorite.path;
    case NameRole:
        return favorite.name;
    case PathRole:
        return favorite.path;
    case AvailableRole:
        return available;
    case MissingRole:
        return !available;
    default:
        return {};
    }
}

QHash<int, QByteArray> FavoritesModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles[NameRole] = "name";
    roles[PathRole] = "path";
    roles[AvailableRole] = "available";
    roles[MissingRole] = "missing";
    return roles;
}

bool FavoritesModel::addFavorite(const QString &name, const QString &path) {
    const QString cleanedName = name.trimmed();
    const QString cleanedPath = normalizedPath(path);
    if (cleanedName.isEmpty() || cleanedPath.isEmpty() || containsPath(cleanedPath)) {
        return false;
    }

    const int row = favorites_.size();
    beginInsertRows(QModelIndex(), row, row);
    favorites_.append({cleanedName, cleanedPath});
    endInsertRows();
    return true;
}

bool FavoritesModel::removeFavorite(int row) {
    if (row < 0 || row >= favorites_.size()) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    favorites_.removeAt(row);
    endRemoveRows();
    return true;
}

FavoriteState FavoritesModel::favoriteAt(int row) const {
    if (row < 0 || row >= favorites_.size()) {
        return {};
    }
    return favorites_.at(row);
}

QVector<FavoriteState> FavoritesModel::favorites() const {
    return favorites_;
}

void FavoritesModel::setFavorites(const QVector<FavoriteState> &favorites) {
    QVector<FavoriteState> normalizedFavorites;
    for (const FavoriteState &favorite : favorites) {
        const QString cleanedName = favorite.name.trimmed();
        const QString cleanedPath = normalizedPath(favorite.path);
        if (cleanedName.isEmpty() || cleanedPath.isEmpty()) {
            continue;
        }

        bool duplicate = false;
        for (const FavoriteState &existing : normalizedFavorites) {
            if (existing.path == cleanedPath) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            normalizedFavorites.append({cleanedName, cleanedPath});
        }
    }

    beginResetModel();
    favorites_ = normalizedFavorites;
    endResetModel();
}

QString FavoritesModel::normalizedPath(const QString &path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
}

bool FavoritesModel::containsPath(const QString &path) const {
    for (const FavoriteState &favorite : favorites_) {
        if (favorite.path == path) {
            return true;
        }
    }
    return false;
}

bool FavoritesModel::isAvailable(const QString &path) const {
    return QFileInfo::exists(path);
}
