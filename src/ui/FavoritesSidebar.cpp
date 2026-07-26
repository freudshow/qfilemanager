#include "ui/FavoritesSidebar.h"

#include "models/FavoritesModel.h"

#include <QAction>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QVBoxLayout>

FavoritesSidebar::FavoritesSidebar(QWidget *parent)
    : QWidget(parent)
    , listView_(new QListView(this)) {
    setObjectName("favoritesSidebar");
    listView_->setObjectName("favoritesListView");
    listView_->setContextMenuPolicy(Qt::CustomContextMenu);
    listView_->setCursor(Qt::PointingHandCursor);
    listView_->setSelectionMode(QAbstractItemView::SingleSelection);
    listView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listView_->setStyleSheet(QStringLiteral(
        "QListView#favoritesListView { background: transparent; border: none; outline: none; }"
        "QListView#favoritesListView::item { margin: 3px 0; padding: 7px 10px; border-radius: 8px; color: #1f5f99; }"
        "QListView#favoritesListView::item:hover { background: #eaf3ff; }"
        "QListView#favoritesListView::item:selected { background: #dcecff; color: #174f82; }"));
    addCurrentFolderAction_ = new QAction(tr("Add Current Folder"), this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *label = new QLabel(tr("Favorites"), this);
    label->setObjectName("favoritesSidebarLabel");
    layout->addWidget(label);
    layout->addWidget(listView_, 1);

    connect(listView_, &QListView::clicked, this, &FavoritesSidebar::activateFavorite);
    connect(listView_, &QListView::activated, this, &FavoritesSidebar::activateFavorite);
    connect(listView_, &QListView::doubleClicked, this, &FavoritesSidebar::activateFavorite);
    connect(listView_, &QListView::customContextMenuRequested, this, &FavoritesSidebar::showContextMenu);
    connect(addCurrentFolderAction_, &QAction::triggered, this, &FavoritesSidebar::addCurrentFolderRequested);
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

QAction *FavoritesSidebar::addCurrentFolderAction() const {
    return addCurrentFolderAction_;
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
    const QModelIndex index = listView_->indexAt(position);

    QMenu menu(this);
    menu.addAction(addCurrentFolderAction_);
    QAction *removeAction = nullptr;
    if (index.isValid()) {
        menu.addSeparator();
        removeAction = menu.addAction(tr("Remove"));
    }
    QAction *selectedAction = menu.exec(listView_->viewport()->mapToGlobal(position));
    if (model_ != nullptr && selectedAction == removeAction) {
        model_->removeFavorite(index.row());
    }
}
