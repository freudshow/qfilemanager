#include "ui/FavoritesSidebar.h"

#include "models/FavoritesModel.h"

#include <QAction>
#include <QListView>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

FavoritesSidebar::FavoritesSidebar(QWidget *parent)
    : QWidget(parent)
    , listView_(new QListView(this)) {
    setObjectName("favoritesSidebar");
    listView_->setObjectName("favoritesListView");
    listView_->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *label = new QLabel(tr("Favorites"), this);
    label->setObjectName("favoritesSidebarLabel");
    layout->addWidget(label);
    layout->addWidget(listView_, 1);

    connect(listView_, &QListView::activated, this, &FavoritesSidebar::activateFavorite);
    connect(listView_, &QListView::doubleClicked, this, &FavoritesSidebar::activateFavorite);
    connect(listView_, &QListView::customContextMenuRequested, this, &FavoritesSidebar::showContextMenu);
}

void FavoritesSidebar::setModel(FavoritesModel *model) {
    model_ = model;
    listView_->setModel(model_);
}

FavoritesModel *FavoritesSidebar::model() const {
    return model_;
}

QListView *FavoritesSidebar::listView() const {
    return listView_;
}

void FavoritesSidebar::activateFavorite(const QModelIndex &index) {
    if (model_ == nullptr || !index.isValid()) {
        return;
    }
    if (!model_->data(index, FavoritesModel::AvailableRole).toBool()) {
        return;
    }
    const QString path = model_->data(index, FavoritesModel::PathRole).toString();
    if (!path.isEmpty()) {
        emit favoriteActivated(path);
    }
}

void FavoritesSidebar::showContextMenu(const QPoint &position) {
    if (model_ == nullptr) {
        return;
    }

    const QModelIndex index = listView_->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu menu(this);
    QAction *removeAction = menu.addAction(tr("Remove"));
    QAction *selectedAction = menu.exec(listView_->viewport()->mapToGlobal(position));
    if (selectedAction == removeAction) {
        model_->removeFavorite(index.row());
    }
}
