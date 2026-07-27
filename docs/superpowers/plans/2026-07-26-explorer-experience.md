# Explorer Experience Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an integrated Explorer-like browsing experience with view modes, breadcrumb address navigation, link-style favorites, plus-tab creation, and app-managed open-with defaults.

**Architecture:** Progressively enhance the existing Qt Widgets structure. Keep browsing behavior in `FileBrowserWidget`, tab lifecycle in `TabManager`/`TabStrip`, favorites interaction in `FavoritesSidebar`, and persistent app preferences in `SettingsStore`.

**Tech Stack:** C++20, Qt 6.11 Widgets/Core/Gui/Test, CMake, Qt Test.

---

## File Structure

- Modify: `src/services/SettingsStore.h` - add `openWithDefaults` to `AppSettings`.
- Modify: `src/services/SettingsStore.cpp` - serialize, validate, and load `openWithDefaults`.
- Modify: `tests/test_settings_store.cpp` - cover open-with settings persistence and invalid shapes.
- Modify: `src/ui/FileBrowserWidget.h` - add `ViewMode`, multiple view accessors, breadcrumb/edit APIs, and open-with defaults API.
- Modify: `src/ui/FileBrowserWidget.cpp` - implement stacked views, breadcrumb controls, Ctrl+L edit mode, view mode buttons, and app-managed open-with launch resolution.
- Modify: `tests/test_filebrowser.cpp` - cover view switching, breadcrumb navigation, Ctrl+L behavior, and open-with launch seams.
- Modify: `src/ui/FavoritesSidebar.cpp` - make favorites activate on single click and style rows like link buttons.
- Modify: `tests/test_favorites_sidebar.cpp` - cover single-click activation and unavailable favorite suppression.
- Modify: `src/ui/TabStrip.cpp` - move the existing `+` action into the tab bar corner if needed and keep home-path creation.
- Modify: `tests/test_mainwindow.cpp` or `tests/test_filebrowser.cpp` - verify plus-tab behavior remains accessible and creates a home tab.
- Modify: `CMakeLists.txt` only if a new helper file is introduced during implementation; prefer keeping helpers private to existing files for this pass.

## Task 1: Persist App-Managed Open-With Defaults

**Files:**
- Modify: `src/services/SettingsStore.h`
- Modify: `src/services/SettingsStore.cpp`
- Test: `tests/test_settings_store.cpp`

- [ ] **Step 1: Write failing settings tests**

Add these includes if missing in `tests/test_settings_store.cpp`:

```cpp
#include <QJsonDocument>
#include <QJsonObject>
```

Add these private slots to `SettingsStoreTest`:

```cpp
void savesAndLoadsOpenWithDefaults();
void rejectsInvalidOpenWithDefaults();
```

Add these test bodies:

```cpp
void SettingsStoreTest::savesAndLoadsOpenWithDefaults() {
    QTemporaryDir configDir;
    QVERIFY(configDir.isValid());
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());

    AppSettings settings;
    settings.openWithDefaults.insert(QStringLiteral(".txt"), QStringLiteral("/usr/bin/kate"));
    settings.openWithDefaults.insert(QStringLiteral(".png"), QStringLiteral("C:/Program Files/Viewer/viewer.exe"));

    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    AppSettings loaded;
    QVERIFY2(store.load(loaded, &error), qPrintable(error));
    QCOMPARE(loaded.openWithDefaults.value(QStringLiteral(".txt")), QStringLiteral("/usr/bin/kate"));
    QCOMPARE(loaded.openWithDefaults.value(QStringLiteral(".png")), QStringLiteral("C:/Program Files/Viewer/viewer.exe"));
}

void SettingsStoreTest::rejectsInvalidOpenWithDefaults() {
    QTemporaryDir configDir;
    QVERIFY(configDir.isValid());
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());

    SettingsStore store;
    QFile settingsFile(store.settingsPath());
    QVERIFY(QDir().mkpath(QFileInfo(settingsFile).absolutePath()));
    QVERIFY(settingsFile.open(QIODevice::WriteOnly));
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("openWithDefaults"), QJsonObject{{QStringLiteral(".txt"), 42}}}};
    settingsFile.write(QJsonDocument(root).toJson());
    settingsFile.close();

    AppSettings loaded;
    QString error;
    QVERIFY(!store.load(loaded, &error));
    QVERIFY2(error.contains(QStringLiteral("openWithDefaults")), qPrintable(error));
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R SettingsStoreTest
```

Expected: compile fails because `AppSettings::openWithDefaults` does not exist.

- [ ] **Step 3: Add settings field**

In `src/services/SettingsStore.h`, add the include and field:

```cpp
#include <QHash>

struct AppSettings {
    int version = 1;
    QByteArray windowGeometry;
    QByteArray windowState;
    QVector<int> splitterSizes;
    QVector<TabState> tabs;
    QVector<FavoriteState> favorites;
    QHash<QString, QString> openWithDefaults;
    bool showHiddenFiles = false;
    bool confirmDeleteToTrash = true;
};
```

- [ ] **Step 4: Implement JSON read/write**

In `src/services/SettingsStore.cpp`, add helpers near the other JSON helpers:

```cpp
QJsonObject openWithDefaultsToJson(const QHash<QString, QString> &defaults) {
    QJsonObject object;
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        object.insert(it.key(), it.value());
    }
    return object;
}

bool readOpenWithDefaults(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    const QJsonValue value = root.value("openWithDefaults");
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isObject()) {
        setError(errorMessage, "Invalid settings: openWithDefaults must be an object.");
        return false;
    }

    settings.openWithDefaults.clear();
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!it.value().isString()) {
            setError(errorMessage, "Invalid settings: openWithDefaults values must be strings.");
            return false;
        }
        if (!it.key().startsWith(QStringLiteral("."))) {
            setError(errorMessage, "Invalid settings: openWithDefaults keys must be file extensions.");
            return false;
        }
        settings.openWithDefaults.insert(it.key().toLower(), it.value().toString());
    }
    return true;
}
```

Update `readSettingsObject()`:

```cpp
bool readSettingsObject(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    settings = AppSettings{};
    return readVersion(root, settings, errorMessage) && readWindowObject(root, settings, errorMessage) && readTabs(root, settings, errorMessage)
        && readFavorites(root, settings, errorMessage) && readOpenWithDefaults(root, settings, errorMessage) && readOptions(root, settings, errorMessage);
}
```

Update `SettingsStore::save()` before creating `QSaveFile`:

```cpp
root["openWithDefaults"] = openWithDefaultsToJson(settings.openWithDefaults);
```

- [ ] **Step 5: Run settings tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R SettingsStoreTest
```

Expected: `SettingsStoreTest` passes.

- [ ] **Step 6: Commit**

```bash
git add src/services/SettingsStore.h src/services/SettingsStore.cpp tests/test_settings_store.cpp
git commit -m "add open-with defaults to settings"
```

## Task 2: Add File View Modes

**Files:**
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Test: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Write failing view-mode tests**

Add includes to `tests/test_filebrowser.cpp`:

```cpp
#include <QListView>
#include <QStackedWidget>
```

Add private slots:

```cpp
void switchesBetweenDetailsListAndTilesViews();
void viewModeSwitchPreservesCurrentPath();
```

Add test bodies:

```cpp
void FileBrowserWidgetTest::switchesBetweenDetailsListAndTilesViews() {
    FileBrowserWidget browser;

    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::Details);
    QVERIFY(browser.detailsView()->isVisible() || browser.detailsView()->parentWidget() != nullptr);

    browser.setViewMode(FileBrowserWidget::ViewMode::List);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::List);
    QCOMPARE(browser.activeView(), browser.listView());
    QCOMPARE(browser.listView()->viewMode(), QListView::ListMode);

    browser.setViewMode(FileBrowserWidget::ViewMode::Tiles);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::Tiles);
    QCOMPARE(browser.activeView(), browser.tilesView());
    QCOMPARE(browser.tilesView()->viewMode(), QListView::IconMode);
}

void FileBrowserWidgetTest::viewModeSwitchPreservesCurrentPath() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(directory.path()));

    browser.setViewMode(FileBrowserWidget::ViewMode::Tiles);
    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(browser.activeView()->rootIndex().isValid());

    browser.setViewMode(FileBrowserWidget::ViewMode::Details);
    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(browser.activeView()->rootIndex().isValid());
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: compile fails because view-mode APIs do not exist.

- [ ] **Step 3: Extend `FileBrowserWidget` interface**

In `src/ui/FileBrowserWidget.h`, add forward declarations:

```cpp
class QAbstractItemView;
class QListView;
class QStackedWidget;
```

Update the public section:

```cpp
enum class ViewMode {
    Details,
    List,
    Tiles
};

ViewMode viewMode() const;
void setViewMode(ViewMode mode);
QAbstractItemView *activeView() const;
QTableView *detailsView() const;
QListView *listView() const;
QListView *tilesView() const;
QTableView *view() const;
```

Update private members:

```cpp
QStackedWidget *viewStack_ = nullptr;
QTableView *detailsView_ = nullptr;
QListView *listView_ = nullptr;
QListView *tilesView_ = nullptr;
ViewMode viewMode_ = ViewMode::Details;
```

- [ ] **Step 4: Implement stacked views**

In `src/ui/FileBrowserWidget.cpp`, add includes:

```cpp
#include <QAbstractItemView>
#include <QListView>
#include <QStackedWidget>
```

Replace the single `view_` construction with:

```cpp
, viewStack_(new QStackedWidget(this))
, detailsView_(new QTableView(this))
, listView_(new QListView(this))
, tilesView_(new QListView(this))
```

Configure all views with this helper in the anonymous namespace:

```cpp
void configureFileView(QAbstractItemView *view) {
    view->setDragEnabled(true);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
}
```

In the constructor after `model_->setRootPath(...)`:

```cpp
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
```

Replace `layout->addWidget(view_, 1);` with:

```cpp
layout->addWidget(viewStack_, 1);
```

Update connections for each view:

```cpp
const auto connectView = [this](QAbstractItemView *view) {
    connect(view, &QAbstractItemView::doubleClicked, this, &FileBrowserWidget::openIndex);
    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FileBrowserWidget::emitSelectedPath);
    connect(view, &QWidget::customContextMenuRequested, this, &FileBrowserWidget::showContextMenu);
};
connectView(detailsView_);
connectView(listView_);
connectView(tilesView_);
```

Add methods:

```cpp
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
```

Update `setCurrentPath()` to set all root indexes:

```cpp
const QModelIndex rootIndex = model_->setRootPath(absolutePath);
detailsView_->setRootIndex(rootIndex);
listView_->setRootIndex(rootIndex);
tilesView_->setRootIndex(rootIndex);
```

Update context menu coordinate mapping:

```cpp
QAbstractItemView *view = activeView();
const QModelIndex index = view->indexAt(position);
QAction *selectedAction = menu.exec(view->viewport()->mapToGlobal(position));
```

- [ ] **Step 5: Run file browser tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: `FileBrowserWidgetTest` passes.

- [ ] **Step 6: Commit**

```bash
git add src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp tests/test_filebrowser.cpp
git commit -m "add switchable file view modes"
```

## Task 3: Add View Mode Controls to the Browser Chrome

**Files:**
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Test: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Write failing control tests**

Add private slot:

```cpp
void viewModeButtonsChangeActiveView();
```

Add test body:

```cpp
void FileBrowserWidgetTest::viewModeButtonsChangeActiveView() {
    FileBrowserWidget browser;

    auto *listButton = browser.findChild<QToolButton *>("listViewButton");
    auto *detailsButton = browser.findChild<QToolButton *>("detailsViewButton");
    auto *tilesButton = browser.findChild<QToolButton *>("tilesViewButton");
    QVERIFY(listButton != nullptr);
    QVERIFY(detailsButton != nullptr);
    QVERIFY(tilesButton != nullptr);

    QTest::mouseClick(listButton, Qt::LeftButton);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::List);

    QTest::mouseClick(tilesButton, Qt::LeftButton);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::Tiles);

    QTest::mouseClick(detailsButton, Qt::LeftButton);
    QCOMPARE(browser.viewMode(), FileBrowserWidget::ViewMode::Details);
}
```

- [ ] **Step 2: Run test to verify failure**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: test fails because the buttons are not present.

- [ ] **Step 3: Add buttons and connections**

In `src/ui/FileBrowserWidget.h`, add private members:

```cpp
QToolButton *listViewButton_ = nullptr;
QToolButton *detailsViewButton_ = nullptr;
QToolButton *tilesViewButton_ = nullptr;
```

In `src/ui/FileBrowserWidget.cpp` constructor initializer list:

```cpp
, listViewButton_(new QToolButton(this))
, detailsViewButton_(new QToolButton(this))
, tilesViewButton_(new QToolButton(this))
```

After up button setup:

```cpp
listViewButton_->setObjectName("listViewButton");
listViewButton_->setText(tr("List"));
listViewButton_->setToolTip(tr("List view"));

detailsViewButton_->setObjectName("detailsViewButton");
detailsViewButton_->setText(tr("Details"));
detailsViewButton_->setToolTip(tr("Details view"));

tilesViewButton_->setObjectName("tilesViewButton");
tilesViewButton_->setText(tr("Tiles"));
tilesViewButton_->setToolTip(tr("Tiles view"));
```

Add buttons to `addressLayout` after the address widget:

```cpp
addressLayout->addWidget(detailsViewButton_);
addressLayout->addWidget(listViewButton_);
addressLayout->addWidget(tilesViewButton_);
```

Add connections:

```cpp
connect(detailsViewButton_, &QToolButton::clicked, this, [this] { setViewMode(ViewMode::Details); });
connect(listViewButton_, &QToolButton::clicked, this, [this] { setViewMode(ViewMode::List); });
connect(tilesViewButton_, &QToolButton::clicked, this, [this] { setViewMode(ViewMode::Tiles); });
```

Update the stylesheet string to include:

```cpp
"QToolButton#listViewButton, QToolButton#detailsViewButton, QToolButton#tilesViewButton { background: #ffffff; border: 1px solid #c8d2df; border-radius: 8px; padding: 5px 10px; }"
"QToolButton#listViewButton:hover, QToolButton#detailsViewButton:hover, QToolButton#tilesViewButton:hover { background: #eaf1f8; }"
```

- [ ] **Step 4: Run test to verify pass**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: `FileBrowserWidgetTest` passes.

- [ ] **Step 5: Commit**

```bash
git add src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp tests/test_filebrowser.cpp
git commit -m "add file view mode controls"
```

## Task 4: Replace Always-Visible Path Input with Breadcrumb and Ctrl+L Edit Mode

**Files:**
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Test: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Write failing breadcrumb tests**

Add includes:

```cpp
#include <QLabel>
#include <QScrollArea>
#include <QShortcut>
```

Add private slots:

```cpp
void showsBreadcrumbByDefaultAndPathEditorOnCtrlL();
void breadcrumbButtonNavigatesToAncestor();
void escapeLeavesPathEditModeWithoutNavigation();
```

Add test bodies:

```cpp
void FileBrowserWidgetTest::showsBreadcrumbByDefaultAndPathEditorOnCtrlL() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(directory.path()));

    QVERIFY(browser.breadcrumbContainer()->isVisible());
    QVERIFY(!browser.addressBar()->isVisible());

    QTest::keyClick(&browser, Qt::Key_L, Qt::ControlModifier);
    QVERIFY(browser.addressBar()->isVisible());
    QCOMPARE(browser.addressBar()->text(), directory.path());
    QVERIFY(!browser.breadcrumbContainer()->isVisible());
}

void FileBrowserWidgetTest::breadcrumbButtonNavigatesToAncestor() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("parent/child"));
    const QString parentPath = QDir(root.path()).filePath("parent");
    const QString childPath = QDir(root.path()).filePath("parent/child");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(childPath));

    QToolButton *parentButton = nullptr;
    const auto buttons = browser.breadcrumbContainer()->findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button->property("path").toString() == parentPath) {
            parentButton = button;
            break;
        }
    }
    QVERIFY(parentButton != nullptr);

    QTest::mouseClick(parentButton, Qt::LeftButton);
    QCOMPARE(browser.currentPath(), parentPath);
}

void FileBrowserWidgetTest::escapeLeavesPathEditModeWithoutNavigation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString missing = QDir(directory.path()).filePath("missing");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(directory.path()));

    QTest::keyClick(&browser, Qt::Key_L, Qt::ControlModifier);
    browser.addressBar()->setText(missing);
    QTest::keyClick(browser.addressBar(), Qt::Key_Escape);

    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(browser.breadcrumbContainer()->isVisible());
    QVERIFY(!browser.addressBar()->isVisible());
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: compile fails because breadcrumb APIs do not exist.

- [ ] **Step 3: Add breadcrumb/edit interface**

In `src/ui/FileBrowserWidget.h`, add forward declarations:

```cpp
class QHBoxLayout;
class QShortcut;
```

Add public methods:

```cpp
QWidget *breadcrumbContainer() const;
```

Add private slots:

```cpp
void enterAddressEditMode();
void leaveAddressEditMode();
```

Add private helpers and members:

```cpp
void rebuildBreadcrumbs();
QStringList pathSegments(const QString &path) const;

QWidget *breadcrumbContainer_ = nullptr;
QHBoxLayout *breadcrumbLayout_ = nullptr;
QShortcut *focusAddressShortcut_ = nullptr;
```

- [ ] **Step 4: Implement breadcrumb control**

In `src/ui/FileBrowserWidget.cpp`, add includes:

```cpp
#include <QEvent>
#include <QKeyEvent>
#include <QShortcut>
```

In the constructor initializer list:

```cpp
, breadcrumbContainer_(new QWidget(this))
, focusAddressShortcut_(new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this))
```

Build the breadcrumb container before adding address widgets:

```cpp
breadcrumbContainer_->setObjectName("breadcrumbContainer");
breadcrumbLayout_ = new QHBoxLayout(breadcrumbContainer_);
breadcrumbLayout_->setContentsMargins(0, 0, 0, 0);
breadcrumbLayout_->setSpacing(4);

addressBar_->hide();
addressBar_->installEventFilter(this);
addressLayout->addWidget(breadcrumbContainer_, 1);
addressLayout->addWidget(addressBar_, 1);
```

Add connections:

```cpp
connect(focusAddressShortcut_, &QShortcut::activated, this, &FileBrowserWidget::enterAddressEditMode);
```

Add method implementations:

```cpp
QWidget *FileBrowserWidget::breadcrumbContainer() const {
    return breadcrumbContainer_;
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

void FileBrowserWidget::rebuildBreadcrumbs() {
    while (QLayoutItem *item = breadcrumbLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const QString cleanPath = QDir::cleanPath(currentPath_);
    QDir dir(cleanPath);
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
```

Update `setCurrentPath()` after `currentPath_ = absolutePath;`:

```cpp
addressBar_->setText(currentPath_);
rebuildBreadcrumbs();
leaveAddressEditMode();
```

Update `navigateFromAddressBar()`:

```cpp
void FileBrowserWidget::navigateFromAddressBar() {
    if (setCurrentPath(addressBar_->text())) {
        leaveAddressEditMode();
    }
}
```

Add an event filter declaration to the header and implementation:

```cpp
bool eventFilter(QObject *watched, QEvent *event) override;
```

```cpp
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
```

- [ ] **Step 5: Run tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: `FileBrowserWidgetTest` passes.

- [ ] **Step 6: Commit**

```bash
git add src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp tests/test_filebrowser.cpp
git commit -m "add breadcrumb address navigation"
```

## Task 5: Make Favorites Behave Like Link Buttons

**Files:**
- Modify: `src/ui/FavoritesSidebar.cpp`
- Test: `tests/test_favorites_sidebar.cpp`

- [ ] **Step 1: Write failing single-click test**

In `tests/test_favorites_sidebar.cpp`, add or update tests with this behavior:

```cpp
void FavoritesSidebarTest::singleClickActivatesAvailableFavorite() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FavoritesModel model;
    model.addFavorite(QStringLiteral("Project"), directory.path());

    FavoritesSidebar sidebar;
    sidebar.setModel(&model);
    sidebar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sidebar));

    QSignalSpy activatedSpy(&sidebar, &FavoritesSidebar::favoriteActivated);
    const QModelIndex index = model.index(0, 0);
    const QRect rect = sidebar.listView()->visualRect(index);
    QVERIFY(rect.isValid());

    QTest::mouseClick(sidebar.listView()->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(activatedSpy.takeFirst().at(0).toString(), directory.path());
}
```

If there is already a missing-favorite test, keep it. If not, add:

```cpp
void FavoritesSidebarTest::singleClickDoesNotActivateMissingFavorite() {
    FavoritesModel model;
    model.addFavorite(QStringLiteral("Missing"), QStringLiteral("/path/that/does/not/exist"));

    FavoritesSidebar sidebar;
    sidebar.setModel(&model);
    sidebar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sidebar));

    QSignalSpy activatedSpy(&sidebar, &FavoritesSidebar::favoriteActivated);
    const QModelIndex index = model.index(0, 0);
    const QRect rect = sidebar.listView()->visualRect(index);
    QVERIFY(rect.isValid());

    QTest::mouseClick(sidebar.listView()->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

    QCOMPARE(activatedSpy.count(), 0);
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FavoritesSidebarTest
```

Expected: single-click test fails if only activated/double-click signals are wired.

- [ ] **Step 3: Implement single-click link behavior**

In `src/ui/FavoritesSidebar.cpp`, update constructor setup:

```cpp
listView_->setCursor(Qt::PointingHandCursor);
listView_->setSelectionMode(QAbstractItemView::SingleSelection);
listView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
listView_->setStyleSheet(QStringLiteral(
    "QListView#favoritesListView { background: transparent; border: none; outline: none; }"
    "QListView#favoritesListView::item { margin: 3px 0; padding: 7px 10px; border-radius: 8px; color: #1f5f99; }"
    "QListView#favoritesListView::item:hover { background: #eaf3ff; }"
    "QListView#favoritesListView::item:selected { background: #dcecff; color: #174f82; }"));
connect(listView_, &QListView::clicked, this, &FavoritesSidebar::activateFavorite);
```

Keep existing `activated` and `doubleClicked` connections so keyboard and double-click continue to work.

- [ ] **Step 4: Run tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FavoritesSidebarTest
```

Expected: `FavoritesSidebarTest` passes.

- [ ] **Step 5: Commit**

```bash
git add src/ui/FavoritesSidebar.cpp tests/test_favorites_sidebar.cpp
git commit -m "make favorites activate like links"
```

## Task 6: Put Plus New-Tab Action on the Tab Bar

**Files:**
- Modify: `src/ui/TabStrip.cpp`
- Test: `tests/test_mainwindow.cpp` or `tests/test_filebrowser.cpp`

- [ ] **Step 1: Write or update plus-tab test**

Add a test that locates `newTabButton`, clicks it, and verifies the new tab opens at home:

```cpp
void FileBrowserWidgetTest::newTabButtonCreatesHomeTab() {
    TabStrip strip;
    TabManager manager;
    strip.setTabManager(&manager);
    manager.restoreTabs(AppSettings{});

    auto *button = strip.findChild<QToolButton *>("newTabButton");
    QVERIFY(button != nullptr);

    QTest::mouseClick(button, Qt::LeftButton);

    QCOMPARE(manager.count(), 2);
    QCOMPARE(manager.browserAt(1)->currentPath(), QDir::homePath());
    QCOMPARE(manager.tabWidget()->currentIndex(), 1);
}
```

Add `#include "ui/TabStrip.h"` to the test file if using `tests/test_filebrowser.cpp`.

- [ ] **Step 2: Run test**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: test may already pass because `TabStrip` has a header-level `+` button. If it passes, continue to Step 3 to improve placement.

- [ ] **Step 3: Move plus button into tab corner**

In `src/ui/TabStrip.cpp`, keep `newTabButton_` but replace the separate header placement with a tab corner widget:

```cpp
tabWidget_->setObjectName("tabPlaceholder");
tabWidget_->setTabsClosable(true);
newTabButton_->setObjectName("newTabButton");
newTabButton_->setText(QStringLiteral("+"));
newTabButton_->setToolTip(tr("New tab"));
newTabButton_->setAutoRaise(true);
tabWidget_->setCornerWidget(newTabButton_, Qt::TopRightCorner);
layout->addWidget(tabWidget_, 1);
```

Remove the label/header layout only if doing so does not break existing tests that assert `tabStripLabel`. If tests require the label, keep the header label and also set the corner widget; do not add a second plus button.

- [ ] **Step 4: Run relevant tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R "FileBrowserWidgetTest|MainWindowTest"
```

Expected: plus-tab and main-window tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/ui/TabStrip.cpp tests/test_filebrowser.cpp tests/test_mainwindow.cpp
git commit -m "place new tab action on tab bar"
```

## Task 7: Add App-Managed Open-With Behavior

**Files:**
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Modify: `src/MainWindow.cpp`
- Test: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Write failing open-with tests**

Add private slots:

```cpp
void configuredDefaultAppOpensFileInsteadOfDesktopUrl();
void missingConfiguredDefaultFallsBackToDesktopUrl();
```

Add test bodies:

```cpp
void FileBrowserWidgetTest::configuredDefaultAppOpensFileInsteadOfDesktopUrl() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("document.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("content");
    file.close();

    const QString appPath = QDir(root.path()).filePath("fake-editor");
    QFile app(appPath);
    QVERIFY(app.open(QIODevice::WriteOnly));
    app.write("#!/bin/sh\n");
    app.close();
    QVERIFY(app.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    FileBrowserWidget browser;
    browser.setOpenWithDefaults({{QStringLiteral(".txt"), appPath}});

    QString launchedApp;
    QString launchedFile;
    browser.setOpenWithLauncherForTests([&](const QString &program, const QStringList &arguments) {
        launchedApp = program;
        launchedFile = arguments.value(0);
        return true;
    });

    QVERIFY(browser.setCurrentPath(root.path()));
    auto *model = qobject_cast<QFileSystemModel *>(browser.view()->model());
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = model->index(filePath);
    QVERIFY(fileIndex.isValid());

    QMetaObject::invokeMethod(browser.view(), "doubleClicked", Q_ARG(QModelIndex, fileIndex));

    QCOMPARE(launchedApp, appPath);
    QCOMPARE(launchedFile, filePath);
}

void FileBrowserWidgetTest::missingConfiguredDefaultFallsBackToDesktopUrl() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("document.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("content");
    file.close();

    UrlOpenHandler handler;
    QDesktopServices::setUrlHandler("file", &handler, "openUrl");

    FileBrowserWidget browser;
    browser.setOpenWithDefaults({{QStringLiteral(".txt"), QDir(root.path()).filePath("missing-editor")}});
    QVERIFY(browser.setCurrentPath(root.path()));

    auto *model = qobject_cast<QFileSystemModel *>(browser.view()->model());
    QVERIFY(model != nullptr);
    const QModelIndex fileIndex = model->index(filePath);
    QVERIFY(fileIndex.isValid());

    QMetaObject::invokeMethod(browser.view(), "doubleClicked", Q_ARG(QModelIndex, fileIndex));

    QDesktopServices::unsetUrlHandler("file");
    QCOMPARE(handler.openedUrls.size(), 1);
    QCOMPARE(handler.openedUrls.first(), QUrl::fromLocalFile(filePath));
}
```

- [ ] **Step 2: Run tests to verify failure**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: compile fails because open-with APIs do not exist.

- [ ] **Step 3: Add open-with APIs**

In `src/ui/FileBrowserWidget.h`, add includes:

```cpp
#include <QHash>
#include <functional>
```

Add public methods:

```cpp
using OpenWithLauncher = std::function<bool(const QString &program, const QStringList &arguments)>;

void setOpenWithDefaults(const QHash<QString, QString> &defaults);
QHash<QString, QString> openWithDefaults() const;
void setOpenWithLauncherForTests(OpenWithLauncher launcher);
```

Add signals:

```cpp
void openWithDefaultsChanged(const QHash<QString, QString> &defaults);
void errorOccurred(const QString &message);
```

Add private helpers and members:

```cpp
QString extensionForPath(const QString &path) const;
bool openFilePath(const QString &path);
bool launchConfiguredApplication(const QString &applicationPath, const QString &filePath);

QHash<QString, QString> openWithDefaults_;
OpenWithLauncher openWithLauncher_;
```

- [ ] **Step 4: Implement double-click resolution**

In `src/ui/FileBrowserWidget.cpp`, add includes:

```cpp
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
```

Add constructor default launcher:

```cpp
openWithLauncher_ = [](const QString &program, const QStringList &arguments) {
    return QProcess::startDetached(program, arguments);
};
```

Replace file open part of `openIndex()` with:

```cpp
if (info.exists()) {
    openFilePath(path);
}
```

Add methods:

```cpp
void FileBrowserWidget::setOpenWithDefaults(const QHash<QString, QString> &defaults) {
    openWithDefaults_ = defaults;
}

QHash<QString, QString> FileBrowserWidget::openWithDefaults() const {
    return openWithDefaults_;
}

void FileBrowserWidget::setOpenWithLauncherForTests(OpenWithLauncher launcher) {
    openWithLauncher_ = std::move(launcher);
}

QString FileBrowserWidget::extensionForPath(const QString &path) const {
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;
}

bool FileBrowserWidget::launchConfiguredApplication(const QString &applicationPath, const QString &filePath) {
    if (!QFileInfo::exists(applicationPath)) {
        emit errorOccurred(tr("Configured application does not exist: %1").arg(applicationPath));
        return false;
    }
    if (!openWithLauncher_ || !openWithLauncher_(applicationPath, {filePath})) {
        emit errorOccurred(tr("Could not open %1 with %2").arg(filePath, applicationPath));
        return false;
    }
    return true;
}

bool FileBrowserWidget::openFilePath(const QString &path) {
    const QString extension = extensionForPath(path);
    const QString configuredApplication = openWithDefaults_.value(extension);
    if (!configuredApplication.isEmpty() && launchConfiguredApplication(configuredApplication, path)) {
        return true;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
```

- [ ] **Step 5: Add context menu actions for open-with defaults**

In `showContextMenu()`, after Open and Open in New Tab actions:

```cpp
QAction *openWithAction = menu.addAction(tr("Open With..."));
QAction *setDefaultOpenWithAction = menu.addAction(tr("Set Default App for This Type..."));
QAction *clearDefaultOpenWithAction = menu.addAction(tr("Clear Default App for This Type"));
const QString extension = extensionForPath(path);
const bool fileSelected = index.isValid() && info.isFile();
openWithAction->setEnabled(fileSelected);
setDefaultOpenWithAction->setEnabled(fileSelected && !extension.isEmpty());
clearDefaultOpenWithAction->setEnabled(fileSelected && openWithDefaults_.contains(extension));
```

Handle actions after `menu.exec(...)`:

```cpp
} else if (selectedAction == openWithAction && fileSelected) {
    const QString application = QFileDialog::getOpenFileName(this, tr("Choose application"));
    if (!application.isEmpty()) {
        launchConfiguredApplication(application, path);
    }
} else if (selectedAction == setDefaultOpenWithAction && fileSelected) {
    const QString application = QFileDialog::getOpenFileName(this, tr("Choose default application"));
    if (!application.isEmpty()) {
        openWithDefaults_.insert(extension, application);
        emit openWithDefaultsChanged(openWithDefaults_);
    }
} else if (selectedAction == clearDefaultOpenWithAction && fileSelected) {
    openWithDefaults_.remove(extension);
    emit openWithDefaultsChanged(openWithDefaults_);
}
```

- [ ] **Step 6: Wire settings in `MainWindow`**

In `src/MainWindow.cpp`, when creating/restoring browsers, set defaults on each browser and persist changes. Add a helper or lambda near tab setup:

```cpp
const auto configureBrowser = [this](FileBrowserWidget *browser) {
    if (browser == nullptr) {
        return;
    }
    browser->setOpenWithDefaults(settings_.openWithDefaults);
    connect(browser, &FileBrowserWidget::openWithDefaultsChanged, this, [this](const QHash<QString, QString> &defaults) {
        settings_.openWithDefaults = defaults;
        persistSettings();
    });
};
```

If `MainWindow` does not keep an `AppSettings settings_` member, add one in `src/MainWindow.h` and assign it after load. Apply the lambda to existing restored tabs and any newly created tabs. If `TabManager` is the only place that creates tabs, add a `tabAdded(FileBrowserWidget *)` signal to `TabManager` and emit it at the end of `addTab()`:

```cpp
emit tabAdded(browser);
```

Connect `tabAdded` in `MainWindow` to configure each new browser.

- [ ] **Step 7: Run tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R "FileBrowserWidgetTest|MainWindowTest|SettingsStoreTest"
```

Expected: selected tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp src/MainWindow.h src/MainWindow.cpp src/services/TabManager.h src/services/TabManager.cpp tests/test_filebrowser.cpp
git commit -m "add app-managed open with defaults"
```

## Task 8: Final Integration and Regression Verification

**Files:**
- Modify only files required by failing tests discovered during this task.

- [ ] **Step 1: Run full test suite**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 2: Manual offscreen smoke run**

Run:

```bash
QT_QPA_PLATFORM=offscreen ./build/filemanager_app --help
```

Expected: the app starts or exits without a crash. If `--help` is not handled by the app, run the existing smoke test instead:

```bash
ctest --test-dir build --output-on-failure -R filemanager_smoke_test
```

- [ ] **Step 3: Inspect working tree**

Run:

```bash
git status --short
git diff --stat
```

Expected: only intentional source, test, spec, and plan changes are present. Do not stage `.qtcreator/CMakeLists.txt.user` because it predates this work and is unrelated.

- [ ] **Step 4: Final commit if any integration fixes were needed**

```bash
git add CMakeLists.txt src tests docs/superpowers/specs/2026-07-26-explorer-experience-design.md docs/superpowers/plans/2026-07-26-explorer-experience.md
git commit -m "complete explorer experience integration"
```

Skip this commit if Task 8 made no code changes and all previous commits already included the relevant files.

## Self-Review

- Spec coverage: file views are covered by Tasks 2-3; link-style favorites by Task 5; plus-tab home creation by Task 6; breadcrumb and Ctrl+L behavior by Task 4; app-managed open-with defaults by Tasks 1 and 7; testing and verification by each task plus Task 8.
- Placeholder scan: no unchecked step relies on undefined placeholder text; each implementation step includes concrete code or exact commands.
- Type consistency: the plan consistently uses `FileBrowserWidget::ViewMode`, `openWithDefaults`, `setOpenWithDefaults`, `openWithDefaultsChanged`, `breadcrumbContainer`, and `setOpenWithLauncherForTests`.
