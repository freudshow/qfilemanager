# Qt File Manager Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Qt 6.11 desktop file manager for Debian 13 and Windows 11 with tab restoration, saved favorites, a metadata inspector, and Explorer-like file operations.

**Architecture:** Use a Qt Widgets `QMainWindow` shell with a left favorites sidebar, center tabbed file browser, and right metadata inspector. Keep persistence, platform behavior, and file operations in separate services so the UI stays thin and the behavior is testable.

**Tech Stack:** CMake, Qt 6.11 Widgets/Core/Gui/Test, `QFileSystemModel`, `QTreeView`, `QTableView`, `QSettings`-style path handling via `QStandardPaths`, JSON via Qt `QJsonDocument`, async work via Qt signals/slots and worker objects.

---

### Task 1: Bootstrap the Qt project skeleton

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `src/MainWindow.h`
- Create: `src/MainWindow.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `README.md`

- [ ] **Step 1: Write the failing build expectation**

Create a minimal smoke test target in `tests/CMakeLists.txt` that will not build until the app target exists:

```cmake
add_executable(filemanager_smoke_test test_smoke.cpp)
target_link_libraries(filemanager_smoke_test PRIVATE Qt6::Test filemanager_app)
add_test(NAME filemanager_smoke_test COMMAND filemanager_smoke_test)
```

- [ ] **Step 2: Run configure/build to verify it fails for the right reason**

Run:

```bash
cmake -S . -B build
cmake --build build
```

Expected: configure or build fails because `filemanager_app` and the source files do not exist yet.

- [ ] **Step 3: Add the minimal app entry point**

```cpp
// src/main.cpp
#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
```

```cpp
// src/MainWindow.h
#pragma once

#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
};
```

```cpp
// src/MainWindow.cpp
#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Qt File Manager");
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.24)
project(filemanager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets Test)

qt_standard_project_setup()

add_executable(filemanager_app
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
)
target_link_libraries(filemanager_app PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 4: Run configure/build again and verify the app target exists**

Run:

```bash
cmake -S . -B build
cmake --build build
```

Expected: `filemanager_app` builds successfully.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src tests README.md
git commit -m "feat: bootstrap qt file manager project"
```

### Task 2: Implement JSON settings storage and restore

**Files:**
- Create: `src/services/SettingsStore.h`
- Create: `src/services/SettingsStore.cpp`
- Create: `src/services/AppPaths.h`
- Create: `tests/test_settings_store.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing settings test**

```cpp
// tests/test_settings_store.cpp
#include <QtTest/QtTest>
#include "services/SettingsStore.h"

class SettingsStoreTest : public QObject {
    Q_OBJECT
private slots:
    void roundTripPreservesTabsAndFavorites();
    void invalidJsonCreatesBackupAndFallsBack();
};

QTEST_MAIN(SettingsStoreTest)
#include "test_settings_store.moc"
```

- [ ] **Step 2: Run the test and confirm it fails because the class is missing**

Run:

```bash
cmake --build build --target filemanager_smoke_test
ctest --test-dir build -R SettingsStoreTest -V
```

Expected: compile failure or unresolved include because `SettingsStore` does not exist yet.

- [ ] **Step 3: Implement the settings API and JSON model**

```cpp
// src/services/SettingsStore.h
#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>

struct TabState { QString path; QString sortColumn; QString sortOrder; };
struct FavoriteState { QString name; QString path; };

struct AppSettings {
    int version = 1;
    QByteArray windowGeometry;
    QByteArray windowState;
    QVector<int> splitterSizes;
    QVector<TabState> tabs;
    QVector<FavoriteState> favorites;
    bool showHiddenFiles = false;
    bool confirmDeleteToTrash = true;
};

class SettingsStore {
public:
    QString settingsPath() const;
    bool load(AppSettings &settings, QString *errorMessage = nullptr);
    bool save(const AppSettings &settings, QString *errorMessage = nullptr);
};
```

Implement `SettingsStore.cpp` using `QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)`, `QSaveFile`, `QJsonDocument`, and backup logic that renames bad JSON to `settings.json.bak`.

- [ ] **Step 4: Run the settings test until it passes**

Run:

```bash
ctest --test-dir build -R SettingsStoreTest -V
```

Expected: both tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/services tests CMakeLists.txt
git commit -m "feat: add json settings persistence"
```

### Task 3: Build the Focused Workspace main window shell

**Files:**
- Modify: `src/MainWindow.h`
- Modify: `src/MainWindow.cpp`
- Create: `src/ui/FavoritesSidebar.h`
- Create: `src/ui/FavoritesSidebar.cpp`
- Create: `src/ui/MetadataPanel.h`
- Create: `src/ui/MetadataPanel.cpp`
- Create: `src/ui/TabStrip.h`
- Create: `src/ui/TabStrip.cpp`

- [ ] **Step 1: Write the failing UI construction test**

```cpp
// tests/test_mainwindow.cpp
#include <QtTest/QtTest>
#include "MainWindow.h"

class MainWindowTest : public QObject {
    Q_OBJECT
private slots:
    void createsThreePaneLayout();
};
```

- [ ] **Step 2: Run the test and confirm the layout classes are missing**

Run:

```bash
ctest --test-dir build -R MainWindowTest -V
```

Expected: compile failure until the layout widgets exist.

- [ ] **Step 3: Implement the widget shell**

Create a `QSplitter`-based layout in `MainWindow` with:

```cpp
// src/MainWindow.cpp
auto *root = new QSplitter(this);
auto *left = new FavoritesSidebar(root);
auto *center = new TabStrip(root);
auto *right = new MetadataPanel(root);
root->addWidget(left);
root->addWidget(center);
root->addWidget(right);
setCentralWidget(root);
```

The center panel should reserve room for tabs, an address bar, and a file browser widget in later tasks.

- [ ] **Step 4: Run the UI test and verify the three-pane shell exists**

Run:

```bash
ctest --test-dir build -R MainWindowTest -V
```

Expected: the test passes and the window title, splitter count, and pane presence match the design.

- [ ] **Step 5: Commit**

```bash
git add src/MainWindow.* src/ui tests
git commit -m "feat: add file manager window shell"
```

### Task 4: Implement tabbed browsing with filesystem navigation

**Files:**
- Create: `src/ui/FileBrowserWidget.h`
- Create: `src/ui/FileBrowserWidget.cpp`
- Create: `src/services/TabManager.h`
- Create: `src/services/TabManager.cpp`
- Modify: `src/ui/TabStrip.h`
- Modify: `src/ui/TabStrip.cpp`
- Create: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Write the failing navigation test**

```cpp
// tests/test_filebrowser.cpp
#include <QtTest/QtTest>
#include "ui/FileBrowserWidget.h"

class FileBrowserWidgetTest : public QObject {
    Q_OBJECT
private slots:
    void opensHomeDirectoryWhenNoTabsAreSaved();
    void changesPathOnDoubleClickingFolder();
};
```

- [ ] **Step 2: Run the test and confirm the widget is missing**

Run:

```bash
ctest --test-dir build -R FileBrowserWidgetTest -V
```

Expected: compile failure because `FileBrowserWidget` is not implemented yet.

- [ ] **Step 3: Implement browsing using `QFileSystemModel`**

`FileBrowserWidget` should own:

```cpp
QFileSystemModel *model;
QTreeView *treeView;
QLineEdit *addressBar;
```

Behavior:

- Set the root path on the model.
- Bind the view to the model.
- Update the address bar when navigation changes.
- Emit a `pathChanged(QString)` signal.
- Use open-on-double-click for folders and OS default app for files.

`TabManager` should create one `FileBrowserWidget` per tab and restore saved tabs from `AppSettings`.

- [ ] **Step 4: Run the navigation test and verify tab restoration works**

Run:

```bash
ctest --test-dir build -R FileBrowserWidgetTest -V
```

Expected: home directory opens when empty state is loaded, and folder navigation updates the current path.

- [ ] **Step 5: Commit**

```bash
git add src/ui src/services tests
git commit -m "feat: add tabbed filesystem browsing"
```

### Task 5: Add favorites management in the left sidebar

**Files:**
- Create: `src/models/FavoritesModel.h`
- Create: `src/models/FavoritesModel.cpp`
- Create: `src/ui/FavoritesSidebar.h`
- Create: `src/ui/FavoritesSidebar.cpp`
- Create: `tests/test_favorites_model.cpp`

- [ ] **Step 1: Write the failing favorites model test**

```cpp
// tests/test_favorites_model.cpp
#include <QtTest/QtTest>
#include "models/FavoritesModel.h"

class FavoritesModelTest : public QObject {
    Q_OBJECT
private slots:
    void addsAndRemovesFavorites();
    void preservesMissingFavoritePaths();
};
```

- [ ] **Step 2: Run the test and confirm the model is missing**

Run:

```bash
ctest --test-dir build -R FavoritesModelTest -V
```

Expected: compile failure because the model does not exist yet.

- [ ] **Step 3: Implement a thin `QAbstractListModel` for favorites**

Expose name and path roles, plus add/remove/reorder methods.

```cpp
bool addFavorite(const QString &name, const QString &path);
bool removeFavorite(int row);
FavoriteState favoriteAt(int row) const;
```

`FavoritesSidebar` should use a `QListView` bound to the model, support context-menu remove, and emit a signal when a favorite is activated.

- [ ] **Step 4: Run the favorites test and verify sidebar activation works**

Run:

```bash
ctest --test-dir build -R FavoritesModelTest -V
```

Expected: adding, removing, duplicate-path rejection, and missing-path preservation pass.

- [ ] **Step 5: Commit**

```bash
git add src/models src/ui tests
git commit -m "feat: add favorite path sidebar"
```

### Task 6: Implement the right-side metadata inspector

**Files:**
- Create: `src/models/FileMetadata.h`
- Create: `src/models/FileMetadata.cpp`
- Modify: `src/ui/MetadataPanel.h`
- Modify: `src/ui/MetadataPanel.cpp`
- Create: `tests/test_metadata.cpp`

- [ ] **Step 1: Write the failing metadata formatting test**

```cpp
// tests/test_metadata.cpp
#include <QtTest/QtTest>
#include "models/FileMetadata.h"

class FileMetadataTest : public QObject {
    Q_OBJECT
private slots:
    void formatsRegularFileMetadata();
    void formatsFolderAndSymlinkMetadata();
};
```

- [ ] **Step 2: Run the test and confirm formatting code is missing**

Run:

```bash
ctest --test-dir build -R FileMetadataTest -V
```

Expected: compile failure until `FileMetadata` and formatting helpers exist.

- [ ] **Step 3: Implement metadata extraction and display**

`FileMetadata` should include fields for name, path, type, size, timestamps, permissions, owner/group, extension, symlink target, and root capacity/free-space. `MetadataPanel` should render those fields in a read-only form and show `Unavailable` when a field cannot be resolved on the current platform.

- [ ] **Step 4: Run the metadata test and verify the panel updates from selection changes**

Run:

```bash
ctest --test-dir build -R FileMetadataTest -V
```

Expected: the metadata object formats real filesystem entries and the panel updates on selection change signals.

- [ ] **Step 5: Commit**

```bash
git add src/models src/ui tests
git commit -m "feat: add file metadata inspector"
```

### Task 7: Implement file operations, trash handling, and drag/drop

**Files:**
- Create: `src/services/FileOperationService.h`
- Create: `src/services/FileOperationService.cpp`
- Create: `src/services/PlatformServices.h`
- Create: `src/services/PlatformServices.cpp`
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Create: `tests/test_file_operations.cpp`

- [ ] **Step 1: Write the failing file operation tests**

```cpp
// tests/test_file_operations.cpp
#include <QtTest/QtTest>
#include "services/FileOperationService.h"

class FileOperationServiceTest : public QObject {
    Q_OBJECT
private slots:
    void copiesFilesBetweenTemporaryDirectories();
    void renamesAndCreatesFolders();
    void deletesToTrashViaPlatformService();
};
```

- [ ] **Step 2: Run the tests and confirm the service does not exist yet**

Run:

```bash
ctest --test-dir build -R FileOperationServiceTest -V
```

Expected: compile failure because `FileOperationService` and `PlatformServices` are missing.

- [ ] **Step 3: Implement the operations service with a platform abstraction**

`FileOperationService` should expose methods like:

```cpp
bool copy(const QStringList &sources, const QString &destination, QString *errorMessage);
bool move(const QStringList &sources, const QString &destination, QString *errorMessage);
bool renamePath(const QString &source, const QString &targetName, QString *errorMessage);
bool createFolder(const QString &parentDir, const QString &name, QString *errorMessage);
bool deleteToTrash(const QStringList &paths, QString *errorMessage);
```

Use `QFile`, `QDir`, and platform services for trash/recycle integration. Keep the UI responsive by emitting progress and using worker objects when needed.

`PlatformServices` should centralize:

- opening a file or folder in the OS default handler,
- sending items to trash/recycle bin,
- platform-specific properties dialogs when available.

- [ ] **Step 4: Run the file operation tests and verify trash failure does not delete permanently**

Run:

```bash
ctest --test-dir build -R FileOperationServiceTest -V
```

Expected: copy, move, rename, new folder, and trash-to-recycle integration tests pass; trash failure returns an error and leaves the source intact.

- [ ] **Step 5: Commit**

```bash
git add src/services src/ui tests
git commit -m "feat: add file operations and trash support"
```

### Task 8: Wire context menus, properties actions, search, and persistence restore

**Files:**
- Modify: `src/MainWindow.cpp`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Modify: `src/services/SettingsStore.cpp`
- Create: `src/services/SearchService.h`
- Create: `src/services/SearchService.cpp`
- Create: `tests/test_restore.cpp`

- [ ] **Step 1: Write the failing restore/search test**

```cpp
// tests/test_restore.cpp
#include <QtTest/QtTest>
#include "services/SettingsStore.h"

class RestoreTest : public QObject {
    Q_OBJECT
private slots:
    void restoresWindowTabsAndFavorites();
    void searchServiceLimitsResultsToCurrentPath();
};
```

- [ ] **Step 2: Run the tests and confirm restore/search services are not implemented yet**

Run:

```bash
ctest --test-dir build -R RestoreTest -V
```

Expected: compile failure until restore and search plumbing exist.

- [ ] **Step 3: Implement tab restore, favorites restore, context menu actions, and scoped search**

`MainWindow` should load the JSON settings on startup, restore splitters, restore tabs, and save them on close. `FileBrowserWidget` should expose a context menu with open, open in new tab, copy, move, rename, delete to trash, new folder, and properties. `SearchService` should perform background search scoped to the active path and emit results incrementally.

- [ ] **Step 4: Run the restore/search test and verify the app persists user state**

Run:

```bash
ctest --test-dir build -R RestoreTest -V
```

Expected: tabs, favorites, and window state round-trip through JSON; search returns results only from the selected scope.

- [ ] **Step 5: Commit**

```bash
git add src tests CMakeLists.txt
git commit -m "feat: restore tabs and add scoped search"
```

### Task 9: Add cross-platform build and verification instructions

**Files:**
- Modify: `README.md`
- Create: `docs/build-debian-13.md`
- Create: `docs/build-windows-11.md`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing documentation check**

Add a small docs verification target or at least a manual checklist in `README.md` that references the exact commands below.

- [ ] **Step 2: Add build commands for Debian 13 and Windows 11**

Document these exact commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For Windows with Visual Studio or Ninja, document the matching generator command and note that Qt 6.11, CMake, and the MSVC toolchain must be installed.

- [ ] **Step 3: Make the docs and test targets consistent with the finished app**

Ensure `tests/CMakeLists.txt` registers every test target added above and that the README lists the supported platforms, build steps, and first-version limitations.

- [ ] **Step 4: Run the full test suite and verify both platforms are documented**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: all Qt Test targets pass.

- [ ] **Step 5: Commit**

```bash
git add README.md docs tests/CMakeLists.txt
git commit -m "docs: add build and verification instructions"
```

## Coverage Check

This plan covers the approved spec requirements as follows:

- Qt Widgets app: Tasks 1 and 3.
- CMake build: Task 1.
- Debian 13 and Windows 11 support: Tasks 1, 7, and 9.
- Tabs restore across launches: Tasks 2, 4, and 8.
- Favorites in left sidebar: Task 5.
- Metadata-only right panel: Task 6.
- Explorer-like operations: Task 7 and Task 8.
- Delete to trash/recycle bin: Task 7.
- JSON config persistence: Task 2 and Task 8.
- Search in the current path: Task 8.
- Background work for slow operations: Tasks 4, 7, and 8.
- Tests and build verification: Tasks 1 through 9.

## Notes For Implementers

- Keep file responsibilities small; if a class starts owning UI, persistence, and filesystem behavior, split it.
- Use `QStandardPaths` for all user-writable paths.
- Do not add preview features in the first pass; metadata only is the agreed scope.
- Do not implement permanent delete unless the spec changes.
