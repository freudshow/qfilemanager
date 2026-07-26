#include "ui/FileBrowserWidget.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QStackedWidget>
#include <QTableView>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

void configureFileView(QAbstractItemView *view) {
    view->setDragEnabled(true);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
}

}

FileBrowserWidget::FileBrowserWidget(QWidget *parent)
    : QWidget(parent)
    , model_(new QFileSystemModel(this))
    , viewStack_(new QStackedWidget(this))
    , detailsView_(new QTableView(this))
    , listView_(new QListView(this))
    , tilesView_(new QListView(this))
    , addressBar_(new QLineEdit(this))
    , upButton_(new QToolButton(this)) {
    setObjectName("fileBrowserWidget");
    addressBar_->setObjectName("addressBar");
    addressBar_->setPlaceholderText(tr("Enter a folder path"));
    upButton_->setObjectName("upButton");
    upButton_->setText(QStringLiteral("↑"));
    upButton_->setToolTip(tr("Go to parent folder"));

    auto *addressContainer = new QWidget(this);
    addressContainer->setObjectName("addressBarContainer");
    auto *addressLayout = new QHBoxLayout(addressContainer);
    addressLayout->setContentsMargins(8, 6, 8, 6);
    addressLayout->setSpacing(8);
    addressLayout->addWidget(upButton_);
    addressLayout->addWidget(addressBar_, 1);
    addressContainer->setStyleSheet(QStringLiteral(
        "QWidget#addressBarContainer { background: #f6f8fb; border: 1px solid #d7dee8; border-radius: 10px; }"
        "QLineEdit#addressBar { background: white; border: 1px solid #c8d2df; border-radius: 8px; padding: 6px 10px; }"
        "QToolButton#upButton { background: #ffffff; border: 1px solid #c8d2df; border-radius: 8px; padding: 5px 10px; }"
        "QToolButton#upButton:hover { background: #eaf1f8; }"));

    model_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
    model_->setRootPath(QDir::rootPath());
    detailsView_->setObjectName("fileBrowserDetailsView");
    listView_->setObjectName("fileBrowserListView");
    tilesView_->setObjectName("fileBrowserTilesView");

    detailsView_->setModel(model_);
    detailsView_->setSortingEnabled(true);
    configureFileView(detailsView_);

    listView_->setModel(model_);
    listView_->setViewMode(QListView::ListMode);
    listView_->setUniformItemSizes(true);
    configureFileView(listView_);

    tilesView_->setModel(model_);
    tilesView_->setViewMode(QListView::IconMode);
    tilesView_->setIconSize(QSize(48, 48));
    tilesView_->setGridSize(QSize(140, 84));
    tilesView_->setResizeMode(QListView::Adjust);
    tilesView_->setWordWrap(true);
    configureFileView(tilesView_);

    viewStack_->addWidget(detailsView_);
    viewStack_->addWidget(listView_);
    viewStack_->addWidget(tilesView_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(addressContainer);
    layout->addWidget(viewStack_, 1);

    connect(addressBar_, &QLineEdit::returnPressed, this, &FileBrowserWidget::navigateFromAddressBar);
    connect(upButton_, &QToolButton::clicked, this, &FileBrowserWidget::navigateToParentDirectory);
    const auto connectView = [this](QAbstractItemView *view) {
        connect(view, &QAbstractItemView::doubleClicked, this, &FileBrowserWidget::openIndex);
        connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FileBrowserWidget::emitSelectedPath);
        connect(view, &QWidget::customContextMenuRequested, this, &FileBrowserWidget::showContextMenu);
    };
    connectView(detailsView_);
    connectView(listView_);
    connectView(tilesView_);
}

QString FileBrowserWidget::currentPath() const {
    return currentPath_;
}

bool FileBrowserWidget::setCurrentPath(const QString &path) {
    const QFileInfo info(path);
    if (!info.exists() || !info.isDir()) {
        addressBar_->setText(currentPath_);
        return false;
    }

    const QString absolutePath = QDir::cleanPath(info.absoluteFilePath());
    if (absolutePath == currentPath_) {
        addressBar_->setText(currentPath_);
        return true;
    }

    const QModelIndex rootIndex = model_->setRootPath(absolutePath);
    detailsView_->setRootIndex(rootIndex);
    listView_->setRootIndex(rootIndex);
    tilesView_->setRootIndex(rootIndex);
    currentPath_ = absolutePath;
    addressBar_->setText(currentPath_);
    emit pathChanged(currentPath_);
    return true;
}

QLineEdit *FileBrowserWidget::addressBar() const {
    return addressBar_;
}

FileBrowserWidget::ViewMode FileBrowserWidget::viewMode() const {
    return viewMode_;
}

void FileBrowserWidget::setViewMode(ViewMode mode) {
    viewMode_ = mode;
    QAbstractItemView *target = detailsView_;
    if (mode == ViewMode::List) {
        target = listView_;
    } else if (mode == ViewMode::Tiles) {
        target = tilesView_;
    }
    viewStack_->setCurrentWidget(target);
    if (!currentPath_.isEmpty()) {
        target->setRootIndex(model_->index(currentPath_));
    }
}

QAbstractItemView *FileBrowserWidget::activeView() const {
    return qobject_cast<QAbstractItemView *>(viewStack_->currentWidget());
}

QTableView *FileBrowserWidget::detailsView() const {
    return detailsView_;
}

QListView *FileBrowserWidget::listView() const {
    return listView_;
}

QListView *FileBrowserWidget::tilesView() const {
    return tilesView_;
}

QTableView *FileBrowserWidget::view() const {
    return detailsView_;
}

void FileBrowserWidget::navigateFromAddressBar() {
    setCurrentPath(addressBar_->text());
}

void FileBrowserWidget::navigateToParentDirectory() {
    QDir directory(currentPath_);
    if (directory.cdUp()) {
        setCurrentPath(directory.absolutePath());
    }
}

void FileBrowserWidget::openIndex(const QModelIndex &index) {
    if (!index.isValid()) {
        return;
    }

    const QString path = model_->filePath(index);
    const QFileInfo info(path);
    if (info.isDir()) {
        setCurrentPath(path);
        return;
    }

    if (info.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void FileBrowserWidget::emitSelectedPath(const QItemSelection &selected, const QItemSelection &) {
    const QModelIndexList indexes = selected.indexes();
    if (indexes.isEmpty()) {
        emit selectedPathChanged(QString());
        return;
    }

    const QString path = model_->filePath(indexes.first());
    if (!path.isEmpty()) {
        emit selectedPathChanged(path);
    }
}

void FileBrowserWidget::showContextMenu(const QPoint &position) {
    QAbstractItemView *view = activeView();
    const QModelIndex index = view->indexAt(position);
    const QString path = index.isValid() ? model_->filePath(index) : currentPath_;
    const QFileInfo info(path);

    QMenu menu(this);
    QAction *openAction = menu.addAction(tr("Open"));
    QAction *openInNewTabAction = menu.addAction(tr("Open in New Tab"));
    openAction->setEnabled(index.isValid());
    openInNewTabAction->setEnabled(info.isDir());
    menu.addSeparator();
    menu.addAction(tr("Copy"));
    menu.addAction(tr("Move"));
    menu.addAction(tr("Rename"));
    menu.addAction(tr("Delete to Trash"));
    menu.addSeparator();
    menu.addAction(tr("New Folder"));
    menu.addAction(tr("Properties"));
    QAction *selectedAction = menu.exec(view->viewport()->mapToGlobal(position));
    if (selectedAction == openAction && index.isValid()) {
        openIndex(index);
    } else if (selectedAction == openInNewTabAction && info.isDir()) {
        emit openPathInNewTabRequested(path);
    }
}
