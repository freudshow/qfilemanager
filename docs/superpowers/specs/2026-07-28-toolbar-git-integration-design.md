# Toolbar and Git Integration Design

Date: 2026-07-28

## Goal

Add an application-level toolbar for navigation and file-view controls, and add a TortoiseGit-like Git submenu to the file browser context menu. Navigation history and view state apply independently to the active tab. Git actions are available only when the clicked path is inside a Git working tree and surface repository modification state.

## Scope

This design extends the existing Qt Widgets `MainWindow`, `FileBrowserWidget`, and tab management behavior. It adds a focused Git service layer that uses the installed Git command-line client.

In scope:

- A top-level toolbar containing Back, Forward, Details, List, and Tiles actions.
- Independent navigation history for each browser tab.
- Moving view-mode controls from the per-tab address row to the main toolbar.
- Git repository discovery from file browser context-menu targets.
- A Git submenu providing Pull, Push, Stash, Stash Pop, Diff, Show Log, Switch Branch, and Status.
- A visible dirty-worktree indicator in the Git submenu title.
- Confirmation before commands that can change local, remote, or checked-out content.
- In-app dialogs for command output, errors, and branch selection.
- Automated tests for history, toolbar synchronization, Git command construction, Git visibility, and error paths.

Out of scope:

- An embedded graphical commit, merge, rebase, conflict-resolution, or remote-management interface.
- Automatic conflict recovery, automatic stashing, or automatic force operations.
- Bundling Git or adding libgit2 as an application dependency.
- System-level Git credential configuration.

## Chosen Approach

Use a new `GitService` with a narrow command-runner seam, backed in production by `QProcess` invoking the user's installed `git` executable. `MainWindow` owns the toolbar and coordinates it with the active `FileBrowserWidget`. `FileBrowserWidget` owns per-tab path history and exposes commands for Back, Forward, and view-mode selection.

This approach fits the existing design: `FileBrowserWidget` already owns browsing state and context menus; `MainWindow` already coordinates shared application UI; `TabManager` already manages independent browser instances. The Git service keeps process execution, repository discovery, and status parsing out of the browser widget, making the UI straightforward to test without invoking real Git commands.

Rejected alternatives:

- Direct `QProcess` calls in `FileBrowserWidget`: smaller initial patch but mixes process management and repository semantics with the view UI.
- libgit2 integration: avoids a CLI dependency but adds a large cross-platform build and deployment dependency for the required feature set.

## User Experience

### Toolbar

`MainWindow` adds a `QToolBar` above the central splitter. It contains these actions in order:

1. Back.
2. Forward.
3. A separator.
4. Details.
5. List.
6. Tiles.

The toolbar always operates on the browser in the selected tab. On a tab change, `MainWindow` reconnects or refreshes the active-browser state so Back and Forward enabled state, plus the selected view-mode action, reflect that browser. The three view actions are mutually exclusive and invoke `FileBrowserWidget::setViewMode()`.

The current compact address row remains inside each browser tab, retaining Up navigation, breadcrumb presentation, and Ctrl+L path editing. Its Details, List, and Tiles buttons are removed to prevent duplicate controls.

### Tab Navigation History

Each `FileBrowserWidget` keeps a sequence of successfully visited absolute directory paths and a current history index.

- The initial successful path becomes the first history entry.
- A normal navigation to a different path appends a new entry after removing any forward entries.
- Back and Forward select an existing history entry without appending a duplicate entry.
- Navigating to the current path does not change history.
- Invalid or inaccessible paths do not change history or current path.
- History is session-only; it is not written to `AppSettings`.

The widget exposes `canGoBack()`, `canGoForward()`, `goBack()`, and `goForward()` so the toolbar can remain UI-only. It emits a history-state signal when navigation changes availability.

### Git Context Menu

The regular file browser context menu continues to include file actions. When its target is within a Git worktree, it additionally contains a `Git` submenu. Target resolution follows these rules:

- A right-clicked directory is the discovery start directory.
- A right-clicked file uses its parent directory.
- A right-click on empty space uses the current browser directory.

Repository discovery walks upward through parent directories until a worktree root is found or the filesystem root is reached. A `.git` directory and a `.git` worktree file are both valid markers. The root is passed as the process working directory for every Git command.

The submenu title is `Git` for a clean repository and `Git (modified)` with a warning/dirty icon when `git status --porcelain` returns any records. Untracked, modified, staged, deleted, renamed, and unmerged entries all make the repository modified. A non-repository target has no Git submenu.

Git actions are:

- Pull: `git pull`.
- Push: `git push`.
- Stash: `git stash push`.
- Stash Pop: `git stash pop`.
- Diff: for an item target, `git diff -- <repository-relative-path>`; for a background target, `git diff`.
- Show Log: `git log --decorate --oneline -n 100`, using an item pathspec where the target is a file or directory below the root.
- Switch Branch: reads local branches with `git branch --format=%(refname:short)`, then uses the selected branch in `git switch <branch>`.
- Status: `git status --short --branch`.

The Git menu refreshes repository state each time it is opened. Once a command completes, it refreshes status again so the dirty indicator reflects the result.

## Components and Data Flow

### `FileBrowserWidget`

Responsibilities:

- Maintain the active directory, breadcrumb, address edit mode, view mode, and the per-tab history list.
- Emit `pathChanged()` for all successful directory changes, including history navigation.
- Expose history capabilities and slots or methods for toolbar actions.
- Request Git menu construction through a small signal or callback supplied by the owning window, rather than owning Git process state.

`setCurrentPath()` gains an internal navigation origin parameter or private helper. Normal navigation records a visit; Back and Forward reuse the shared path-update helper but suppress recording.

### `MainWindow`

Responsibilities:

- Create and own the `QToolBar` and its `QAction` instances.
- Resolve the selected browser from `TabManager` and invoke its Back, Forward, and view-mode APIs.
- Update toolbar enabled and checked state after tab changes, path changes, history changes, and view-mode changes.
- Act as the Git UI coordinator: build the Git submenu, show confirmations, show text-output dialogs, prompt for a branch, and refresh menu state after operations.

### `GitService`

Responsibilities:

- Discover the worktree root from a path.
- Run Git commands relative to a discovered root.
- Query working-tree state and local branches.
- Return command success, exit code, standard output, and standard error through a value type.

`GitService` uses an injectable command runner for tests. Production execution uses `QProcess` asynchronously to keep the UI responsive. A command's completion callback is invoked on the Qt event loop and includes the combined structured result. The service never uses shell strings; it passes program and arguments separately.

### Operation Flow

1. User invokes the file browser context menu.
2. `FileBrowserWidget` determines the target path and asks `MainWindow` to attach a Git submenu when `GitService` finds a root.
3. `GitService` queries status and `MainWindow` updates the submenu title/icon.
4. User selects a Git action.
5. Read-only commands run immediately; state-changing commands show a confirmation explaining the exact Git operation.
6. `GitService` runs the accepted command asynchronously.
7. `MainWindow` shows output or a failure dialog, then refreshes the Git status indicator.

## Confirmation and Error Handling

The following commands require a confirmation dialog before a process is started: Pull, Push, Stash, Stash Pop, and Switch Branch. The dialog names the command and warns that it may update files, repository state, or a remote. Selecting Cancel performs no operation.

Diff, Show Log, and Status are read-only and execute without confirmation. Their standard output appears in a read-only, selectable text dialog. Empty successful Diff output is presented as `No differences.`; empty successful log output is presented as `No commits found.`

Git unavailable, repository discovery failures, start failures, timeouts, and non-zero exits result in a clear error dialog containing the action name and useful standard-error output. The application does not alter the current folder or attempt a rollback. Merge conflicts, stash-pop conflicts, and failed branch switches are reported exactly as Git returns them; the application does not auto-resolve or auto-recover.

Branch listing failure prevents Switch Branch and shows the error. A selected empty branch is ignored. Switching to the currently checked-out branch is allowed to be passed to Git, whose output is displayed if it reports an error or no-op.

## Testing

Qt Test coverage will include:

- `FileBrowserWidget` appends successful ordinary directory navigations to per-tab history.
- Back and Forward restore expected paths, do not create duplicate history entries, and disable at the history bounds.
- New navigation after Back discards the forward branch.
- Invalid navigation does not mutate history.
- `MainWindow` toolbar actions affect only the active tab and synchronize after changing tabs.
- The toolbar view actions switch the active browser and correctly reflect its selected view mode.
- `GitService` discovers roots for directories and files, rejects non-repository paths, and supports `.git` worktree files.
- `GitService` maps clean and dirty porcelain output correctly and constructs each action's program, working directory, and arguments without shell interpolation.
- Context menu construction omits Git outside repositories and marks the submenu dirty for non-empty porcelain status.
- State-changing menu actions show a confirmation and do not call the runner when cancelled.
- Read-only results and command failures select the appropriate output or error dialog behavior through test seams.

Existing file browsing, breadcrumbs, open-with, favorites, metadata, and tab tests remain part of the regression suite.

## Manual Verification

On Linux and Windows with Git installed:

1. Open a clean repository and confirm that the Git submenu is visible without the modified marker.
2. Modify, stage, create, or delete a file and confirm that the marker appears after reopening the context menu.
3. Run Diff, Status, and Show Log and verify readable, selectable output.
4. Cancel each destructive or remote-affecting confirmation and confirm no command runs.
5. Exercise Pull, Push, Stash, Stash Pop, and Switch Branch against a disposable repository with appropriate remote credentials.
6. Confirm Git errors and merge conflicts remain visible and no automatic cleanup occurs.
