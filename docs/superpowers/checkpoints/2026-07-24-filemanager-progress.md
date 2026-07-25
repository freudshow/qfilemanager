# File Manager Progress Checkpoint

Date: 2026-07-24

## Resume Prompt

Continue implementing the Qt file manager from this checkpoint. Use `docs/superpowers/specs/2026-07-24-filemanager-design.md` and `docs/superpowers/plans/2026-07-24-filemanager.md`. Continue with Task 6: right-side metadata inspector. Tasks 1 through 5 are complete and reviewed. This directory is not a git repository, so do not try to commit unless a git repo is initialized later.

## Current State

- Working directory: `/home/floyd/repo/qtproj/filemanager.qt`.
- Project is intentionally configured for Qt 6.11.
- Local machine currently has Qt 6.8.2, so normal `cmake -S . -B build` fails at `find_package(Qt6 6.11 ...)`.
- For verification so far, agents temporarily lowered the Qt requirement to 6.8 in disposable local build directories, ran tests, then restored `CMakeLists.txt` to Qt 6.11.
- The folder is not a git repository; no commits were made.

## Completed Tasks

### Task 1: Bootstrap Qt project skeleton

Implemented and reviewed:

- `CMakeLists.txt`
- `src/main.cpp`
- `src/MainWindow.h`
- `src/MainWindow.cpp`
- `tests/CMakeLists.txt`
- `tests/test_smoke.cpp`
- `README.md`

Summary:

- CMake project `filemanager`, C++20, Qt 6.11 Core/Gui/Widgets/Test.
- `filemanager_app` executable.
- `MainWindow` title: `Qt File Manager`.
- Smoke test target registered.

### Task 2: JSON settings storage

Implemented and reviewed:

- `src/services/AppPaths.h`
- `src/services/SettingsStore.h`
- `src/services/SettingsStore.cpp`
- `tests/test_settings_store.cpp`

Summary:

- `TabState`, `FavoriteState`, `AppSettings`.
- `SettingsStore::settingsPath()`, `load()`, `save()`.
- JSON stored via `QStandardPaths::AppConfigLocation` and `QSaveFile`.
- Missing settings returns defaults.
- Invalid JSON/schema backs up to `settings.json.bak` and returns defaults.
- Validation hardened for base64 window blobs, splitter sizes, tab/favorite fields, boolean options, and version bounds.

### Task 3: Focused Workspace shell

Implemented and reviewed:

- `src/ui/FavoritesSidebar.h/.cpp`
- `src/ui/TabStrip.h/.cpp`
- `src/ui/MetadataPanel.h/.cpp`
- `tests/test_mainwindow.cpp`

Summary:

- `MainWindow` central `QSplitter` named `mainWorkspaceSplitter`.
- Three panes: `favoritesSidebar`, `tabStrip`, `metadataPanel`.
- Headless GUI tests use `QT_QPA_PLATFORM=offscreen`.

### Task 4: Tabbed filesystem browsing

Implemented and reviewed:

- `src/ui/FileBrowserWidget.h/.cpp`
- `src/services/TabManager.h/.cpp`
- Updated `src/ui/TabStrip.h/.cpp`
- `tests/test_filebrowser.cpp`

Summary:

- `FileBrowserWidget` uses `QFileSystemModel`, `QTableView`, and an address bar.
- Directory double-click navigates in the current tab.
- File double-click opens through `QDesktopServices::openUrl`.
- Address bar Return navigates only to existing directories.
- `TabManager` restores saved tabs and falls back to home.
- `MainWindow` loads `SettingsStore` and restores tabs through `TabManager`.

### Task 5: Favorites sidebar

Implemented and reviewed:

- `src/models/FavoritesModel.h/.cpp`
- Updated `src/ui/FavoritesSidebar.h/.cpp`
- Updated `src/MainWindow.h/.cpp`
- `tests/test_favorites_model.cpp`
- `tests/test_favorites_sidebar.cpp`

Summary:

- `FavoritesModel` is a `QAbstractListModel` backed by `QVector<FavoriteState>`.
- Roles: name, path, available, missing.
- Rejects duplicate normalized paths.
- Rejects empty/whitespace-only names and trims stored names.
- Missing paths remain visible but unavailable.
- `FavoritesSidebar` owns `QListView`, emits activation only for available favorites, and supports context-menu removal.
- `MainWindow` owns `FavoritesModel`, loads favorites from settings, persists favorite changes back to JSON while preserving other settings, and navigates the current tab on favorite activation.

## Next Task

### Task 6: Right-side metadata inspector

Start here next session.

Expected files:

- Create `src/models/FileMetadata.h`
- Create `src/models/FileMetadata.cpp`
- Modify `src/ui/MetadataPanel.h`
- Modify `src/ui/MetadataPanel.cpp`
- Create `tests/test_metadata.cpp`
- Modify `src/ui/FileBrowserWidget.h/.cpp` if adding selection signals.
- Modify `src/MainWindow.cpp` if wiring selected file to metadata panel.
- Modify `CMakeLists.txt` and `tests/CMakeLists.txt`.

Task 6 requirements:

- `FileMetadata` contains name, path, type, size, timestamps, permissions, owner/group where available, extension, symlink target, root capacity/free-space.
- Add extraction from path, such as `FileMetadata::fromPath(QString)`.
- Add formatting helpers for size, permissions, and timestamps.
- Missing/unavailable fields should display as `Unavailable`.
- `MetadataPanel` renders a read-only form/list and exposes test helpers like `setMetadata(FileMetadata)`, `clear()`, and `displayedValue(key)`.
- `FileBrowserWidget` should emit `selectedPathChanged(QString)` when selection changes.
- `MainWindow` should connect active browser selection to `MetadataPanel` updates where practical.
- Tests should cover regular file metadata, folder metadata, symlink metadata where supported, missing file handling, formatting, and panel update.
- GUI tests should use `QT_QPA_PLATFORM=offscreen` in CTest.

## Remaining Tasks After Task 6

- Task 7: File operations, trash handling, drag/drop.
- Task 8: Context menus, properties, search, final persistence restore/save.
- Task 9: Cross-platform build and verification docs.
- Final review and verification.

## Verification Caveat

Do not claim final build success unless Qt 6.11 is installed. Local Qt 6.8.2 can be used only for compatibility smoke checks by temporarily lowering the version in a disposable verification flow and restoring `CMakeLists.txt` afterward.
