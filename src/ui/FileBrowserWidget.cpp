#include "ui/FileBrowserWidget.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

FileBrowserWidget::FileBrowserWidget(QWidget *parent)
    : QWidget(parent)
    , model_(new QFileSystemModel(this))
    , view_(new QTableView(this))
    , addressBar_(new QLineEdit(this)) {
    setObjectName("fileBrowserWidget");
    addressBar_->setObjectName("addressBar");
    view_->setObjectName("fileBrowserView");

    model_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
    model_->setRootPath(QDir::rootPath());
    view_->setModel(model_);
    view_->setSortingEnabled(true);
    view_->setDragEnabled(true);
    view_->setAcceptDrops(true);
    view_->setDropIndicatorShown(true);
    view_->setDragDropMode(QAbstractItemView::DragDrop);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(addressBar_);
    layout->addWidget(view_, 1);

    connect(addressBar_, &QLineEdit::returnPressed, this, &FileBrowserWidget::navigateFromAddressBar);
    connect(view_, &QTableView::doubleClicked, this, &FileBrowserWidget::openIndex);
    connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FileBrowserWidget::emitSelectedPath);
    connect(view_, &QWidget::customContextMenuRequested, this, &FileBrowserWidget::showContextMenu);
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
    view_->setRootIndex(rootIndex);
    currentPath_ = absolutePath;
    addressBar_->setText(currentPath_);
    emit pathChanged(currentPath_);
    return true;
}

QLineEdit *FileBrowserWidget::addressBar() const {
    return addressBar_;
}

QTableView *FileBrowserWidget::view() const {
    return view_;
}

void FileBrowserWidget::navigateFromAddressBar() {
    setCurrentPath(addressBar_->text());
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
    QMenu menu(this);
    menu.addAction(tr("Open"));
    menu.addAction(tr("Open in New Tab"));
    menu.addSeparator();
    menu.addAction(tr("Copy"));
    menu.addAction(tr("Move"));
    menu.addAction(tr("Rename"));
    menu.addAction(tr("Delete to Trash"));
    menu.addSeparator();
    menu.addAction(tr("New Folder"));
    menu.addAction(tr("Properties"));
    menu.exec(view_->viewport()->mapToGlobal(position));
}
