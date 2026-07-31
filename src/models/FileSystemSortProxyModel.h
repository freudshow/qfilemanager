#pragma once

#include <QSortFilterProxyModel>

class FileSystemSortProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    enum class SortColumn {
        Name,
        Type,
        Size,
        Modified,
        Created,
    };
    Q_ENUM(SortColumn)

    explicit FileSystemSortProxyModel(QObject *parent = nullptr);

    SortColumn sortColumn() const;
    void setSortColumn(SortColumn column);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    SortColumn sortColumn_ = SortColumn::Name;
};
