#include "models/FileSystemSortProxyModel.h"

#include <QFileInfo>
#include <QFileSystemModel>

FileSystemSortProxyModel::FileSystemSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
}

FileSystemSortProxyModel::SortColumn FileSystemSortProxyModel::sortColumn() const {
    return sortColumn_;
}

void FileSystemSortProxyModel::setSortColumn(SortColumn column) {
    if (sortColumn_ == column) {
        return;
    }
    sortColumn_ = column;
    invalidate();
}

bool FileSystemSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const auto *fileModel = qobject_cast<const QFileSystemModel *>(sourceModel());
    if (fileModel == nullptr) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    const QFileInfo leftInfo = fileModel->fileInfo(left);
    const QFileInfo rightInfo = fileModel->fileInfo(right);
    if (leftInfo.isDir() != rightInfo.isDir()) {
        return leftInfo.isDir();
    }

    int comparison = 0;
    switch (sortColumn_) {
    case SortColumn::Type:
        comparison = QString::compare(fileModel->type(left), fileModel->type(right), Qt::CaseInsensitive);
        break;
    case SortColumn::Size:
        comparison = leftInfo.size() < rightInfo.size() ? -1 : leftInfo.size() > rightInfo.size() ? 1 : 0;
        break;
    case SortColumn::Modified: {
        const QDateTime leftTime = leftInfo.lastModified();
        const QDateTime rightTime = rightInfo.lastModified();
        comparison = leftTime < rightTime ? -1 : leftTime > rightTime ? 1 : 0;
        break;
    }
    case SortColumn::Created: {
        QDateTime leftTime = leftInfo.birthTime();
        QDateTime rightTime = rightInfo.birthTime();
        if (!leftTime.isValid()) {
            leftTime = leftInfo.metadataChangeTime();
        }
        if (!rightTime.isValid()) {
            rightTime = rightInfo.metadataChangeTime();
        }
        if (!leftTime.isValid()) {
            leftTime = leftInfo.lastModified();
        }
        if (!rightTime.isValid()) {
            rightTime = rightInfo.lastModified();
        }
        comparison = leftTime < rightTime ? -1 : leftTime > rightTime ? 1 : 0;
        break;
    }
    case SortColumn::Name:
    default:
        comparison = QString::compare(fileModel->fileName(left), fileModel->fileName(right), Qt::CaseInsensitive);
        break;
    }

    if (comparison == 0) {
        comparison = QString::compare(fileModel->fileName(left), fileModel->fileName(right), Qt::CaseInsensitive);
    }
    return comparison < 0;
}
