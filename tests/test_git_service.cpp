#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "services/GitService.h"

class GitServiceTest : public QObject {
    Q_OBJECT

private slots:
    void findsRepositoryRootFromDirectory();
    void findsRepositoryRootFromFile();
    void findsWorktreeRepositoryRootFromGitFile();
    void returnsEmptyRootOutsideRepository();
    void detectsDirtyPorcelainOutput();
    void rejectsEmptyRepositoryRoot();
    void runsInjectedCommandWithRepositoryAndArguments();
};

namespace {

void createGitFileMarker(const QString &repositoryRoot) {
    const QString markerPath = QDir(repositoryRoot).filePath(QStringLiteral(".git"));
    QFile marker(markerPath);
    QVERIFY2(marker.open(QIODevice::WriteOnly), qPrintable(marker.errorString()));
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
    createGitFileMarker(repositoryRoot);

    GitService service;
    QCOMPARE(service.findRepositoryRoot(nestedDirectory), QFileInfo(repositoryRoot).absoluteFilePath());
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

    QVERIFY(called);
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

    QVERIFY(called);
    QCOMPARE(receivedRoot, repositoryRoot);
    QCOMPARE(receivedArguments, arguments);
    QVERIFY(result.succeeded());
    QCOMPARE(result.standardOutput, QStringLiteral("output"));
}

QTEST_MAIN(GitServiceTest)
#include "test_git_service.moc"
