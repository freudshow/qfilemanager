#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "services/GitService.h"

class GitServiceTest : public QObject {
    Q_OBJECT

private slots:
    void findsRepositoryRootFromDirectory();
    void findsRepositoryRootFromFile();
    void findsWorktreeRepositoryRootFromGitFile();
    void rejectsEmptyGitFileMarker();
    void rejectsMalformedGitFileMarker();
    void rejectsDanglingGitFileMarker();
    void returnsEmptyRootOutsideRepository();
    void detectsDirtyPorcelainOutput();
    void rejectsEmptyRepositoryRoot();
    void runsInjectedCommandWithRepositoryAndArguments();
    void deliversInjectedRunnerCallbackOnlyOnce();
    void runsGitVersionAsynchronously();
};

namespace {

void createGitFileMarker(const QString &repositoryRoot, const QString &contents) {
    const QString markerPath = QDir(repositoryRoot).filePath(QStringLiteral(".git"));
    QFile marker(markerPath);
    QVERIFY2(marker.open(QIODevice::WriteOnly), qPrintable(marker.errorString()));
    QCOMPARE(marker.write(contents.toUtf8()), contents.toUtf8().size());
    marker.close();
}

void createGitDirectoryMarker(const QString &repositoryRoot) {
    QVERIFY(QDir().mkpath(QDir(repositoryRoot).filePath(QStringLiteral(".git"))));
}

} // namespace

void GitServiceTest::findsRepositoryRootFromDirectory() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("repository"));
    const QString nestedDirectory = QDir(repositoryRoot).filePath(QStringLiteral("src/nested"));
    QVERIFY(QDir().mkpath(nestedDirectory));
    createGitDirectoryMarker(repositoryRoot);

    GitService service;
    QCOMPARE(service.findRepositoryRoot(nestedDirectory), QFileInfo(repositoryRoot).absoluteFilePath());
}

void GitServiceTest::findsRepositoryRootFromFile() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("repository"));
    const QString sourceDirectory = QDir(repositoryRoot).filePath(QStringLiteral("src"));
    QVERIFY(QDir().mkpath(sourceDirectory));
    createGitDirectoryMarker(repositoryRoot);
    const QString sourceFile = QDir(sourceDirectory).filePath(QStringLiteral("main.cpp"));
    QFile file(sourceFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    GitService service;
    QCOMPARE(service.findRepositoryRoot(sourceFile), QFileInfo(repositoryRoot).absoluteFilePath());
}

void GitServiceTest::findsWorktreeRepositoryRootFromGitFile() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("worktree"));
    const QString nestedDirectory = QDir(repositoryRoot).filePath(QStringLiteral("deep/path"));
    QVERIFY(QDir().mkpath(nestedDirectory));
    const QString gitDirectory = QDir(repositoryRoot).filePath(QStringLiteral("metadata"));
    QVERIFY(QDir().mkpath(gitDirectory));
    createGitFileMarker(repositoryRoot, QStringLiteral("gitdir: metadata\n"));

    GitService service;
    QCOMPARE(service.findRepositoryRoot(nestedDirectory), QFileInfo(repositoryRoot).absoluteFilePath());
}

void GitServiceTest::rejectsEmptyGitFileMarker() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("worktree"));
    QVERIFY(QDir().mkpath(repositoryRoot));
    createGitFileMarker(repositoryRoot, {});

    GitService service;
    QVERIFY(service.findRepositoryRoot(repositoryRoot).isEmpty());
}

void GitServiceTest::rejectsMalformedGitFileMarker() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("worktree"));
    QVERIFY(QDir().mkpath(repositoryRoot));
    QVERIFY(QDir().mkpath(QDir(repositoryRoot).filePath(QStringLiteral("metadata"))));
    createGitFileMarker(repositoryRoot, QStringLiteral("gitdir:metadata\n"));

    GitService service;
    QVERIFY(service.findRepositoryRoot(repositoryRoot).isEmpty());
}

void GitServiceTest::rejectsDanglingGitFileMarker() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString repositoryRoot = QDir(temporaryDirectory.path()).filePath(QStringLiteral("worktree"));
    QVERIFY(QDir().mkpath(repositoryRoot));
    createGitFileMarker(repositoryRoot, QStringLiteral("gitdir: missing\n"));

    GitService service;
    QVERIFY(service.findRepositoryRoot(repositoryRoot).isEmpty());
}

void GitServiceTest::returnsEmptyRootOutsideRepository() {
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString nestedDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("plain/nested"));
    QVERIFY(QDir().mkpath(nestedDirectory));

    GitService service;
    QVERIFY(service.findRepositoryRoot(nestedDirectory).isEmpty());
}

void GitServiceTest::detectsDirtyPorcelainOutput() {
    QVERIFY(!GitService::isDirtyPorcelainOutput(QStringLiteral(" \n\t ")));
    QVERIFY(GitService::isDirtyPorcelainOutput(QStringLiteral(" M src/main.cpp\n")));
}

void GitServiceTest::rejectsEmptyRepositoryRoot() {
    GitService service;
    GitCommandResult result;
    bool called = false;

    service.run({}, {}, [&](const GitCommandResult &commandResult) {
        called = true;
        result = commandResult;
    });

    QVERIFY(!called);
    QTRY_VERIFY(called);
    QVERIFY(!result.started);
    QVERIFY(!result.succeeded());
    QVERIFY(!result.startError.isEmpty());
}

void GitServiceTest::runsInjectedCommandWithRepositoryAndArguments() {
    GitService service;
    QString receivedRoot;
    QStringList receivedArguments;
    service.setCommandRunner([&](const QString &workingDirectory, const QStringList &arguments, GitService::CommandCallback callback) {
        receivedRoot = workingDirectory;
        receivedArguments = arguments;
        callback({true, 0, QStringLiteral("output"), QString(), QString()});
    });

    GitCommandResult result;
    bool called = false;
    const QString repositoryRoot = QDir::cleanPath(QDir::currentPath());
    const QStringList arguments{QStringLiteral("status"), QStringLiteral("--porcelain")};
    service.run(repositoryRoot, arguments, [&](const GitCommandResult &commandResult) {
        called = true;
        result = commandResult;
    });

    QVERIFY(!called);
    QTRY_VERIFY(called);
    QCOMPARE(receivedRoot, repositoryRoot);
    QCOMPARE(receivedArguments, arguments);
    QVERIFY(result.succeeded());
    QCOMPARE(result.standardOutput, QStringLiteral("output"));
}

void GitServiceTest::deliversInjectedRunnerCallbackOnlyOnce() {
    GitService service;
    service.setCommandRunner([](const QString &, const QStringList &, GitService::CommandCallback callback) {
        callback({true, 0, QStringLiteral("first"), {}, {}});
        callback({true, 0, QStringLiteral("second"), {}, {}});
    });

    int callbackCount = 0;
    GitCommandResult result;
    service.run(QDir::currentPath(), {}, [&](const GitCommandResult &commandResult) {
        ++callbackCount;
        result = commandResult;
    });

    QVERIFY(callbackCount == 0);
    QTRY_COMPARE(callbackCount, 1);
    QCOMPARE(result.standardOutput, QStringLiteral("first"));
}

void GitServiceTest::runsGitVersionAsynchronously() {
    if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty()) {
        QSKIP("Git executable is unavailable.");
    }

    GitService service;
    service.setCommandTimeout(5000);
    bool called = false;
    GitCommandResult result;
    service.run(QDir::currentPath(), {QStringLiteral("--version")}, [&](const GitCommandResult &commandResult) {
        called = true;
        result = commandResult;
    });

    QVERIFY(!called);
    QTRY_VERIFY_WITH_TIMEOUT(called, 5000);
    QVERIFY2(result.succeeded(), qPrintable(result.startError + result.standardError));
    QVERIFY(result.standardOutput.contains(QStringLiteral("git version")));
}

QTEST_MAIN(GitServiceTest)
#include "test_git_service.moc"
