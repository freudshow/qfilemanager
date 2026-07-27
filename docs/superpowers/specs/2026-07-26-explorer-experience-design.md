# Explorer Experience Enhancement Design

Date: 2026-07-26

## Goal

Enhance the Qt Widgets file manager with an integrated Windows Explorer-like browsing experience while preserving the current architecture. The feature set includes switchable file views, link-style favorites, a plus button for new tabs, a Windows 11-style breadcrumb address bar, Ctrl+L path editing, and app-managed default open-with behavior.

## Scope

This design extends the existing `MainWindow`, `TabManager`, `TabStrip`, `FavoritesSidebar`, `FileBrowserWidget`, `SettingsStore`, and platform-service boundaries. It does not replace the app shell or rewrite the file manager around a new UI framework.

In scope:

- File display modes: list, details, and tiles.
- Link-button-style favorites that navigate on single click.
- A `+` tab affordance that opens the user's home directory.
- Breadcrumb address navigation with clickable path segments.
- Ctrl+L edit mode for entering the full path.
- Per-extension default open-with settings managed by this application.
- Tests for navigation, view switching, tab creation, settings, and open-with behavior.

Out of scope:

- Changing system-level file associations.
- Rich file previews.
- Replacing the existing metadata panel or file operation service.
- Heavy visual redesign outside the current integrated Explorer-style layout.

## Chosen Approach

Use progressive enhancement of the existing Qt Widgets components.

The current code already has the right major boundaries: `FileBrowserWidget` owns directory browsing, `TabManager` owns tab lifecycle, `FavoritesSidebar` owns favorites UI, and `SettingsStore` persists state. Enhancing these classes keeps the change focused and avoids a large rewrite.

Rejected alternatives:

- A new `ExplorerPage` wrapper would create cleaner long-term boundaries but would replace too much existing tested code for this feature pass.
- Directly appending controls without small helper components would be faster initially but would make `FileBrowserWidget` harder to test and maintain.

## User Experience

The center browsing area keeps the existing tabbed layout. Each tab shows a compact top navigation row, then the file display area.

The navigation row contains:

- An up button.
- A breadcrumb address control by default.
- A full path editor only while editing.
- View mode controls for list, details, and tiles.

The left favorites area remains in the splitter but presents each favorite like a navigational link button. Single-clicking an available favorite navigates the current tab immediately. Missing favorites remain visible but disabled or visually muted so users can remove them from the context menu.

The tab bar gains a `+` affordance. Clicking it creates a new tab at `QDir::homePath()` and switches focus to it.

## File View Modes

`FileBrowserWidget` will expose a `ViewMode` enum with these values:

- `Details`: table view with columns from `QFileSystemModel`.
- `List`: compact icon/list view.
- `Tiles`: larger icon grid with wrapped names.

The implementation can share one `QFileSystemModel` and switch between view widgets, or use a `QStackedWidget` containing `QTableView` and `QListView` variants. The active view must keep the same root index, selection behavior, double-click behavior, context menu behavior, drag/drop settings, and sorting where applicable.

Each tab tracks its own view mode. The existing tab state persistence should be extended later to include view mode alongside path and sort state.

## Breadcrumb Address Bar

The address bar defaults to breadcrumb mode. A clean path is split into clickable segments. Clicking a segment navigates to the directory represented by that segment.

Behavior rules:

- Root segments are handled explicitly for Linux roots and Windows drive roots.
- Each breadcrumb button stores its absolute target path.
- `setCurrentPath()` updates the breadcrumb display after successful navigation.
- Invalid paths do not change the current directory.
- The up button continues to navigate to the parent directory.

Ctrl+L switches the address area to edit mode. Edit mode shows the current full path in a `QLineEdit`, selects the text, and focuses the input. Pressing Enter tries to navigate. Pressing Escape or losing focus returns to breadcrumb mode without changing paths. After a successful Enter navigation, the control returns to breadcrumb mode.

## Favorites Sidebar

`FavoritesSidebar` will keep using `FavoritesModel` but adjust interaction and styling so rows behave like link buttons.

Behavior rules:

- Single click or keyboard activation emits `favoriteActivated(path)` for available favorites.
- Missing favorites are visible but do not emit activation.
- The existing context menu keeps Add Current Folder and Remove.
- The model remains the source of truth for favorite name, path, and availability.

This avoids a new persistence format while making favorites feel like navigational shortcuts.

## Tabs

`TabStrip` or `TabManager` will add a plus affordance to the tab bar. The preferred implementation is a corner widget or tab-bar button that calls `TabManager::addTab(QDir::homePath())`, then selects the new browser.

Behavior rules:

- The default path for new plus-created tabs is the user's home directory.
- Existing restore behavior remains unchanged.
- The app still prevents closing the final remaining tab.

## Default Open-With

The app will manage default applications by file extension inside its settings instead of changing system-level file associations.

Settings will gain an `openWithDefaults` map shaped like this:

```json
{
  "openWithDefaults": {
    ".txt": "/usr/bin/kate",
    ".png": "C:\\Program Files\\Example\\viewer.exe"
  }
}
```

Behavior rules:

- Double-clicking a file checks the extension map first.
- If a configured application exists, the app launches it with the selected file path.
- If no configured application exists, the app uses the OS default via `QDesktopServices` or `PlatformServices`.
- If the configured application is missing, the app shows an error and offers fallback behavior without changing the file.
- The context menu offers Open, Open With..., Set Default App for This Type..., and Clear Default App for This Type when applicable.

Program selection should use a native file picker where practical. On Windows, executable filters should favor `.exe`, `.bat`, and `.cmd`. On Debian, executable files can be selected from normal filesystem paths.

## Data Flow

Navigation:

1. User clicks a breadcrumb segment, favorite, up button, or enters a path.
2. The UI calls `FileBrowserWidget::setCurrentPath()` or emits a navigation signal for the current tab.
3. `QFileSystemModel` root index is updated after validation.
4. Breadcrumb, edit path, tab title, metadata panel, and settings state update from `pathChanged`.

View mode:

1. User selects List, Details, or Tiles.
2. `FileBrowserWidget` switches the visible view and reapplies the current root index.
3. The current tab state records the selected mode.

Open-with:

1. User double-clicks a file or selects a context menu action.
2. `FileBrowserWidget` resolves the file extension.
3. A configured app path is used when present and valid.
4. Otherwise the platform default open operation runs.

## Error Handling

- Invalid address entries keep the current path and restore the previous display.
- Inaccessible directories show a clear status or dialog message and do not alter the current tab path.
- Missing favorites are disabled and preserved until removed by the user.
- Missing open-with applications show an error and do not silently fail.
- Launch failures fall back to a user-visible error instead of pretending the file opened.

## Testing

Qt Test coverage should include:

- `FileBrowserWidget` path validation, Ctrl+L edit mode, Escape behavior, and breadcrumb segment navigation.
- `FileBrowserWidget` view mode switching while preserving the current path.
- `FileBrowserWidget` context menu/open-with logic through test seams where UI dialogs cannot be automated directly.
- `FavoritesSidebar` single-click activation and disabled missing-favorite behavior.
- `TabStrip` or `TabManager` plus-tab creation at `QDir::homePath()`.
- `SettingsStore` load/save of `openWithDefaults` and backwards-compatible handling when the field is absent.

Manual verification should cover the native file picker for selecting default applications on Debian and Windows.

## Implementation Notes

The implementation should keep helper logic small and testable. Likely additions include:

- A `BreadcrumbAddressBar` widget or equivalent private helper inside `FileBrowserWidget`.
- A `ViewMode` enum and view-switching API on `FileBrowserWidget`.
- Settings fields and serialization for per-extension open-with defaults.
- A small launch helper in `PlatformServices` or `FileBrowserWidget` so open-with behavior can be tested without launching real apps in unit tests.

No unrelated refactoring is required. Any cleanup should directly support the feature boundaries above.
