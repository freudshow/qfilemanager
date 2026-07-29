# Toolbar and Git Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add active-tab toolbar controls, independent navigation history, and safe TortoiseGit-like context-menu actions backed by the installed Git CLI.

**Architecture:** Keep directory history and view state inside each `FileBrowserWidget`; let `MainWindow` own the application toolbar and bind it to the selected tab. Add a focused asynchronous `GitService` with an injectable command runner, then have `MainWindow` use it to populate Git submenus and dialogs without coupling Git process handling to browser rendering.

**Tech Stack:** C++20, Qt 6.11 Core/Gui/Widgets/Test, `QProcess`, CMake, Qt Test, Git CLI.

---

## File Structure

- Create: `src/services/GitService.h` - Git result types, repository discovery, command-runner seam, asynchronous query/command API.
- Create: `src/services/GitService.cpp` - `.git` marker discovery and production `QProcess` command runner.
- Create: `tests/test_git_service.cpp` - deterministic repository discovery, status, argument, and asynchronous-runner tests.
- Modify: `CMakeLists.txt` - compile `GitService` into `filemanager_ui`.
- Modify: `tests/CMakeLists.txt` - register `filemanager_git_service_test`.
- Modify: `src/ui/FileBrowserWidget.h` - expose per-tab history APIs, emit history/view updates, and request Git submenu population.
- Modify: `src/ui/FileBrowserWidget.cpp` - record navigations, remove per-tab view buttons, and attach Git requests to context menus.
- Modify: `src/MainWindow.h` - declare toolbar synchronization and Git menu/dialog helpers; own actions and `GitService`.
- Modify: `src/MainWindow.cpp` - create toolbar, bind it to current tab, build Git menu actions, confirmations, output dialogs, and branch picker.
- Modify: `tests/test_filebrowser.cpp` - test history traversal and correct forward-branch truncation.
- Modify: `tests/test_mainwindow.cpp` - test toolbar synchronization and Git-menu request behavior through the browser signal.

## Task 1: Add a Testable Git Service

**Files:**
- Create: `src/services/GitService.h`
- Create: `src/services/GitService.cpp`
- Create: `tests/test_git_service.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing Git service tests**

Create `tests/test_git_service.cpp`:

```cpp
#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "services/GitService.h"

class GitServiceTest : public QObject {
    Q_OBJECT

private slots:
    void findsRepositoryRootForDirectoryAndFile();
    void recognizesWorktreeGitFile();
    void returnsEmptyRootOutsideRepository();
    void parsesDirtyStatus();
    void forwardsWorkingDirectoryAndArgumentsToRunner();
};

void GitServiceTest::findsRepositoryRootForDirectoryAndFile() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir(directory.path()).mkpath(".git"));
    QVERIFY(QDir(directory.path()).mkpath("src/nested"));
    const QString filePath = QDir(directory.path()).filePath("src/nested/file.cpp");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    GitService service;
    QCOMPARE(service.findRepositoryRoot(QDir(directory.path()).filePath("src/nested")), directory.path());
    QCOMPARE(service.findRepositoryRoot(filePath), directory.path());
}

void GitServiceTest::recognizesWorktreeGitFile() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile gitFile(QDir(directory.path()).filePath(".git"));
    QVERIFY(gitFile.open(QIODevice::WriteOnly));
    gitFile.write("gitdir: /tmp/linked-worktree\n");
    gitFile.close();

    GitService service;
    QCOMPARE(service.findRepositoryRoot(directory.path()), directory.path());
}

void GitServiceTest::returnsEmptyRootOutsideRepository() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    GitService service;
    QVERIFY(service.findRepositoryRoot(directory.path()).isEmpty());
}

void GitServiceTest::parsesDirtyStatus() {
    QVERIFY(!GitService::isDirtyPorcelainOutput(QString()));
    QVERIFY(GitService::isDirtyPorcelainOutput(QStringLiteral(" M src/main.cpp\n")));
    QVERIFY(GitService::isDirtyPorcelainOutput(QStringLiteral("?? untracked.txt\n")));
}

void GitServiceTest::forwardsWorkingDirectoryAndArgumentsToRunner() {
    QString capturedDirectory;
    QStringList capturedArguments;
    GitService service;
    service.setCommandRunner([&](const QString &workingDirectory, const QStringList &arguments, GitService::CommandCallback callback) {
        capturedDirectory = workingDirectory;
        capturedArguments = arguments;
        callback({true, 0, QStringLiteral("ok"), {}, {}});
    });

    bool completed = false;
    service.run(QStringLiteral("/repository"), {QStringLiteral("status"), QStringLiteral("--porcelain")}, [&](const GitCommandResult &result) {
        completed = result.succeeded();
    });

    QCOMPARE(capturedDirectory, QStringLiteral("/repository"));
    QCOMPARE(capturedArguments, QStringList({QStringLiteral("status"), QStringLiteral("--porcelain")}));
    QVERIFY(completed);
}

QTEST_MAIN(GitServiceTest)
#include "test_git_service.moc"
```

Add this test executable to `tests/CMakeLists.txt` after the other service tests:

```cmake
add_executable(filemanager_git_service_test test_git_service.cpp)
target_link_libraries(filemanager_git_service_test PRIVATE Qt6::Core Qt6::Test filemanager_ui)
add_test(NAME GitServiceTest COMMAND filemanager_git_service_test)
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R GitServiceTest
```

Expected: CMake or compilation fails because `GitService.h` and its test target do not exist.

- [ ] **Step 3: Define the Git service interface**

Create `src/services/GitService.h`:

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

struct GitCommandResult {
    bool started = false;
    int exitCode = -1;
    QString standardOutput;
    QString standardError;
    QString startError;

    bool succeeded() const {
        return started && exitCode == 0;
    }
};

class GitService : public QObject {
    Q_OBJECT

public:
    using CommandCallback = std::function<void(const GitCommandResult &result)>;
    using CommandRunner = std::function<void(const QString &workingDirectory, const QStringList &arguments, CommandCallback callback)>;

    explicit GitService(QObject *parent = nullptr);

    QString findRepositoryRoot(const QString &path) const;
    static bool isDirtyPorcelainOutput(const QString &output);
    void run(const QString &repositoryRoot, const QStringList &arguments, CommandCallback callback) const;
    void setCommandRunner(CommandRunner runner);

private:
    void runWithQProcess(const QString &workingDirectory, const QStringList &arguments, CommandCallback callback) const;

    CommandRunner commandRunner_;
};
```

- [ ] **Step 4: Implement repository discovery and asynchronous process execution**

Create `src/services/GitService.cpp`:

```cpp
#include "services/GitService.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

GitService::GitService(QObject *parent)
    : QObject(parent) {
}

QString GitService::findRepositoryRoot(const QString &path) const {
    QFileInfo info(path);
    QDir directory(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
    while (directory.exists()) {
        const QFileInfo marker(directory.filePath(QStringLiteral(".git")));
        if (marker.isDir() || marker.isFile()) {
            return QDir::cleanPath(directory.absolutePath());
        }
        if (!directory.cdUp()) {
            break;
        }
    }
    return {};
}

bool GitService::isDirtyPorcelainOutput(const QString &output) {
    return !output.trimmed().isEmpty();
}

void GitService::run(const QString &repositoryRoot, const QStringList &arguments, CommandCallback callback) const {
    if (repositoryRoot.isEmpty()) {
        callback({false, -1, {}, {}, tr("No Git repository was found.")});
        return;
    }
    if (commandRunner_) {
        commandRunner_(repositoryRoot, arguments, std::move(callback));
        return;
    }
    runWithQProcess(repositoryRoot, arguments, std::move(callback));
}

void GitService::setCommandRunner(CommandRunner runner) {
    commandRunner_ = std::move(runner);
}

void GitService::runWithQProcess(const QString &workingDirectory, const QStringList &arguments, CommandCallback callback) const {
    auto *process = new QProcess(const_cast<GitService *>(this));
    process->setWorkingDirectory(workingDirectory);
    connect(process, &QProcess::errorOccurred, process, [process, callback](QProcess::ProcessError) {
        if (process->state() == QProcess::NotRunning && process->exitCode() == -1) {
            callback({false, -1, {}, {}, process->errorString()});
            process->deleteLater();
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process, [process, callback](int exitCode, QProcess::ExitStatus) {
        callback({true, exitCode, QString::fromUtf8(process->readAllStandardOutput()), QString::fromUtf8(process->readAllStandardError()), {}});
        process->deleteLater();
    });
    process->start(QStringLiteral("git"), arguments);
}
```

Add the new source files to the `filemanager_ui` list in `CMakeLists.txt`:

```cmake
    src/services/GitService.cpp
    src/services/GitService.h
```

- [ ] **Step 5: Run the Git service tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R GitServiceTest
```

Expected: `GitServiceTest` passes.

- [ ] **Step 6: Commit the Git service**

```bash
git add CMakeLists.txt tests/CMakeLists.txt src/services/GitService.h src/services/GitService.cpp tests/test_git_service.cpp
```

## Task 2: Add Independent Browser History

**Files:**
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Modify: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Write failing history tests**

Add these slots to `FileBrowserWidgetTest` in `tests/test_filebrowser.cpp`:

```cpp
void tracksBackAndForwardHistory();
void discardsForwardHistoryAfterNewNavigation();
void invalidNavigationDoesNotChangeHistory();
```

Add the test bodies:

```cpp
void FileBrowserWidgetTest::tracksBackAndForwardHistory() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("one/two"));
    const QString one = QDir(root.path()).filePath("one");
    const QString two = QDir(one).filePath("two");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));
    QVERIFY(browser.setCurrentPath(one));
    QVERIFY(browser.setCurrentPath(two));
    QVERIFY(browser.canGoBack());
    QVERIFY(!browser.canGoForward());

    QVERIFY(browser.goBack());
    QCOMPARE(browser.currentPath(), one);
    QVERIFY(browser.canGoBack());
    QVERIFY(browser.canGoForward());

    QVERIFY(browser.goBack());
    QCOMPARE(browser.currentPath(), root.path());
    QVERIFY(!browser.canGoBack());
    QVERIFY(browser.canGoForward());

    QVERIFY(browser.goForward());
    QCOMPARE(browser.currentPath(), one);
}

void FileBrowserWidgetTest::discardsForwardHistoryAfterNewNavigation() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath("one"));
    QVERIFY(QDir(root.path()).mkpath("two"));
    QVERIFY(QDir(root.path()).mkpath("three"));
    const QString one = QDir(root.path()).filePath("one");
    const QString two = QDir(root.path()).filePath("two");
    const QString three = QDir(root.path()).filePath("three");

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));
    QVERIFY(browser.setCurrentPath(one));
    QVERIFY(browser.setCurrentPath(two));
    QVERIFY(browser.goBack());
    QVERIFY(browser.setCurrentPath(three));

    QCOMPARE(browser.currentPath(), three);
    QVERIFY(!browser.canGoForward());
}

void FileBrowserWidgetTest::invalidNavigationDoesNotChangeHistory() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));
    QVERIFY(!browser.setCurrentPath(QDir(root.path()).filePath("missing")));

    QVERIFY(!browser.canGoBack());
    QVERIFY(!browser.canGoForward());
}
```

- [ ] **Step 2: Run the browser test to verify it fails**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: compilation fails because `canGoBack()`, `canGoForward()`, `goBack()`, and `goForward()` do not exist.

- [ ] **Step 3: Add the history interface and state**

In `src/ui/FileBrowserWidget.h`, add public methods and signals:

```cpp
bool canGoBack() const;
bool canGoForward() const;
bool goBack();
bool goForward();
```

```cpp
void historyChanged(bool canGoBack, bool canGoForward);
void viewModeChanged(FileBrowserWidget::ViewMode mode);
```

Add private helpers and state:

```cpp
bool applyPath(const QString &path, bool recordHistory);
void recordHistoryPath(const QString &path);

QStringList history_;
int historyIndex_ = -1;
```

- [ ] **Step 4: Implement history without duplicating navigation logic**

In `src/ui/FileBrowserWidget.cpp`, make `setCurrentPath()` delegate to `applyPath(path, true)` and implement:

```cpp
bool FileBrowserWidget::setCurrentPath(const QString &path) {
    return applyPath(path, true);
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
    detailsView_->setRootIndex(rootIndex);
    listView_->setRootIndex(rootIndex);
    tilesView_->setRootIndex(rootIndex);
    currentPath_ = absolutePath;
    addressBar_->setText(currentPath_);
    rebuildBreadcrumbs();
    leaveAddressEditMode();
    if (recordHistory) {
        recordHistoryPath(currentPath_);
    }
    emit pathChanged(currentPath_);
    return true;
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
    --historyIndex_;
    const bool changed = applyPath(history_.at(historyIndex_), false);
    if (changed) {
        emit historyChanged(canGoBack(), canGoForward());
    }
    return changed;
}

bool FileBrowserWidget::goForward() {
    if (!canGoForward()) {
        return false;
    }
    ++historyIndex_;
    const bool changed = applyPath(history_.at(historyIndex_), false);
    if (changed) {
        emit historyChanged(canGoBack(), canGoForward());
    }
    return changed;
}
```

At the end of `setViewMode()`, emit the new state signal:

```cpp
emit viewModeChanged(viewMode_);
```

- [ ] **Step 5: Run the browser test to verify it passes**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: `FileBrowserWidgetTest` passes, including the new history cases and all existing browsing cases.

- [ ] **Step 6: Commit browser history**

```bash
git add src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp tests/test_filebrowser.cpp
```

## Task 3: Move View Controls to an Active-Tab Toolbar

**Files:**
- Modify: `src/MainWindow.h`
- Modify: `src/MainWindow.cpp`
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Modify: `tests/test_mainwindow.cpp`

- [ ] **Step 1: Write failing toolbar tests**

Add slots to `MainWindowTest` in `tests/test_mainwindow.cpp`:

```cpp
void toolbarControlsActiveTabAndSynchronizesHistory();
void toolbarViewActionsSynchronizeWithActiveTab();
```

Add test bodies:

```cpp
void MainWindowTest::toolbarControlsActiveTabAndSynchronizesHistory() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(QDir(first.path()).mkdir("child"));

    AppSettings settings;
    settings.tabs.append({first.path(), {}, {}});
    settings.tabs.append({second.path(), {}, {}});
    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    MainWindow window;
    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    auto *back = window.findChild<QAction *>("backAction");
    auto *forward = window.findChild<QAction *>("forwardAction");
    QVERIFY(back != nullptr);
    QVERIFY(forward != nullptr);

    auto *firstBrowser = qobject_cast<FileBrowserWidget *>(tabWidget->widget(0));
    QVERIFY(firstBrowser != nullptr);
    QVERIFY(firstBrowser->setCurrentPath(QDir(first.path()).filePath("child")));
    QVERIFY(back->isEnabled());

    tabWidget->setCurrentIndex(1);
    QVERIFY(!back->isEnabled());
    QVERIFY(!forward->isEnabled());

    tabWidget->setCurrentIndex(0);
    QVERIFY(back->isEnabled());
    back->trigger();
    QCOMPARE(firstBrowser->currentPath(), first.path());
}

void MainWindowTest::toolbarViewActionsSynchronizeWithActiveTab() {
    MainWindow window;
    auto *tabWidget = findTabWidget(window);
    QVERIFY(tabWidget != nullptr);
    auto *details = window.findChild<QAction *>("detailsViewAction");
    auto *list = window.findChild<QAction *>("listViewAction");
    auto *tiles = window.findChild<QAction *>("tilesViewAction");
    QVERIFY(details != nullptr);
    QVERIFY(list != nullptr);
    QVERIFY(tiles != nullptr);

    auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget->currentWidget());
    QVERIFY(browser != nullptr);
    list->trigger();
    QCOMPARE(browser->viewMode(), FileBrowserWidget::ViewMode::List);
    QVERIFY(list->isChecked());

    tiles->trigger();
    QCOMPARE(browser->viewMode(), FileBrowserWidget::ViewMode::Tiles);
    QVERIFY(tiles->isChecked());
}
```

- [ ] **Step 2: Run the main-window test to verify it fails**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R MainWindowTest
```

Expected: failure because the named toolbar actions do not exist.

- [ ] **Step 3: Remove duplicate browser-level view buttons**

In `src/ui/FileBrowserWidget.h`, remove these member declarations:

```cpp
QToolButton *listViewButton_ = nullptr;
QToolButton *detailsViewButton_ = nullptr;
QToolButton *tilesViewButton_ = nullptr;
```

In `src/ui/FileBrowserWidget.cpp`, remove their construction, object names, text/tooltips, additions to `addressLayout`, click connections, and matching stylesheet selectors. Keep `upButton_`, breadcrumbs, and Ctrl+L behavior intact.

Delete `FileBrowserWidgetTest::viewModeButtonsChangeActiveView()` and its declaration from `tests/test_filebrowser.cpp`, because view control now belongs to the window toolbar; retain the direct `setViewMode()` tests.

- [ ] **Step 4: Add toolbar members and synchronization helpers**

In `src/MainWindow.h`, add forward declarations:

```cpp
class QAction;
class QActionGroup;
class QToolBar;
```

Add private methods:

```cpp
FileBrowserWidget *currentBrowser() const;
void updateToolbar();
void connectBrowserToolbar(FileBrowserWidget *browser);
```

Add members:

```cpp
QToolBar *toolbar_ = nullptr;
QAction *backAction_ = nullptr;
QAction *forwardAction_ = nullptr;
QAction *detailsViewAction_ = nullptr;
QAction *listViewAction_ = nullptr;
QAction *tilesViewAction_ = nullptr;
QActionGroup *viewModeActionGroup_ = nullptr;
```

- [ ] **Step 5: Create the toolbar and connect it to selected tabs**

In `src/MainWindow.cpp`, include:

```cpp
#include <QAction>
#include <QActionGroup>
#include <QToolBar>
```

After `setWindowTitle(...)` in the constructor, create the toolbar:

```cpp
toolbar_ = addToolBar(tr("Browse"));
toolbar_->setObjectName("mainToolbar");
toolbar_->setMovable(false);

backAction_ = toolbar_->addAction(tr("Back"));
backAction_->setObjectName("backAction");
backAction_->setToolTip(tr("Go back in this tab"));
forwardAction_ = toolbar_->addAction(tr("Forward"));
forwardAction_->setObjectName("forwardAction");
forwardAction_->setToolTip(tr("Go forward in this tab"));
toolbar_->addSeparator();

viewModeActionGroup_ = new QActionGroup(this);
viewModeActionGroup_->setExclusive(true);
detailsViewAction_ = toolbar_->addAction(tr("Details"));
detailsViewAction_->setObjectName("detailsViewAction");
listViewAction_ = toolbar_->addAction(tr("List"));
listViewAction_->setObjectName("listViewAction");
tilesViewAction_ = toolbar_->addAction(tr("Tiles"));
tilesViewAction_->setObjectName("tilesViewAction");
for (QAction *action : {detailsViewAction_, listViewAction_, tilesViewAction_}) {
    action->setCheckable(true);
    viewModeActionGroup_->addAction(action);
}
```

Add action connections:

```cpp
connect(backAction_, &QAction::triggered, this, [this] {
    if (FileBrowserWidget *browser = currentBrowser()) {
        browser->goBack();
    }
});
connect(forwardAction_, &QAction::triggered, this, [this] {
    if (FileBrowserWidget *browser = currentBrowser()) {
        browser->goForward();
    }
});
connect(detailsViewAction_, &QAction::triggered, this, [this] {
    if (FileBrowserWidget *browser = currentBrowser()) {
        browser->setViewMode(FileBrowserWidget::ViewMode::Details);
    }
});
connect(listViewAction_, &QAction::triggered, this, [this] {
    if (FileBrowserWidget *browser = currentBrowser()) {
        browser->setViewMode(FileBrowserWidget::ViewMode::List);
    }
});
connect(tilesViewAction_, &QAction::triggered, this, [this] {
    if (FileBrowserWidget *browser = currentBrowser()) {
        browser->setViewMode(FileBrowserWidget::ViewMode::Tiles);
    }
});
```

After the existing browser setup loop, connect current-tab changes and initialize state:

```cpp
connect(tabManager_->tabWidget(), &QTabWidget::currentChanged, this, [this](int) {
    updateToolbar();
});
updateToolbar();
```

Implement the helpers:

```cpp
FileBrowserWidget *MainWindow::currentBrowser() const {
    return tabManager_ == nullptr || tabManager_->tabWidget() == nullptr
        ? nullptr
        : qobject_cast<FileBrowserWidget *>(tabManager_->tabWidget()->currentWidget());
}

void MainWindow::connectBrowserToolbar(FileBrowserWidget *browser) {
    if (browser == nullptr) {
        return;
    }
    connect(browser, &FileBrowserWidget::historyChanged, this, [this](bool, bool) { updateToolbar(); }, Qt::UniqueConnection);
    connect(browser, &FileBrowserWidget::viewModeChanged, this, [this](FileBrowserWidget::ViewMode) { updateToolbar(); }, Qt::UniqueConnection);
}

void MainWindow::updateToolbar() {
    FileBrowserWidget *browser = currentBrowser();
    const bool available = browser != nullptr;
    backAction_->setEnabled(available && browser->canGoBack());
    forwardAction_->setEnabled(available && browser->canGoForward());
    detailsViewAction_->setChecked(available && browser->viewMode() == FileBrowserWidget::ViewMode::Details);
    listViewAction_->setChecked(available && browser->viewMode() == FileBrowserWidget::ViewMode::List);
    tilesViewAction_->setChecked(available && browser->viewMode() == FileBrowserWidget::ViewMode::Tiles);
}
```

Call `connectBrowserToolbar(browser)` in the existing `tabAdded` lambda and existing-browser setup loop.

- [ ] **Step 6: Run toolbar and browser regression tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R "MainWindowTest|FileBrowserWidgetTest"
```

Expected: both named test suites pass; no browser-level view-mode buttons remain.

- [ ] **Step 7: Commit toolbar integration**

```bash
git add src/MainWindow.h src/MainWindow.cpp src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp tests/test_mainwindow.cpp tests/test_filebrowser.cpp
```

## Task 4: Request Git Submenus from Browser Context Menus

**Files:**
- Modify: `src/ui/FileBrowserWidget.h`
- Modify: `src/ui/FileBrowserWidget.cpp`
- Modify: `tests/test_filebrowser.cpp`

- [ ] **Step 1: Write the failing Git-request test**

Add this slot and test body to `FileBrowserWidgetTest`:

```cpp
void emitsGitMenuRequestForFileAndBackgroundTarget();
```

```cpp
void FileBrowserWidgetTest::emitsGitMenuRequestForFileAndBackgroundTarget() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("tracked.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    FileBrowserWidget browser;
    QVERIFY(browser.setCurrentPath(root.path()));
    QSignalSpy requestSpy(&browser, &FileBrowserWidget::gitMenuRequested);

    emit browser.gitMenuRequested(nullptr, filePath, false);
    emit browser.gitMenuRequested(nullptr, root.path(), true);

    QCOMPARE(requestSpy.count(), 2);
    QCOMPARE(requestSpy.at(0).at(1).toString(), filePath);
    QVERIFY(!requestSpy.at(0).at(2).toBool());
    QCOMPARE(requestSpy.at(1).at(1).toString(), root.path());
    QVERIFY(requestSpy.at(1).at(2).toBool());
}
```

- [ ] **Step 2: Run the browser test to verify it fails**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: compilation fails because `gitMenuRequested` does not exist.

- [ ] **Step 3: Add a narrow Git-menu request signal**

In `src/ui/FileBrowserWidget.h`, forward declare `QMenu` and add:

```cpp
void gitMenuRequested(QMenu *parentMenu, const QString &targetPath, bool backgroundTarget);
```

In `FileBrowserWidget::showContextMenu()` in `src/ui/FileBrowserWidget.cpp`, after the standard actions and before `menu.exec(...)`, emit a request after adding a separator:

```cpp
const bool backgroundTarget = !index.isValid();
menu.addSeparator();
emit gitMenuRequested(&menu, path, backgroundTarget);
```

Do not put Git command execution or repository discovery in `FileBrowserWidget`; the signal keeps it independent of Git infrastructure.

- [ ] **Step 4: Run the browser test to verify it passes**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R FileBrowserWidgetTest
```

Expected: `FileBrowserWidgetTest` passes.

- [ ] **Step 5: Commit Git menu request plumbing**

```bash
git add src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp tests/test_filebrowser.cpp
```

## Task 5: Populate and Run Safe Git Context Actions

**Files:**
- Modify: `src/MainWindow.h`
- Modify: `src/MainWindow.cpp`
- Modify: `tests/test_mainwindow.cpp`

- [ ] **Step 1: Write failing MainWindow Git-menu tests**

Add `#include <QMenu>` and `#include "services/GitService.h"` to `tests/test_mainwindow.cpp`. Add slots:

```cpp
void addsGitSubmenuOnlyForRepositoryTargets();
void gitMenuShowsDirtyStateAndUsesTargetPathspec();
```

Add these test bodies. They invoke the browser signal directly so no modal context menu must be displayed:

```cpp
void MainWindowTest::addsGitSubmenuOnlyForRepositoryTargets() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(".git"));
    MainWindow window;
    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QVERIFY(browser != nullptr);

    QMenu menu;
    emit browser->gitMenuRequested(&menu, root.path(), true);
    const auto gitMenus = menu.findChildren<QMenu *>();
    QVERIFY(!gitMenus.isEmpty());
    QCOMPARE(gitMenus.constFirst()->title(), QString("Git"));
}

void MainWindowTest::gitMenuShowsDirtyStateAndUsesTargetPathspec() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(".git"));
    QVERIFY(QDir(root.path()).mkdir("src"));
    const QString filePath = QDir(root.path()).filePath("src/main.cpp");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    MainWindow window;
    auto *service = window.findChild<GitService *>("gitService");
    QVERIFY(service != nullptr);
    service->setCommandRunner([](const QString &, const QStringList &arguments, GitService::CommandCallback callback) {
        if (arguments == QStringList({QStringLiteral("status"), QStringLiteral("--porcelain")})) {
            callback({true, 0, QStringLiteral(" M src/main.cpp\n"), {}, {}});
            return;
        }
        callback({true, 0, {}, {}, {}});
    });

    auto *browser = qobject_cast<FileBrowserWidget *>(findTabWidget(window)->currentWidget());
    QMenu menu;
    emit browser->gitMenuRequested(&menu, filePath, false);
    auto *gitMenu = menu.findChild<QMenu *>();
    QVERIFY(gitMenu != nullptr);
    emit gitMenu->aboutToShow();
    QTRY_COMPARE(gitMenu->title(), QString("Git (modified)"));
    QVERIFY(gitMenu->actions().contains(gitMenu->findChild<QAction *>("gitDiffAction")));
}
```

- [ ] **Step 2: Run the MainWindow test to verify it fails**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R MainWindowTest
```

Expected: failure because `MainWindow` has no Git service or Git submenu builder.

- [ ] **Step 3: Add Git ownership and menu helper declarations**

In `src/MainWindow.h`, forward declare `GitService`, `QMenu`, and `QTextEdit`. Add methods:

```cpp
void connectBrowserGitMenu(FileBrowserWidget *browser);
void populateGitMenu(QMenu *parentMenu, const QString &targetPath, bool backgroundTarget);
void refreshGitMenuTitle(QMenu *gitMenu, const QString &repositoryRoot);
void runGitAction(const QString &title, const QString &repositoryRoot, const QStringList &arguments, bool requiresConfirmation);
void showGitOutput(const QString &title, const GitCommandResult &result, const QString &emptyMessage = QString());
void switchGitBranch(const QString &repositoryRoot);
```

Add a member:

```cpp
GitService *gitService_ = nullptr;
```

- [ ] **Step 4: Instantiate the service and attach it to every browser**

In `src/MainWindow.cpp`, include:

```cpp
#include "services/GitService.h"

#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTextEdit>
#include <QVBoxLayout>
```

Initialize the service in the initializer list:

```cpp
, gitService_(new GitService(this))
```

Immediately after construction, set an object name for the test seam:

```cpp
gitService_->setObjectName("gitService");
```

Extend the existing `tabAdded` lambda and existing-browser loop to call `connectBrowserGitMenu(browser)`.

Implement:

```cpp
void MainWindow::connectBrowserGitMenu(FileBrowserWidget *browser) {
    if (browser == nullptr) {
        return;
    }
    connect(browser, &FileBrowserWidget::gitMenuRequested, this, &MainWindow::populateGitMenu, Qt::UniqueConnection);
}
```

- [ ] **Step 5: Build the Git submenu and refresh its dirty marker**

Implement `populateGitMenu()` in `src/MainWindow.cpp`:

```cpp
void MainWindow::populateGitMenu(QMenu *parentMenu, const QString &targetPath, bool backgroundTarget) {
    if (parentMenu == nullptr || gitService_ == nullptr) {
        return;
    }
    const QString repositoryRoot = gitService_->findRepositoryRoot(targetPath);
    if (repositoryRoot.isEmpty()) {
        return;
    }

    auto *gitMenu = parentMenu->addMenu(tr("Git"));
    gitMenu->setObjectName("gitContextMenu");
    QAction *pullAction = gitMenu->addAction(tr("Pull"));
    QAction *pushAction = gitMenu->addAction(tr("Push"));
    gitMenu->addSeparator();
    QAction *stashAction = gitMenu->addAction(tr("Stash"));
    QAction *stashPopAction = gitMenu->addAction(tr("Stash Pop"));
    gitMenu->addSeparator();
    QAction *diffAction = gitMenu->addAction(tr("Diff"));
    diffAction->setObjectName("gitDiffAction");
    QAction *logAction = gitMenu->addAction(tr("Show Log"));
    QAction *switchAction = gitMenu->addAction(tr("Switch Branch..."));
    QAction *statusAction = gitMenu->addAction(tr("Status"));

    const QFileInfo targetInfo(targetPath);
    const QString relativeTarget = backgroundTarget ? QString() : QDir(repositoryRoot).relativeFilePath(targetInfo.absoluteFilePath());
    const auto withPathspec = [relativeTarget](QStringList arguments) {
        if (!relativeTarget.isEmpty() && relativeTarget != QStringLiteral(".")) {
            arguments << QStringLiteral("--") << relativeTarget;
        }
        return arguments;
    };

    connect(pullAction, &QAction::triggered, this, [this, repositoryRoot] { runGitAction(tr("Pull"), repositoryRoot, {QStringLiteral("pull")}, true); });
    connect(pushAction, &QAction::triggered, this, [this, repositoryRoot] { runGitAction(tr("Push"), repositoryRoot, {QStringLiteral("push")}, true); });
    connect(stashAction, &QAction::triggered, this, [this, repositoryRoot] { runGitAction(tr("Stash"), repositoryRoot, {QStringLiteral("stash"), QStringLiteral("push")}, true); });
    connect(stashPopAction, &QAction::triggered, this, [this, repositoryRoot] { runGitAction(tr("Stash Pop"), repositoryRoot, {QStringLiteral("stash"), QStringLiteral("pop")}, true); });
    connect(diffAction, &QAction::triggered, this, [this, repositoryRoot, withPathspec] { runGitAction(tr("Diff"), repositoryRoot, withPathspec({QStringLiteral("diff")}), false); });
    connect(logAction, &QAction::triggered, this, [this, repositoryRoot, withPathspec] { runGitAction(tr("Show Log"), repositoryRoot, withPathspec({QStringLiteral("log"), QStringLiteral("--decorate"), QStringLiteral("--oneline"), QStringLiteral("-n"), QStringLiteral("100")}), false); });
    connect(switchAction, &QAction::triggered, this, [this, repositoryRoot] { switchGitBranch(repositoryRoot); });
    connect(statusAction, &QAction::triggered, this, [this, repositoryRoot] { runGitAction(tr("Status"), repositoryRoot, {QStringLiteral("status"), QStringLiteral("--short"), QStringLiteral("--branch")}, false); });
    connect(gitMenu, &QMenu::aboutToShow, this, [this, gitMenu, repositoryRoot] { refreshGitMenuTitle(gitMenu, repositoryRoot); });
}
```

Implement state refresh with a standard icon rather than a Unicode glyph:

```cpp
void MainWindow::refreshGitMenuTitle(QMenu *gitMenu, const QString &repositoryRoot) {
    gitService_->run(repositoryRoot, {QStringLiteral("status"), QStringLiteral("--porcelain")}, [this, gitMenu](const GitCommandResult &result) {
        if (gitMenu == nullptr || !result.succeeded()) {
            return;
        }
        const bool dirty = GitService::isDirtyPorcelainOutput(result.standardOutput);
        gitMenu->setTitle(dirty ? tr("Git (modified)") : tr("Git"));
        gitMenu->setIcon(dirty ? style()->standardIcon(QStyle::SP_MessageBoxWarning) : QIcon());
    });
}
```

Add `#include <QStyle>` for `QStyle::SP_MessageBoxWarning`.

- [ ] **Step 6: Implement confirmation, output, and branch selection**

Implement `runGitAction()` and `showGitOutput()`:

```cpp
void MainWindow::runGitAction(const QString &title, const QString &repositoryRoot, const QStringList &arguments, bool requiresConfirmation) {
    if (requiresConfirmation) {
        const auto response = QMessageBox::question(this, title, tr("Run git %1 in %2? This may change files, repository state, or a remote.").arg(arguments.join(QLatin1Char(' ')), repositoryRoot));
        if (response != QMessageBox::Yes) {
            return;
        }
    }
    gitService_->run(repositoryRoot, arguments, [this, title](const GitCommandResult &result) {
        const QString empty = title == tr("Diff") ? tr("No differences.") : title == tr("Show Log") ? tr("No commits found.") : QString();
        showGitOutput(title, result, empty);
    });
}

void MainWindow::showGitOutput(const QString &title, const GitCommandResult &result, const QString &emptyMessage) {
    if (!result.succeeded()) {
        const QString details = !result.startError.isEmpty() ? result.startError : result.standardError;
        QMessageBox::critical(this, title, tr("Git command failed.\n%1").arg(details.isEmpty() ? tr("No error output was returned.") : details));
        return;
    }
    QString output = result.standardOutput;
    if (output.trimmed().isEmpty() && !emptyMessage.isEmpty()) {
        output = emptyMessage;
    }
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(title);
    auto *layout = new QVBoxLayout(dialog);
    auto *text = new QTextEdit(dialog);
    text->setReadOnly(true);
    text->setPlainText(output);
    layout->addWidget(text);
    dialog->resize(720, 480);
    dialog->show();
}
```

Implement `switchGitBranch()`:

```cpp
void MainWindow::switchGitBranch(const QString &repositoryRoot) {
    gitService_->run(repositoryRoot, {QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)")}, [this, repositoryRoot](const GitCommandResult &result) {
        if (!result.succeeded()) {
            showGitOutput(tr("Switch Branch"), result);
            return;
        }
        const QStringList branches = result.standardOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        bool accepted = false;
        const QString branch = QInputDialog::getItem(this, tr("Switch Branch"), tr("Local branch:"), branches, 0, true, &accepted).trimmed();
        if (!accepted || branch.isEmpty()) {
            return;
        }
        runGitAction(tr("Switch Branch"), repositoryRoot, {QStringLiteral("switch"), branch}, true);
    });
}
```

- [ ] **Step 7: Run focused Git and UI tests**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure -R "GitServiceTest|MainWindowTest|FileBrowserWidgetTest"
```

Expected: all three test suites pass. The Git submenu appears only for detected worktrees, its title updates to `Git (modified)` for non-empty porcelain output, and its actions use the expected arguments.

- [ ] **Step 8: Commit Git context integration**

```bash
git add src/MainWindow.h src/MainWindow.cpp tests/test_mainwindow.cpp
```

## Task 6: Full Regression and Manual Verification

**Files:**
- Modify only files required to correct a reproducible failing test discovered in this task.

- [ ] **Step 1: Build and run the complete automated suite**

Run:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: every registered test passes, including `GitServiceTest`, `MainWindowTest`, and `FileBrowserWidgetTest`.

- [ ] **Step 2: Run the existing smoke test offscreen**

Run:

```bash
ctest --test-dir build --output-on-failure -R filemanager_smoke_test
```

Expected: `filemanager_smoke_test` passes without a GUI crash.

- [ ] **Step 3: Manually verify Git behavior using a disposable repository**

Run the application normally, then verify these scenarios in a temporary Git repository:

```bash
tmpdir=$(mktemp -d) && git -C "$tmpdir" init && git -C "$tmpdir" config user.email test@example.invalid && git -C "$tmpdir" config user.name Test && touch "$tmpdir/clean.txt" && git -C "$tmpdir" add clean.txt && git -C "$tmpdir" commit -m initial && printf 'changed\n' > "$tmpdir/clean.txt"
```

Expected manual results:

- Right-clicking inside the repository shows the Git submenu; right-clicking outside it does not.
- Opening the Git submenu after modification displays `Git (modified)` with a warning icon.
- Diff, Status, and Show Log open readable, selectable output dialogs.
- Pull, Push, Stash, Stash Pop, and Switch Branch show a confirmation; cancelling starts no command.
- A missing Git binary or a deliberately invalid operation surfaces an error dialog without changing the file browser path.

- [ ] **Step 4: Inspect the final working tree**

Run:

```bash
git status --short && git diff --check
```

Expected: only intentional source and test changes are present, and `git diff --check` emits no whitespace errors.

- [ ] **Step 5: Commit any final integration correction**

If and only if Steps 1-4 required a source/test correction not included in an earlier task, run:

```bash
git add CMakeLists.txt tests/CMakeLists.txt src/MainWindow.h src/MainWindow.cpp src/services/GitService.h src/services/GitService.cpp src/ui/FileBrowserWidget.h src/ui/FileBrowserWidget.cpp tests/test_filebrowser.cpp tests/test_git_service.cpp tests/test_mainwindow.cpp
```

## Self-Review

- Spec coverage: Task 1 implements Git discovery, asynchronous execution, status parsing, and test seams. Task 2 implements independent transient tab history. Task 3 implements the top toolbar and active-tab synchronization. Task 4 keeps Git menu requests separate from browsing UI. Task 5 implements all required Git actions, dirty marker, confirmation policy, output dialogs, and branch selection. Task 6 covers full regression and disposable-repository manual validation.
- Placeholder scan: every task includes concrete paths, API names, code, commands, expected outcomes, and commit commands. No task relies on implicit Git shell interpolation or automatic conflict recovery.
- Type consistency: `GitService::run()` consistently accepts a repository root, `QStringList` arguments, and `GitService::CommandCallback`; `GitCommandResult` is consistently used for output and failures. Toolbar code consistently calls `FileBrowserWidget::canGoBack()`, `goBack()`, `canGoForward()`, `goForward()`, `viewModeChanged()`, and `historyChanged()`.
