# Directory Refresh and Windows App Branding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let users refresh an open folder manually or automatically after filesystem changes, and make the Windows build launch as an identifiable GUI application without a console window.

**Architecture:** Keep refresh controls and directory observation within each `FileBrowserWidget`, where the active path and `QFileSystemModel` already live. Add a lightweight `QFileSystemWatcher` plus a short single-shot debounce timer to reapply the model root after external directory changes without recording navigation history. Bundle a raster application icon for Qt windows and a multi-resolution `.ico` in the Windows executable resource; use CMake's Windows GUI-subsystem property only on Windows.

**Tech Stack:** C++20, Qt 6.11 Core/Gui/Widgets/Test (`QFileSystemWatcher`, `QTimer`, `QIcon`), CMake 3.24+, Qt Test, Windows resource compiler.

---

## File Structure

- Create: `resources/filemanager.png` - 256x256 raster icon used by Qt at runtime on every supported desktop.
- Create: `resources/filemanager.ico` - Windows executable icon containing 16x16, 32x32, 48x48, and 256x256 variants of the same artwork.
- Create: `resources/filemanager.rc` - Windows resource-script entry that embeds `filemanager.ico` in `filemanager_app.exe`.
- Modify: `CMakeLists.txt` - compile the PNG into the Qt resource system; conditionally embed the ICO and select the Windows GUI subsystem.
- Modify: `src/main.cpp` - set the process-wide Qt application icon before constructing the first top-level window.
- Modify: `src/MainWindow.cpp` - set the same icon on the main window so all platforms and tests have an explicit window icon.
- Modify: `src/ui/FileBrowserWidget.h` - expose a testable, non-navigating refresh operation; own refresh UI and filesystem observation state.
- Modify: `src/ui/FileBrowserWidget.cpp` - add the adjacent Refresh button, watch the active directory, debounce change notifications, and refresh all active views without mutating history.
- Modify: `tests/test_filebrowser.cpp` - cover manual refresh, automatic refresh after an external file change, and history preservation.
- Modify: `tests/test_mainwindow.cpp` - verify `MainWindow` uses the compiled application icon.
- Modify: `docs/build-windows-11.md` - document the GUI-subsystem/icon behavior and Windows-specific build/manual verification.

## Task 1: Add the Failing Directory Refresh Tests

**Files:**
- Modify: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Declare the new test cases**

In `FileBrowserWidgetTest` in `tests/test_filebrowser.cpp`, add these declarations after `upButtonNavigatesToParentDirectory()`:

```cpp
    void refreshButtonReloadsCurrentDirectoryWithoutNavigation();
    void changesInCurrentDirectoryTriggerRefresh();
```

- [ ] **Step 2: Write the manual-refresh test**

Add this test before `addressBarUsesModernNavigationChrome()`:

```cpp
void FileBrowserWidgetTest::refreshButtonReloadsCurrentDirectoryWithoutNavigation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(directory.path()));
    QSignalSpy refreshedSpy(&browser, &FileBrowserWidget::directoryRefreshed);
    auto *refreshButton = browser.findChild<QToolButton *>("refreshButton");
    QVERIFY(refreshButton != nullptr);

    QTest::mouseClick(refreshButton, Qt::LeftButton);

    QCOMPARE(refreshedSpy.count(), 1);
    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(!browser.canGoBack());
    QVERIFY(!browser.canGoForward());
}
```

- [ ] **Step 3: Write the automatic-refresh test**

Add this test immediately after the manual-refresh test:

```cpp
void FileBrowserWidgetTest::changesInCurrentDirectoryTriggerRefresh() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(directory.path()));
    QSignalSpy refreshedSpy(&browser, &FileBrowserWidget::directoryRefreshed);
    const QString createdPath = QDir(directory.path()).filePath("created-by-another-process.txt");
    QFile createdFile(createdPath);
    QVERIFY(createdFile.open(QIODevice::WriteOnly));
    createdFile.write("new file");
    createdFile.close();

    QTRY_VERIFY(refreshedSpy.count() >= 1);
    QCOMPARE(browser.currentPath(), directory.path());
    QVERIFY(!browser.canGoBack());
    QVERIFY(!browser.canGoForward());
}
```

The test deliberately creates the file after the refresh signal spy is connected. It therefore proves a filesystem notification, not the initial `setCurrentPath()` call, caused the refresh.

- [ ] **Step 4: Run the focused test target and confirm it fails**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: compilation fails because `FileBrowserWidget::directoryRefreshed` and the `refreshButton` object do not exist.

## Task 2: Implement Per-Tab Manual and Automatic Refresh

**Files:**
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Test: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Add the refresh API, notification, and owned Qt objects**

In `src/ui/FileBrowserWidget.h`, add forward declarations alongside the existing Qt declarations:

```cpp
class QFileSystemWatcher;
class QTimer;
```

Add this public method after `goForward()`:

```cpp
    bool refreshCurrentDirectory();
```

Add this signal after `historyChanged(...)`:

```cpp
    void directoryRefreshed();
```

Add these private helper declarations before `recordHistoryPath(...)`:

```cpp
    void updateViewRoots(const QModelIndex &rootIndex);
    void watchCurrentDirectory();
```

Add the following members after `upButton_`:

```cpp
    QToolButton *refreshButton_ = nullptr;
    QFileSystemWatcher *directoryWatcher_ = nullptr;
    QTimer *refreshTimer_ = nullptr;
```

`refreshCurrentDirectory()` is public so the button and any future toolbar/menu action can invoke the same behavior. It must not call `setCurrentPath()` because a refresh is not a user navigation.

- [ ] **Step 2: Initialize the button, watcher, and debounce timer**

In the initializer list in `src/ui/FileBrowserWidget.cpp`, add these objects after `upButton_`:

```cpp
    , refreshButton_(new QToolButton(this))
    , directoryWatcher_(new QFileSystemWatcher(this))
    , refreshTimer_(new QTimer(this))
```

Add these includes with the existing Qt includes:

```cpp
#include <QFileSystemWatcher>
#include <QTimer>
```

Immediately after the existing `upButton_` setup, configure the new control and timer:

```cpp
    refreshButton_->setObjectName("refreshButton");
    refreshButton_->setText(tr("Refresh"));
    refreshButton_->setToolTip(tr("Refresh this folder"));
    refreshTimer_->setSingleShot(true);
    refreshTimer_->setInterval(100);
```

Put the Refresh button directly beside the current Up button by changing the address layout setup to:

```cpp
    addressLayout->addWidget(upButton_);
    addressLayout->addWidget(refreshButton_);
```

Extend the existing stylesheet so Up and Refresh have identical visual treatment:

```cpp
        "QToolButton#upButton, QToolButton#refreshButton { background: #ffffff; border: 1px solid #c8d2df; border-radius: 8px; padding: 5px 10px; }"
        "QToolButton#upButton:hover, QToolButton#refreshButton:hover { background: #eaf1f8; }"));
```

Replace the two original button selector strings with the two combined selectors above; retain the address-container and line-edit style strings unchanged.

- [ ] **Step 3: Connect manual and automatic refresh triggers**

After the existing `upButton_` connection, add the following signal wiring:

```cpp
    connect(refreshButton_, &QToolButton::clicked, this, [this] {
        refreshCurrentDirectory();
    });
    connect(directoryWatcher_, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        refreshTimer_->start();
    });
    connect(refreshTimer_, &QTimer::timeout, this, [this] {
        refreshCurrentDirectory();
    });
```

The 100 ms single-shot delay coalesces common write patterns such as create-then-rename and editor save operations. It also prevents repeated root resets when the operating system reports several notifications for one operation.

- [ ] **Step 4: Centralize setting the root index for all three views**

Add this helper immediately before `FileBrowserWidget::applyPath(...)`:

```cpp
void FileBrowserWidget::updateViewRoots(const QModelIndex &rootIndex) {
    detailsView_->setRootIndex(rootIndex);
    listView_->setRootIndex(rootIndex);
    tilesView_->setRootIndex(rootIndex);
}
```

In `applyPath(...)`, replace:

```cpp
    detailsView_->setRootIndex(rootIndex);
    listView_->setRootIndex(rootIndex);
    tilesView_->setRootIndex(rootIndex);
```

with:

```cpp
    updateViewRoots(rootIndex);
```

After `currentPath_ = absolutePath;`, add:

```cpp
    watchCurrentDirectory();
```

This establishes exactly one watcher for the selected tab's current directory whenever normal navigation, a breadcrumb click, Up, Back, Forward, or a restored tab changes its path.

- [ ] **Step 5: Implement watcher ownership and the non-navigating refresh operation**

Add these definitions after `applyPath(...)`:

```cpp
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
```

This deliberately leaves `currentPath_`, `history_`, `historyIndex_`, breadcrumbs, and the visible view mode unchanged. If the current directory itself has been removed, the method returns `false`, stops watching that path, and does not silently navigate the user elsewhere.

- [ ] **Step 6: Run the focused tests and verify the behavior**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: `FileBrowserWidgetTest` passes, including both new refresh tests and the existing navigation/breadcrumb tests.

- [ ] **Step 7: Commit the refresh feature**

```bash
git add src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp tests/test_filebrowser.cpp
git commit -m "feat: refresh active folders automatically"
```

## Task 3: Add a Bundled Application Icon and Windows GUI Executable Settings

**Files:**
- Create: `resources/filemanager.png`
- Create: `resources/filemanager.ico`
- Create: `resources/filemanager.rc`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`
- Modify: `src/MainWindow.cpp`
- Modify: `tests/test_mainwindow.cpp`

- [ ] **Step 1: Create the matching runtime and executable icon assets**

Create `resources/` and add an original 256x256 `resources/filemanager.png` plus a multi-resolution `resources/filemanager.ico`. Use the same simple, legible artwork in both: a blue folder with a white file/page inset, no text, and sufficient contrast at 16x16. The ICO must contain 16x16, 32x32, 48x48, and 256x256 layers so Windows Explorer, the taskbar, Alt+Tab, and high-DPI displays choose a suitable rendition.

Inspect both files before continuing:

```bash
file resources/filemanager.png resources/filemanager.ico
```

Expected: the PNG is 256x256 and the ICO is recognized as a Windows icon containing multiple image sizes.

- [ ] **Step 2: Add the Windows resource script**

Create `resources/filemanager.rc` with exactly:

```rc
IDI_FILEMANAGER ICON "filemanager.ico"
```

The relative path is resolved by the Windows resource compiler from the `.rc` file's directory, keeping the executable-specific icon independent from Qt's runtime resource bundle.

- [ ] **Step 3: Write the failing main-window icon test**

Add this declaration to `MainWindowTest` in `tests/test_mainwindow.cpp` after `focusedWorkspaceShellHasThreeNamedPanes()`:

```cpp
    void mainWindowUsesBundledApplicationIcon();
```

Add this test after `focusedWorkspaceShellHasThreeNamedPanes()`:

```cpp
void MainWindowTest::mainWindowUsesBundledApplicationIcon() {
    MainWindow window;

    QVERIFY(!window.windowIcon().isNull());
}
```

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R MainWindowTest
```

Expected: the new test fails because no explicit window icon is configured.

- [ ] **Step 4: Compile the PNG as a Qt resource and configure the Windows executable**

In `CMakeLists.txt`, add the Qt resource call immediately after `target_link_libraries(filemanager_ui ...)`:

```cmake
qt_add_resources(filemanager_ui "filemanager_icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/filemanager.png
)
```

After the `target_link_libraries(filemanager_app PRIVATE filemanager_ui)` line, add this Windows-only block:

```cmake
if(WIN32)
    target_sources(filemanager_app PRIVATE resources/filemanager.rc)
    set_target_properties(filemanager_app PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
```

`WIN32_EXECUTABLE` selects the Windows GUI subsystem only for Windows builds. It prevents the unwanted console window when the program is launched normally, while Linux builds retain their existing command-line launch behavior.

- [ ] **Step 5: Set the Qt application and main-window icon**

In `src/main.cpp`, add the include:

```cpp
#include <QIcon>
```

Immediately after `QApplication app(argc, argv);`, add:

```cpp
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/filemanager.png")));
```

In `src/MainWindow.cpp`, add this include with the other Qt includes:

```cpp
#include <QIcon>
```

Immediately after the current `setWindowTitle("Qt File Manager");` call in the `MainWindow` constructor, add:

```cpp
    setWindowIcon(QIcon(QStringLiteral(":/icons/filemanager.png")));
```

The process-wide call covers dialogs and future top-level windows; the explicit `MainWindow` call ensures the primary window exposes the icon when constructed in isolated Qt tests that do not run `main()`.

- [ ] **Step 6: Run the automated regression suite**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: all registered tests pass, including `MainWindowTest::mainWindowUsesBundledApplicationIcon` and the new file-browser refresh coverage.

- [ ] **Step 7: Commit the branding and Windows launch behavior**

```bash
git add CMakeLists.txt resources/filemanager.png resources/filemanager.ico resources/filemanager.rc src/main.cpp src/MainWindow.cpp tests/test_mainwindow.cpp
git commit -m "feat: brand Windows application launch"
```

## Task 4: Document and Verify the Windows-Specific Result

**Files:**
- Modify: `docs/build-windows-11.md`

- [ ] **Step 1: Document the Windows executable behavior**

Append this section to `docs/build-windows-11.md`:

```markdown
## GUI Executable and Icon

On Windows, `filemanager_app.exe` is built as a GUI-subsystem executable. Launching it opens only the file-manager window; it does not open a separate command-prompt window. The executable embeds the Qt File Manager icon for Explorer, the taskbar, Alt+Tab, and pinned shortcuts.

## Windows Manual Verification

1. Configure and build from a Qt-enabled Developer PowerShell:

   ```powershell
   cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64
   cmake --build build-win --config Release
   ctest --test-dir build-win -C Release --output-on-failure
   ```

2. Confirm the executable uses the Windows GUI subsystem:

   ```powershell
   dumpbin /headers build-win\Release\filemanager_app.exe | Select-String "subsystem"
   ```

   The output must identify the Windows GUI subsystem, not the Windows CUI subsystem.

3. Start `build-win\Release\filemanager_app.exe` from Explorer. Confirm no Command Prompt window appears, and confirm the custom folder icon is visible on the taskbar and in Alt+Tab. If an older pinned shortcut shows a cached generic icon, unpin it, close the application, reopen the executable, and pin it again.

4. Open a directory in the file manager, create or delete a file with a separate program, and confirm the active tab updates automatically. Click `Refresh` beside the Up button and confirm the same directory reloads without changing Back/Forward history.
```

- [ ] **Step 2: Review source and platform behavior before committing**

Run on the development platform:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: configuration, build, and every Qt Test target pass. The Windows GUI-subsystem and embedded-ICO checks are intentionally performed on Windows using the documented PowerShell commands.

- [ ] **Step 3: Commit the verification instructions**

```bash
git add docs/build-windows-11.md
git commit -m "docs: verify Windows GUI application behavior"
```

## Final Acceptance Checklist

- [ ] The address row places a `Refresh` button immediately after the existing Up button and identifies it with the `refreshButton` object name.
- [ ] Clicking Refresh reuses the active directory, refreshes all three view roots, emits `directoryRefreshed`, and does not alter back/forward history.
- [ ] Each `FileBrowserWidget` watches only its active directory; an external create, rename, delete, or save triggers a debounced refresh in that tab without affecting other tabs.
- [ ] The app displays a bundled folder icon in Qt windows on Debian and Windows, and Windows embeds the matching multi-resolution ICO for shell surfaces.
- [ ] Windows builds use the GUI subsystem, so ordinary launches do not create a console window; non-Windows builds remain unchanged.
- [ ] `ctest --test-dir build --output-on-failure` passes locally, and the documented Windows build/manual checks pass on Windows 11.
