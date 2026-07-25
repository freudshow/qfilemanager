# Qt File Manager

Qt File Manager is a Qt 6.11 Widgets desktop file manager project targeting Debian 13 and Windows 11.

The first version uses Qt Widgets with a focused workspace: favorites on the left, tabbed filesystem browsing in the center, and metadata on the right. It stores readable JSON settings in the per-user application config directory and supports copy, move, rename, new folder, delete-to-trash, context menus, drag/drop readiness, and scoped search.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The project requires CMake, a C++20 compiler, and Qt 6.11 with the Core, Gui, Widgets, and Test modules.

## Platform Guides

- Debian 13: see `docs/build-debian-13.md`.
- Windows 11: see `docs/build-windows-11.md`.

## Verification Checklist

- Configure with Qt 6.11: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`.
- Build all targets: `cmake --build build`.
- Run all Qt Test targets: `ctest --test-dir build --output-on-failure`.
- Confirm the registered tests include smoke, settings, main window, file browser, favorites, metadata, file operations, and restore/search coverage.

## First-Version Limits

- Metadata only; no rich previews for images, videos, PDFs, or text files.
- Delete sends items to OS trash/recycle bin; permanent delete is intentionally out of scope.
- Search is scoped to the active path and does not use a global file index.
- Platform-specific properties dialogs are represented by the platform service hook and may be expanded later.
