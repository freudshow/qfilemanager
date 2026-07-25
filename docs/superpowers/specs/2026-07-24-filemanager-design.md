# Qt File Manager Design

Date: 2026-07-24

## Goal

Build a Qt 6.11 desktop file manager similar to Windows File Explorer. The app must compile and run on Debian 13 and Windows 11. It should restore previously opened tabs, save favorite paths in a left sidebar, and show detailed file information in a right-side panel.

## Decisions

- UI framework: Qt Widgets.
- Build system: CMake.
- Main layout: Focused Workspace.
- Persistence: readable JSON config in the per-user application config directory.
- File information panel: metadata only, no previews in the first version.
- Delete behavior: send files to OS trash/recycle bin when available; do not silently permanently delete after trash failure.
- Implementation approach: native Qt Widgets MVC using Qt file models, with targeted background workers for slow operations.

## Architecture

The application will use a `QMainWindow` shell with a three-area Focused Workspace layout:

- Left sidebar for favorites and common locations.
- Center tabbed file browser with address bar, search field, and file table.
- Right metadata panel for selected file or folder information.
- Status area for current path, selection count, operation progress, and errors.

Core modules:

- `MainWindow`: owns menus, toolbars, splitters, status bar, and high-level navigation wiring.
- `TabManager`: creates, closes, switches, and restores file browser tabs.
- `FileBrowserWidget`: wraps `QFileSystemModel` and `QTableView` for directory browsing.
- `FavoritesModel`: manages saved favorite paths and feeds the left sidebar.
- `MetadataPanel`: displays selected file and folder metadata.
- `FileOperationService`: handles copy, move, rename, new folder, delete-to-trash, open, properties, drag/drop, search, and refresh.
- `SettingsStore`: reads and writes JSON settings using atomic saves.
- `PlatformServices`: isolates Windows and Debian behavior for trash/recycle bin, opening files, showing properties, and platform-specific metadata.

The design keeps UI, persistence, platform integration, and file operations separate so each area can be tested and changed independently.

## User Interface Behavior

Startup loads JSON settings, restores the window layout, favorites, and previously opened tabs. If there are no saved tabs, the app opens the user's home directory.

Browsing behavior:

- Each tab tracks its current path, address bar state, selection, sorting, and view options.
- Double-clicking a folder opens it in the current tab.
- Double-clicking a file opens it with the OS default application.
- Middle-click or context menu opens folders in a new tab.
- The sidebar shows favorites and common locations; users can add and remove favorite paths.
- Search is scoped to the current tab path and may run in the background.

The metadata panel updates when the selection changes. It shows metadata only:

- Name.
- Full path.
- Type.
- Size.
- Created, modified, and accessed times when available.
- Permissions.
- Owner and group where available.
- Extension.
- Symlink or shortcut target where available.
- Drive capacity and free-space information for root locations.

## File Operations

The first version includes these operations:

- Copy.
- Move.
- Rename.
- Delete to OS trash/recycle bin.
- New folder.
- Open with OS default application.
- Properties.
- Refresh.
- Drag and drop.
- Context menus.
- Search.

Long-running operations should run asynchronously where practical and report progress in the status area or a progress dialog. Operations should refresh affected views after completion. Cancellation should be supported where the Qt or platform operation allows it.

Delete sends items to the OS trash/recycle bin when available. If trash fails, the app shows an error and leaves the file in place. Permanent delete is not part of the first version.

## Persistence

Settings are stored in a readable JSON file in the per-user application config directory resolved by Qt standard paths.

Typical paths:

- Debian 13: `~/.config/<AppName>/settings.json`.
- Windows 11: `%APPDATA%\\<AppName>\\settings.json`.

Config shape:

```json
{
  "version": 1,
  "window": {
    "geometry": "...",
    "state": "...",
    "splitters": [220, 760, 320]
  },
  "tabs": [
    {
      "path": "/home/user/Documents",
      "sortColumn": "name",
      "sortOrder": "ascending"
    }
  ],
  "favorites": [
    {
      "name": "Projects",
      "path": "/home/user/projects"
    }
  ],
  "options": {
    "showHiddenFiles": false,
    "confirmDeleteToTrash": true
  }
}
```

Data flow:

- Startup: `SettingsStore` reads JSON, validates structure, restores layout, tabs, and favorites.
- Runtime: UI changes notify `TabManager` and `FavoritesModel`.
- Save: settings are written on close and after important user changes.
- Atomicity: save to a temporary file and replace the config file after successful write.
- Missing paths: saved tabs and favorites that no longer exist remain in config and appear unavailable in the UI.

Invalid config handling:

- If JSON parsing or schema validation fails, rename the bad file to a backup path and start with defaults.
- The app should show a clear warning that settings were reset.

## Platform Support

Supported target platforms are Debian 13 and Windows 11 with Qt 6.11.

Platform-specific behavior is contained in `PlatformServices`:

- Windows: use native shell integration for Recycle Bin behavior, opening files, and file properties where possible.
- Debian: use FreeDesktop-compatible trash behavior where available, and Qt desktop services for opening files.
- Metadata differences are normalized before display so unavailable fields are shown as unavailable instead of failing.

The build should use CMake targets and link only the Qt modules required by the app and tests, such as Widgets, Core, Gui, and Test.

## Error Handling

- Failed file operations show a clear dialog and a short status message.
- Partial failures retain details for the user instead of collapsing errors into a generic message.
- Inaccessible paths are shown as unavailable and preserved in user settings.
- Trash/recycle failure does not fall back to permanent delete.
- Long operations keep the UI responsive and expose progress where practical.

## Testing

Use Qt Test for unit and service tests.

Test coverage should include:

- `SettingsStore` JSON load, save, invalid config backup, and migration-ready version handling.
- `FavoritesModel` add, remove, rename, duplicate handling, and missing path display.
- Metadata formatting for files, folders, links, missing files, and roots.
- File operations for copy, move, rename, and new folder using temporary directories.
- Startup restoration for tabs, favorites, and window state.
- Platform services through fakes for unit tests, plus manual or platform tests for real trash/recycle behavior.

Build verification should include documented CMake configure and build commands for Debian 13 and Windows 11.

## Initial Source Layout

```text
CMakeLists.txt
src/
  main.cpp
  MainWindow.h
  MainWindow.cpp
  ui/
    FileBrowserWidget.h
    FileBrowserWidget.cpp
    MetadataPanel.h
    MetadataPanel.cpp
  models/
    FavoritesModel.h
    FavoritesModel.cpp
  services/
    FileOperationService.h
    FileOperationService.cpp
    PlatformServices.h
    PlatformServices.cpp
    SettingsStore.h
    SettingsStore.cpp
tests/
  CMakeLists.txt
  test_settings_store.cpp
  test_favorites_model.cpp
  test_metadata.cpp
```

## Out Of Scope For First Version

- Rich file previews for images, PDFs, videos, or text files.
- Permanent delete or Shift+Delete behavior.
- File indexing across the whole system.
- Network share discovery beyond what the OS exposes as normal filesystem paths.
- Custom icon theme or heavily animated UI.
