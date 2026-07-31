#include "ui/FileBrowserWidget.h"

#include "models/FileSystemSortProxyModel.h"
#include "services/FileOperationService.h"
#include "services/PlatformServices.h"
#include "services/TerminalService.h"

#include <QAbstractItemView>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMimeData>
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
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setDragEnabled(true);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
}

FileSystemSortProxyModel::SortColumn sortColumnForKey(const QString &key) {
    if (key == QStringLiteral("type")) {
        return FileSystemSortProxyModel::SortColumn::Type;
    }
    if (key == QStringLiteral("size")) {
        return FileSystemSortProxyModel::SortColumn::Size;
    }
    if (key == QStringLiteral("modified")) {
        return FileSystemSortProxyModel::SortColumn::Modified;
    }
    if (key == QStringLiteral("created")) {
        return FileSystemSortProxyModel::SortColumn::Created;
    }
    return FileSystemSortProxyModel::SortColumn::Name;
}

QString keyForSortColumn(FileSystemSortProxyModel::SortColumn column) {
    switch (column) {
    case FileSystemSortProxyModel::SortColumn::Type:
        return QStringLiteral("type");
    case FileSystemSortProxyModel::SortColumn::Size:
        return QStringLiteral("size");
    case FileSystemSortProxyModel::SortColumn::Modified:
        return QStringLiteral("modified");
    case FileSystemSortProxyModel::SortColumn::Created:
        return QStringLiteral("created");
    case FileSystemSortProxyModel::SortColumn::Name:
    default:
        return QStringLiteral("name");
    }
}

QStringList localClipboardPaths() {
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    if (mimeData == nullptr || !mimeData->hasUrls()) {
        return {};
    }

    QStringList paths;
    for (const QUrl &url : mimeData->urls()) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }
    return paths;
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
    , copyShortcut_(new QShortcut(QKeySequence::Copy, this))
    , pasteShortcut_(new QShortcut(QKeySequence::Paste, this))
    , upButton_(new QToolButton(this))
    , refreshButton_(new QToolButton(this))
    , directoryWatcher_(new QFileSystemWatcher(this))
    , refreshTimer_(new QTimer(this)) {
    ownedPlatformServices_ = std::make_unique<PlatformServices>();
    platformServicesForTests_ = ownedPlatformServices_.get();
    fileOperationService_ = std::make_unique<FileOperationService>(platformServicesForTests_);
    terminalService_ = std::make_unique<TerminalService>();
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
    model_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
    model_->setRootPath(QDir::rootPath());
    sortModel_ = new FileSystemSortProxyModel(this);
    sortModel_->setSourceModel(model_);
    detailsView_->setObjectName("fileBrowserDetailsView");
    listView_->setObjectName("fileBrowserListView");
    tilesView_->setObjectName("fileBrowserTilesView");

    detailsView_->setModel(sortModel_);
    detailsView_->setSortingEnabled(true);
    configureFileView(detailsView_);

    listView_->setModel(sortModel_);
    listView_->setViewMode(QListView::ListMode);
    listView_->setUniformItemSizes(true);
    configureFileView(listView_);

    tilesView_->setModel(sortModel_);
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
    copyShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    pasteShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copyShortcut_, &QShortcut::activated, this, [this] {
        if (addressBar_->hasFocus()) {
            return;
        }
        copySelectionToClipboard();
    });
    connect(pasteShortcut_, &QShortcut::activated, this, [this] {
        if (addressBar_->hasFocus()) {
            return;
        }
        pasteFromClipboard();
    });
    connect(detailsView_->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int section) {
        const QString key = section == 1 ? QStringLiteral("size")
            : section == 2 ? QStringLiteral("type")
            : section == 3 ? QStringLiteral("modified") : QStringLiteral("name");
        const Qt::SortOrder order = key == sortColumnKey_ && sortOrder_ == Qt::AscendingOrder
            ? Qt::DescendingOrder : Qt::AscendingOrder;
        setSort(key, order);
    });
}

FileBrowserWidget::~FileBrowserWidget() = default;

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
    const QModelIndex viewRoot = toViewIndex(rootIndex);
    detailsView_->setRootIndex(viewRoot);
    listView_->setRootIndex(viewRoot);
    tilesView_->setRootIndex(viewRoot);
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
        target->setRootIndex(viewIndexForPath(currentPath_));
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

QFileSystemModel *FileBrowserWidget::fileModel() const {
    return model_;
}

QModelIndex FileBrowserWidget::viewIndexForPath(const QString &path) const {
    return toViewIndex(model_->index(path));
}

QString FileBrowserWidget::sortColumnKey() const {
    return sortColumnKey_;
}

QString FileBrowserWidget::sortOrderKey() const {
    return sortOrder_ == Qt::DescendingOrder ? QStringLiteral("descending") : QStringLiteral("ascending");
}

void FileBrowserWidget::setSort(const QString &column, Qt::SortOrder order) {
    sortColumnKey_ = keyForSortColumn(sortColumnForKey(column));
    sortOrder_ = order;
    sortModel_->setSortColumn(sortColumnForKey(sortColumnKey_));
    sortModel_->sort(0, sortOrder_);
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

void FileBrowserWidget::setTerminalLauncherForTests(TerminalLauncher launcher) {
    terminalLauncher_ = launcher;
    if (terminalService_ != nullptr) {
        terminalService_->setLauncher(std::move(launcher));
    }
}

void FileBrowserWidget::setPlatformServicesForTests(PlatformServices *platformServices) {
    fileOperationService_.reset();
    ownedPlatformServices_.reset();
    platformServicesForTests_ = platformServices;
    if (platformServicesForTests_ == nullptr) {
        ownedPlatformServices_ = std::make_unique<PlatformServices>();
        platformServicesForTests_ = ownedPlatformServices_.get();
    }
    fileOperationService_ = std::make_unique<FileOperationService>(platformServicesForTests_);
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

    const QModelIndex sourceIndex = toSourceIndex(index);
    const QString path = model_->filePath(sourceIndex);
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

    const QString path = model_->filePath(toSourceIndex(indexes.first()));
    if (!path.isEmpty()) {
        emit selectedPathChanged(path);
    }
}

void FileBrowserWidget::showContextMenu(const QPoint &position) {
    QAbstractItemView *view = activeView();
    const QModelIndex index = view->indexAt(position);
    const QModelIndex sourceIndex = toSourceIndex(index);
    const QString path = sourceIndex.isValid() ? model_->filePath(sourceIndex) : currentPath_;
    const QFileInfo info(path);
    const bool clickedIndexIsSelected = index.isValid() && view->selectionModel() != nullptr && view->selectionModel()->isSelected(index);

    QMenu menu(this);
    menu.setObjectName("fileContextMenu");
    QAction *openAction = menu.addAction(tr("Open"));
    openAction->setObjectName("openAction");
    QAction *openInNewTabAction = menu.addAction(tr("Open in New Tab"));
    openInNewTabAction->setObjectName("openInNewTabAction");
    QAction *openWithAction = menu.addAction(tr("Open With..."));
    openWithAction->setObjectName("openWithAction");
    QAction *setDefaultAppAction = menu.addAction(tr("Set Default App for This Type..."));
    setDefaultAppAction->setObjectName("setDefaultAppAction");
    QAction *clearDefaultAppAction = menu.addAction(tr("Clear Default App for This Type"));
    clearDefaultAppAction->setObjectName("clearDefaultAppAction");
    const QString extension = extensionForPath(path);
    const bool isFile = index.isValid() && info.isFile();
    openAction->setEnabled(index.isValid());
    openInNewTabAction->setEnabled(info.isDir());
    openWithAction->setEnabled(isFile);
    setDefaultAppAction->setEnabled(isFile && !extension.isEmpty());
    clearDefaultAppAction->setEnabled(isFile && openWithDefaults_.contains(extension));
    menu.addSeparator();
    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setObjectName("copyAction");
    copyAction->setEnabled(index.isValid());
    QAction *pasteAction = menu.addAction(tr("Paste"));
    pasteAction->setObjectName("pasteAction");
    pasteAction->setEnabled(!localClipboardPaths().isEmpty());
    QAction *moveAction = menu.addAction(tr("Move"));
    moveAction->setObjectName("moveAction");
    QAction *renameAction = menu.addAction(tr("Rename"));
    renameAction->setObjectName("renameAction");
    QAction *deleteAction = menu.addAction(tr("Delete to Trash"));
    deleteAction->setObjectName("deleteAction");
    menu.addSeparator();
    QMenu *sortMenu = menu.addMenu(tr("Sort by"));
    sortMenu->setObjectName("sortMenu");
    QActionGroup *sortGroup = new QActionGroup(sortMenu);
    sortGroup->setExclusive(true);
    const QList<QPair<QString, QString>> sortOptions = {
        {QStringLiteral("name"), tr("Name")}, {QStringLiteral("type"), tr("Type")},
        {QStringLiteral("size"), tr("Size")}, {QStringLiteral("modified"), tr("Modified Time")},
        {QStringLiteral("created"), tr("Creation Time")}
    };
    for (const auto &[key, label] : sortOptions) {
        QAction *action = sortMenu->addAction(label);
        action->setObjectName(QStringLiteral("sortBy%1Action").arg(label.simplified().remove(QLatin1Char(' '))));
        action->setCheckable(true);
        action->setChecked(sortColumnKey_ == key);
        sortGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, key] { setSort(key, sortOrder_); });
    }
    QMenu *orderMenu = menu.addMenu(tr("Sort Order"));
    orderMenu->setObjectName("sortOrderMenu");
    QActionGroup *orderGroup = new QActionGroup(orderMenu);
    orderGroup->setExclusive(true);
    QAction *ascendingAction = orderMenu->addAction(tr("Ascending"));
    ascendingAction->setObjectName("ascendingSortAction");
    ascendingAction->setCheckable(true);
    ascendingAction->setChecked(sortOrder_ == Qt::AscendingOrder);
    orderGroup->addAction(ascendingAction);
    QAction *descendingAction = orderMenu->addAction(tr("Descending"));
    descendingAction->setObjectName("descendingSortAction");
    descendingAction->setCheckable(true);
    descendingAction->setChecked(sortOrder_ == Qt::DescendingOrder);
    orderGroup->addAction(descendingAction);
    connect(ascendingAction, &QAction::triggered, this, [this] { setSort(sortColumnKey_, Qt::AscendingOrder); });
    connect(descendingAction, &QAction::triggered, this, [this] { setSort(sortColumnKey_, Qt::DescendingOrder); });

    QMenu *newMenu = menu.addMenu(tr("New"));
    newMenu->setObjectName("newMenu");
    QAction *newFolderAction = newMenu->addAction(tr("New Folder"));
    newFolderAction->setObjectName("newFolderAction");
    QAction *newTextFileAction = newMenu->addAction(tr("New Text File"));
    newTextFileAction->setObjectName("newTextFileAction");
    moveAction->setEnabled(index.isValid());
    renameAction->setEnabled(index.isValid());
    deleteAction->setEnabled(index.isValid());
    const QString targetDirectory = info.isDir() && index.isValid() ? path : currentPath_;
    QAction *openInTerminalAction = menu.addAction(tr("Open in Terminal"));
    openInTerminalAction->setObjectName("openInTerminalAction");
    openInTerminalAction->setEnabled(!targetDirectory.isEmpty() && QFileInfo(targetDirectory).isDir());
    QAction *propertiesAction = menu.addAction(tr("Properties"));
    propertiesAction->setObjectName("propertiesAction");
    propertiesAction->setEnabled(index.isValid());
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
    } else if (selectedAction == copyAction && index.isValid()) {
        copySelectionToClipboard(path, clickedIndexIsSelected);
    } else if (selectedAction == pasteAction) {
        pasteFromClipboard(targetDirectory);
    } else if (selectedAction == newFolderAction) {
        createFolderFromMenu(targetDirectory);
    } else if (selectedAction == newTextFileAction) {
        createTextFileFromMenu(targetDirectory);
    } else if (selectedAction == openInTerminalAction) {
        openTargetInTerminal(targetDirectory);
    } else if (selectedAction == moveAction && index.isValid()) {
        movePathFromMenu(path);
    } else if (selectedAction == renameAction && index.isValid()) {
        renamePathFromMenu(path);
    } else if (selectedAction == deleteAction && index.isValid()) {
        deletePathsFromMenu(path, clickedIndexIsSelected);
    } else if (selectedAction == propertiesAction && index.isValid()) {
        showPropertiesFromMenu(path);
    }
}

QStringList FileBrowserWidget::selectedPaths() const {
    QAbstractItemView *view = activeView();
    if (view == nullptr || view->selectionModel() == nullptr) {
        return {};
    }

    QStringList paths;
    for (const QModelIndex &index : view->selectionModel()->selectedRows()) {
        const QString path = model_->filePath(toSourceIndex(index));
        if (!path.isEmpty() && !paths.contains(path)) {
            paths.append(path);
        }
    }
    return paths;
}

void FileBrowserWidget::copySelectionToClipboard(const QString &clickedPath, bool useSelection) {
    QStringList paths = useSelection ? selectedPaths() : QStringList();
    if (paths.isEmpty() && !clickedPath.isEmpty() && QFileInfo(clickedPath).exists()) {
        paths.append(clickedPath);
    }
    if (paths.isEmpty()) {
        return;
    }

    auto *mimeData = new QMimeData();
    QList<QUrl> urls;
    for (const QString &path : paths) {
        urls.append(QUrl::fromLocalFile(path));
    }
    mimeData->setUrls(urls);
    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void FileBrowserWidget::pasteFromClipboard(const QString &destination) {
    const QStringList paths = localClipboardPaths();
    if (paths.isEmpty()) {
        return;
    }

    const QString targetDirectory = destination.isEmpty() ? currentPath_ : destination;
    QString error;
    if (!fileOperationService_->copyWithAutoRename(paths, targetDirectory, &error)) {
        showOperationError(error);
        return;
    }
    refreshCurrentDirectory();
}

QString FileBrowserWidget::targetDirectoryForPath(const QString &path) const {
    const QFileInfo info(path);
    return info.isDir() ? info.absoluteFilePath() : info.absolutePath();
}

void FileBrowserWidget::createFolderFromMenu(const QString &parentDir) {
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal, tr("New Folder"), &accepted);
    if (!accepted) {
        return;
    }

    QString error;
    if (!fileOperationService_->createFolder(parentDir, name, &error)) {
        showOperationError(error);
        return;
    }
    refreshCurrentDirectory();
}

void FileBrowserWidget::createTextFileFromMenu(const QString &parentDir) {
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("New Text File"), tr("File name:"), QLineEdit::Normal, tr("New Text File.txt"), &accepted);
    if (!accepted) {
        return;
    }

    QString error;
    if (!fileOperationService_->createTextFile(parentDir, name, &error)) {
        showOperationError(error);
        return;
    }
    refreshCurrentDirectory();
}

void FileBrowserWidget::movePathFromMenu(const QString &path) {
    const QString destination = QFileDialog::getExistingDirectory(this, tr("Move To"), currentPath_);
    if (destination.isEmpty()) {
        return;
    }

    QString error;
    if (!fileOperationService_->move({path}, destination, &error)) {
        showOperationError(error);
        return;
    }
    refreshCurrentDirectory();
}

void FileBrowserWidget::renamePathFromMenu(const QString &path) {
    const QFileInfo info(path);
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, info.fileName(), &accepted);
    if (!accepted) {
        return;
    }

    QString error;
    if (!fileOperationService_->renamePath(path, name, &error)) {
        showOperationError(error);
        return;
    }
    refreshCurrentDirectory();
}

void FileBrowserWidget::deletePathsFromMenu(const QString &clickedPath, bool useSelection) {
    QStringList paths = useSelection ? selectedPaths() : QStringList();
    if (paths.isEmpty() && !clickedPath.isEmpty()) {
        paths.append(clickedPath);
    }
    if (paths.isEmpty()) {
        return;
    }

    QString error;
    if (!fileOperationService_->deleteToTrash(paths, &error)) {
        showOperationError(error);
        return;
    }
    refreshCurrentDirectory();
}

void FileBrowserWidget::showPropertiesFromMenu(const QString &path) {
    QString error;
    if (!platformServicesForTests_->showProperties(path, &error)) {
        showOperationError(error);
    }
}

void FileBrowserWidget::showOperationError(const QString &error) {
    emit errorOccurred(error);
}

void FileBrowserWidget::openTargetInTerminal(const QString &path) {
    const QString targetDirectory = targetDirectoryForPath(path);
    QString error;
    if (!terminalService_->open(targetDirectory, &error)) {
        showOperationError(error);
    }
}

QModelIndex FileBrowserWidget::toSourceIndex(const QModelIndex &index) const {
    if (!index.isValid()) {
        return {};
    }
    return index.model() == sortModel_ ? sortModel_->mapToSource(index) : index;
}

QModelIndex FileBrowserWidget::toViewIndex(const QModelIndex &index) const {
    if (!index.isValid()) {
        return {};
    }
    return sortModel_ == nullptr ? index : sortModel_->mapFromSource(index);
}
