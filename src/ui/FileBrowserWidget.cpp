#include "ui/FileBrowserWidget.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QProcess>
#include <QShortcut>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>
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
    , breadcrumbContainer_(new QWidget(this))
    , focusAddressShortcut_(new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this))
    , upButton_(new QToolButton(this))
    , refreshButton_(new QToolButton(this))
    , directoryWatcher_(new QFileSystemWatcher(this))
    , refreshTimer_(new QTimer(this)) {
    openWithLauncher_ = [](const QString &program, const QStringList &arguments) {
        return QProcess::startDetached(program, arguments);
    };

    setObjectName("fileBrowserWidget");
    addressBar_->setObjectName("addressBar");
    addressBar_->setPlaceholderText(tr("Enter a folder path"));
    breadcrumbContainer_->setObjectName("breadcrumbContainer");
    breadcrumbLayout_ = new QHBoxLayout(breadcrumbContainer_);
    breadcrumbLayout_->setContentsMargins(0, 0, 0, 0);
    breadcrumbLayout_->setSpacing(4);
    upButton_->setObjectName("upButton");
    upButton_->setText(QStringLiteral("↑"));
    upButton_->setToolTip(tr("Go to parent folder"));
    refreshButton_->setObjectName("refreshButton");
    refreshButton_->setText(tr("Refresh"));
    refreshButton_->setToolTip(tr("Refresh this folder"));
    refreshTimer_->setSingleShot(true);
    refreshTimer_->setInterval(100);
    auto *addressContainer = new QWidget(this);
    addressContainer->setObjectName("addressBarContainer");
    auto *addressLayout = new QHBoxLayout(addressContainer);
    addressLayout->setContentsMargins(8, 6, 8, 6);
    addressLayout->setSpacing(8);
    addressLayout->addWidget(upButton_);
    addressLayout->addWidget(refreshButton_);
    addressBar_->hide();
    addressBar_->installEventFilter(this);
    addressLayout->addWidget(breadcrumbContainer_, 1);
    addressLayout->addWidget(addressBar_, 1);
    addressContainer->setStyleSheet(QStringLiteral(
        "QWidget#addressBarContainer { background: #f6f8fb; border: 1px solid #d7dee8; border-radius: 10px; }"
        "QLineEdit#addressBar { background: white; border: 1px solid #c8d2df; border-radius: 8px; padding: 6px 10px; }"
        "QToolButton#upButton, QToolButton#refreshButton { background: #ffffff; border: 1px solid #c8d2df; border-radius: 8px; padding: 5px 10px; }"
        "QToolButton#upButton:hover, QToolButton#refreshButton:hover { background: #eaf1f8; }"));

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
    connect(focusAddressShortcut_, &QShortcut::activated, this, &FileBrowserWidget::enterAddressEditMode);
    connect(upButton_, &QToolButton::clicked, this, &FileBrowserWidget::navigateToParentDirectory);
    connect(refreshButton_, &QToolButton::clicked, this, [this] {
        refreshCurrentDirectory();
    });
    connect(directoryWatcher_, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        refreshTimer_->start();
    });
    connect(refreshTimer_, &QTimer::timeout, this, [this] {
        refreshCurrentDirectory();
    });
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
    return applyPath(path, true);
}

bool FileBrowserWidget::canGoBack() const {
    return historyIndex_ > 0;
}

bool FileBrowserWidget::canGoForward() const {
    return historyIndex_ >= 0 && historyIndex_ + 1 < history_.size();
}

bool FileBrowserWidget::goBack() {
    if (!canGoBack()) {
        return false;
    }

    const int candidateIndex = historyIndex_ - 1;
    if (!applyPath(history_.at(candidateIndex), false)) {
        return false;
    }
    historyIndex_ = candidateIndex;
    emit historyChanged(canGoBack(), canGoForward());
    return true;
}

bool FileBrowserWidget::goForward() {
    if (!canGoForward()) {
        return false;
    }

    const int candidateIndex = historyIndex_ + 1;
    if (!applyPath(history_.at(candidateIndex), false)) {
        return false;
    }
    historyIndex_ = candidateIndex;
    emit historyChanged(canGoBack(), canGoForward());
    return true;
}

void FileBrowserWidget::updateViewRoots(const QModelIndex &rootIndex) {
    detailsView_->setRootIndex(rootIndex);
    listView_->setRootIndex(rootIndex);
    tilesView_->setRootIndex(rootIndex);
}

bool FileBrowserWidget::applyPath(const QString &path, bool recordHistory) {
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
    updateViewRoots(rootIndex);
    currentPath_ = absolutePath;
    watchCurrentDirectory();
    addressBar_->setText(currentPath_);
    rebuildBreadcrumbs();
    leaveAddressEditMode();
    if (recordHistory) {
        recordHistoryPath(currentPath_);
    }
    emit pathChanged(currentPath_);
    return true;
}

void FileBrowserWidget::watchCurrentDirectory() {
    const QStringList watchedPaths = directoryWatcher_->directories();
    if (!watchedPaths.isEmpty()) {
        directoryWatcher_->removePaths(watchedPaths);
    }

    if (!currentPath_.isEmpty() && QFileInfo(currentPath_).isDir()) {
        directoryWatcher_->addPath(currentPath_);
    }
}

bool FileBrowserWidget::refreshCurrentDirectory() {
    if (currentPath_.isEmpty() || !QFileInfo(currentPath_).isDir()) {
        watchCurrentDirectory();
        return false;
    }

    const QModelIndex rootIndex = model_->setRootPath(currentPath_);
    updateViewRoots(rootIndex);
    watchCurrentDirectory();
    emit directoryRefreshed();
    return rootIndex.isValid();
}

void FileBrowserWidget::recordHistoryPath(const QString &path) {
    if (historyIndex_ >= 0 && history_.value(historyIndex_) == path) {
        return;
    }

    history_.remove(historyIndex_ + 1, history_.size() - historyIndex_ - 1);
    history_.append(path);
    historyIndex_ = history_.size() - 1;
    emit historyChanged(canGoBack(), canGoForward());
}

QLineEdit *FileBrowserWidget::addressBar() const {
    return addressBar_;
}

QWidget *FileBrowserWidget::breadcrumbContainer() const {
    return breadcrumbContainer_;
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
    emit viewModeChanged(viewMode_);
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

void FileBrowserWidget::setOpenWithDefaults(const QHash<QString, QString> &defaults) {
    openWithDefaults_.clear();
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        QString key = it.key().toLower();
        if (!key.startsWith(QLatin1Char('.'))) {
            key.prepend(QLatin1Char('.'));
        }
        openWithDefaults_.insert(key, it.value());
    }
}

QHash<QString, QString> FileBrowserWidget::openWithDefaults() const {
    return openWithDefaults_;
}

void FileBrowserWidget::setOpenWithLauncherForTests(OpenWithLauncher launcher) {
    openWithLauncher_ = std::move(launcher);
}

void FileBrowserWidget::navigateFromAddressBar() {
    if (setCurrentPath(addressBar_->text())) {
        leaveAddressEditMode();
    }
}

void FileBrowserWidget::navigateToParentDirectory() {
    QDir directory(currentPath_);
    if (directory.cdUp()) {
        setCurrentPath(directory.absolutePath());
    }
}

void FileBrowserWidget::enterAddressEditMode() {
    addressBar_->setText(currentPath_);
    breadcrumbContainer_->hide();
    addressBar_->show();
    addressBar_->setFocus(Qt::ShortcutFocusReason);
    addressBar_->selectAll();
}

void FileBrowserWidget::leaveAddressEditMode() {
    addressBar_->setText(currentPath_);
    addressBar_->hide();
    breadcrumbContainer_->show();
}

bool FileBrowserWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == addressBar_ && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            leaveAddressEditMode();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FileBrowserWidget::rebuildBreadcrumbs() {
    while (QLayoutItem *item = breadcrumbLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const QString cleanPath = QDir::cleanPath(currentPath_);
    QStringList names;
    QStringList paths;

    QString path = cleanPath;
    while (!path.isEmpty()) {
        QFileInfo info(path);
        const QString label = info.fileName().isEmpty() ? path : info.fileName();
        names.prepend(label);
        paths.prepend(path);
        QDir parent(path);
        if (!parent.cdUp()) {
            break;
        }
        const QString parentPath = QDir::cleanPath(parent.absolutePath());
        if (parentPath == path) {
            break;
        }
        path = parentPath;
    }

    for (int i = 0; i < names.size(); ++i) {
        auto *button = new QToolButton(breadcrumbContainer_);
        button->setText(names.at(i));
        button->setProperty("path", paths.at(i));
        button->setAutoRaise(true);
        connect(button, &QToolButton::clicked, this, [this, button] {
            setCurrentPath(button->property("path").toString());
        });
        breadcrumbLayout_->addWidget(button);
        if (i + 1 < names.size()) {
            auto *separator = new QLabel(QStringLiteral(">"), breadcrumbContainer_);
            breadcrumbLayout_->addWidget(separator);
        }
    }
    breadcrumbLayout_->addStretch(1);
}

QStringList FileBrowserWidget::pathSegments(const QString &path) const {
    return QDir::cleanPath(path).split(QDir::separator(), Qt::SkipEmptyParts);
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
        openFilePath(path);
    }
}

QString FileBrowserWidget::extensionForPath(const QString &path) const {
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix.isEmpty() ? QString() : QStringLiteral(".%1").arg(suffix);
}

bool FileBrowserWidget::openFilePath(const QString &path) {
    const QString extension = extensionForPath(path);
    const QString configuredApplication = openWithDefaults_.value(extension);
    if (!configuredApplication.isEmpty()) {
        if (launchConfiguredApplication(configuredApplication, path)) {
            return true;
        }
    }

    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    if (!opened) {
        emit errorOccurred(tr("Unable to open %1").arg(path));
    }
    return opened;
}

bool FileBrowserWidget::launchConfiguredApplication(const QString &applicationPath, const QString &filePath) {
    if (!QFileInfo::exists(applicationPath)) {
        emit errorOccurred(tr("Configured application is missing: %1").arg(applicationPath));
        return false;
    }

    if (!openWithLauncher_) {
        emit errorOccurred(tr("No launcher is configured for %1").arg(applicationPath));
        return false;
    }

    if (!openWithLauncher_(applicationPath, {filePath})) {
        emit errorOccurred(tr("Unable to launch %1").arg(applicationPath));
        return false;
    }
    return true;
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
    QAction *openWithAction = menu.addAction(tr("Open With..."));
    QAction *setDefaultAppAction = menu.addAction(tr("Set Default App for This Type..."));
    QAction *clearDefaultAppAction = menu.addAction(tr("Clear Default App for This Type"));
    const QString extension = extensionForPath(path);
    const bool isFile = index.isValid() && info.isFile();
    openAction->setEnabled(index.isValid());
    openInNewTabAction->setEnabled(info.isDir());
    openWithAction->setEnabled(isFile);
    setDefaultAppAction->setEnabled(isFile && !extension.isEmpty());
    clearDefaultAppAction->setEnabled(isFile && openWithDefaults_.contains(extension));
    menu.addSeparator();
    menu.addAction(tr("Copy"));
    menu.addAction(tr("Move"));
    menu.addAction(tr("Rename"));
    menu.addAction(tr("Delete to Trash"));
    menu.addSeparator();
    menu.addAction(tr("New Folder"));
    menu.addAction(tr("Properties"));
    menu.addSeparator();
    emit gitMenuRequested(&menu, path, !index.isValid());
    QAction *selectedAction = menu.exec(view->viewport()->mapToGlobal(position));
    if (selectedAction == openAction && index.isValid()) {
        openIndex(index);
    } else if (selectedAction == openInNewTabAction && info.isDir()) {
        emit openPathInNewTabRequested(path);
    } else if (selectedAction == openWithAction && isFile) {
        const QString applicationPath = QFileDialog::getOpenFileName(this, tr("Choose Application"));
        if (!applicationPath.isEmpty()) {
            launchConfiguredApplication(applicationPath, path);
        }
    } else if (selectedAction == setDefaultAppAction && isFile && !extension.isEmpty()) {
        const QString applicationPath = QFileDialog::getOpenFileName(this, tr("Choose Default Application"));
        if (!applicationPath.isEmpty()) {
            openWithDefaults_.insert(extension, applicationPath);
            emit openWithDefaultsChanged(openWithDefaults_);
            launchConfiguredApplication(applicationPath, path);
        }
    } else if (selectedAction == clearDefaultAppAction && isFile) {
        if (openWithDefaults_.remove(extension) > 0) {
            emit openWithDefaultsChanged(openWithDefaults_);
        }
    }
}
