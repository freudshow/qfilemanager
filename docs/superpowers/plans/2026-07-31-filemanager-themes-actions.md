# File Manager Themes and File Actions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three selectable application themes, context-menu sorting/creation/terminal actions, and clipboard copy/paste with `Ctrl+C`/`Ctrl+V`, while leaving all changes uncommitted on `master`.

**Architecture:** Keep filesystem access in the existing `FileOperationService`, add a small sortable proxy around `QFileSystemModel` for name/type/size/modified/created ordering, and keep `FileBrowserWidget` responsible only for UI actions, clipboard state, and refreshing. Add a `ThemeManager` for named Qt style sheets and a `TerminalService` for platform-specific terminal command selection; persist the selected theme and each tab's sort state through `SettingsStore`.

**Tech Stack:** C++20, Qt 6 Core/Gui/Widgets/Test, CMake, QTest.

---

### Task 1: Add testable theme and settings persistence

**Files:**
- Create: `src/services/ThemeManager.h`, `src/services/ThemeManager.cpp`
- Modify: `src/services/SettingsStore.h`, `src/services/SettingsStore.cpp`
- Test: `tests/test_settings_store.cpp`

- [x] Add `AppSettings::theme` with default `aurora` and round-trip/legacy-load tests.
- [x] Add `ThemeManager::Theme` values `Aurora`, `Graphite`, and `Clearwater`, stable names, display names, and non-empty style sheets.
- [x] Add a failing test that saves `graphite`, loads it, and verifies the default remains `aurora` when the JSON field is absent.
- [x] Run `cmake --build build --target filemanager_settings_store_test && ctest --test-dir build -R SettingsStoreTest --output-on-failure`; confirm the new test fails before implementation.
- [x] Implement JSON read/write and the theme catalog; invalid/unknown names fall back to `aurora` at application use time.
- [x] Re-run the focused test and then the existing settings test suite.

### Task 2: Add sortable proxy and per-tab sort state

**Files:**
- Create: `src/models/FileSystemSortProxyModel.h`, `src/models/FileSystemSortProxyModel.cpp`
- Modify: `src/ui/FileBrowserWidget.h`, `src/ui/FileBrowserWidget.cpp`, `src/services/TabManager.cpp`, `CMakeLists.txt`
- Test: `tests/test_filebrowser.cpp`, `tests/test_mainwindow.cpp`

- [x] Add failing tests for sorting by name, size, modified time, and creation time, plus ascending/descending state and tab-state persistence.
- [x] Replace direct view usage with a `QFileSystemModel` source plus `FileSystemSortProxyModel`; map root indexes and selected indexes correctly.
- [x] Compare creation time with `QFileInfo::birthTime()` and fall back to metadata-change time or last-modified time when the platform does not expose birth time.
- [x] Add `sortColumnKey()`, `sortOrderKey()`, and `setSort(...)` to `FileBrowserWidget`; restore those values in `TabManager::restoreTabs()` and serialize them in `tabStates()`.
- [x] Re-run focused file-browser and main-window tests after each green cycle.

### Task 3: Add file creation and clipboard operations

**Files:**
- Modify: `src/services/FileOperationService.h`, `src/services/FileOperationService.cpp`, `src/ui/FileBrowserWidget.h`, `src/ui/FileBrowserWidget.cpp`, `CMakeLists.txt`
- Test: `tests/test_file_operations.cpp`, `tests/test_filebrowser.cpp`

- [x] Add failing service tests for creating an empty text file and copying to an automatically renamed conflict target.
- [x] Add `createTextFile()` and an `AutoRename` copy conflict policy without changing the existing strict-copy default.
- [x] Add `Ctrl+C`/`Ctrl+V` shortcuts with widget-with-children context, local-file URL clipboard data, right-click `Copy`/`Paste`, and refresh/error handling.
- [x] Add a `New` submenu containing `New Folder` and `New Text File`; use the existing operation service and input dialogs, rejecting invalid names and existing targets.
- [x] Add tests that inspect menu structure and verify shortcut-driven copying between temporary directories.

### Task 4: Add cross-platform terminal launch

**Files:**
- Create: `src/services/TerminalService.h`, `src/services/TerminalService.cpp`
- Modify: `src/ui/FileBrowserWidget.h`, `src/ui/FileBrowserWidget.cpp`, `CMakeLists.txt`
- Test: `tests/test_filebrowser.cpp`

- [x] Add a failing test using an injected launcher to verify the requested directory is passed as the working directory.
- [x] Implement Linux selection from `$TERMINAL`, then common terminal fallbacks; implement Windows `wt.exe` first and `powershell.exe` fallback.
- [x] Add `Open in Terminal` to the context menu, using a selected directory or the current directory for files/background clicks.
- [x] Surface launch failure through the existing `errorOccurred` signal and refresh no filesystem state on failure.

### Task 5: Add toolbar Themes menu and verification

**Files:**
- Modify: `src/MainWindow.h`, `src/MainWindow.cpp`, `src/services/TabManager.cpp`, `tests/test_mainwindow.cpp`
- Test: `tests/test_mainwindow.cpp`

- [x] Add a failing test for the right-aligned `Themes` toolbar control, nested `Skins` submenu, three checkable theme actions, and `Aurora Garden` default.
- [x] Add an expanding toolbar spacer and a `Themes` tool button whose `Skins` submenu applies the selected theme immediately and persists it through `SettingsStore`.
- [x] Apply the application-level style sheet after loading settings, removing conflicting hard-coded local style sheets from existing widgets so all three themes cover the full UI.
- [x] Run the complete build and `ctest --test-dir build --output-on-failure` with `QT_QPA_PLATFORM=offscreen` test properties already configured.
- [x] Run `git diff --check` and `git status --short`; do not commit or push.

### Verification checklist

- [x] `Aurora Garden` is the default and `Graphite Ember`/`Clearwater` can be selected from `Themes > Skins`.
- [x] Sorting supports name, type, size, modified time, and creation time in both directions.
- [x] New folder and empty text file creation refreshes the current directory without overwriting.
- [x] `Open in Terminal` selects the correct platform command and uses the target directory.
- [x] Copy/paste works from the context menu and `Ctrl+C`/`Ctrl+V`, including directories and same-name conflict renaming.
- [x] No local Git commit is created.
