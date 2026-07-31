#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "services/FileOperationService.h"
#include "services/PlatformServices.h"

#include <filesystem>

class FakePlatformServices : public PlatformServices {
public:
    explicit FakePlatformServices(bool trashResult)
        : trashResult_(trashResult) {
    }

    bool trashPaths(const QStringList &paths, QString *errorMessage = nullptr) override {
        trashedPaths = paths;
        if (!trashResult_) {
            if (errorMessage != nullptr) {
                *errorMessage = "trash failed";
            }
            return false;
        }
        return true;
    }

    QStringList trashedPaths;

private:
    bool trashResult_ = false;
};

class FileOperationServiceTest : public QObject {
    Q_OBJECT

private slots:
    void copiesFilesBetweenTemporaryDirectories();
    void createsEmptyTextFile();
    void copiesWithAutoRenameWhenTargetExists();
    void autoRenamePreservesExtensionlessFilesAndDirectories();
    void autoRenameRollsBackPartialDirectoryCopy();
    void rejectsInvalidAndExistingCreationTargets();
    void movesFilesBetweenTemporaryDirectories();
    void renamesAndCreatesFolders();
    void deletesToTrashViaPlatformService();
    void trashFailureLeavesSourceInPlace();
    void rejectsCopyingDirectoryIntoItsDescendant();
    void rejectsMovingDirectoryIntoItsDescendant();
    void copiesDirectorySymlinkWithoutFollowingCycle();
    void movingDirectorySymlinkAcrossFilesystemsDoesNotDeleteTarget();
    void copiesRelativeSymlinkWithoutRewritingTarget();
    void copiesAndMovesDanglingSymlinkAsEntry();
    void rejectsMoveWhenDestinationIsDanglingSymlink();
    void renamesDanglingSymlinkAsEntry();
};

namespace {

QString writeFile(const QString &path, const QByteArray &content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    file.write(content);
    return {};
}

QByteArray readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

void FileOperationServiceTest::copiesFilesBetweenTemporaryDirectories() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());
    const QString sourcePath = QDir(sourceDir.path()).filePath("document.txt");
    QVERIFY2(writeFile(sourcePath, "copy me").isEmpty(), "source file should be written");

    FileOperationService service;
    QString error;

    QVERIFY2(service.copy({sourcePath}, destinationDir.path(), &error), qPrintable(error));

    const QString copiedPath = QDir(destinationDir.path()).filePath("document.txt");
    QVERIFY(QFileInfo::exists(sourcePath));
    QCOMPARE(readFile(copiedPath), QByteArray("copy me"));
}

void FileOperationServiceTest::createsEmptyTextFile() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FileOperationService service;
    QString error;

    QVERIFY2(service.createTextFile(root.path(), "notes.txt", &error), qPrintable(error));

    const QString filePath = QDir(root.path()).filePath("notes.txt");
    QVERIFY(QFileInfo::exists(filePath));
    QVERIFY(QFileInfo(filePath).isFile());
    QCOMPARE(QFileInfo(filePath).size(), qint64(0));
}

void FileOperationServiceTest::copiesWithAutoRenameWhenTargetExists() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());
    const QString sourcePath = QDir(sourceDir.path()).filePath("document.txt");
    const QString existingPath = QDir(destinationDir.path()).filePath("document.txt");
    QVERIFY2(writeFile(sourcePath, "source").isEmpty(), "source file should be written");
    QVERIFY2(writeFile(existingPath, "existing").isEmpty(), "existing file should be written");

    FileOperationService service;
    QString error;
    QVERIFY2(service.copyWithAutoRename({sourcePath}, destinationDir.path(), &error), qPrintable(error));

    const QString copiedPath = QDir(destinationDir.path()).filePath("document (copy).txt");
    QCOMPARE(readFile(existingPath), QByteArray("existing"));
    QCOMPARE(readFile(copiedPath), QByteArray("source"));
}

void FileOperationServiceTest::autoRenamePreservesExtensionlessFilesAndDirectories() {
    QTemporaryDir sourceRoot;
    QTemporaryDir destinationRoot;
    QVERIFY(sourceRoot.isValid());
    QVERIFY(destinationRoot.isValid());
    const QString extensionlessSource = QDir(sourceRoot.path()).filePath("README");
    const QString extensionlessTarget = QDir(destinationRoot.path()).filePath("README");
    QVERIFY2(writeFile(extensionlessSource, "source").isEmpty(), "extensionless source should be written");
    QVERIFY2(writeFile(extensionlessTarget, "existing").isEmpty(), "extensionless target should be written");
    const QString sourceDirectory = QDir(sourceRoot.path()).filePath("assets");
    const QString targetDirectory = QDir(destinationRoot.path()).filePath("assets");
    QVERIFY(QDir().mkdir(sourceDirectory));
    QVERIFY(QDir().mkdir(targetDirectory));
    QVERIFY2(writeFile(QDir(sourceDirectory).filePath("icon.txt"), "icon").isEmpty(), "directory source should be populated");

    FileOperationService service;
    QString error;
    QVERIFY2(service.copyWithAutoRename({extensionlessSource, sourceDirectory}, destinationRoot.path(), &error), qPrintable(error));

    QCOMPARE(readFile(QDir(destinationRoot.path()).filePath("README (copy)")), QByteArray("source"));
    QVERIFY(QFileInfo(QDir(destinationRoot.path()).filePath("assets (copy)")).isDir());
    QCOMPARE(readFile(QDir(destinationRoot.path()).filePath("assets (copy)/icon.txt")), QByteArray("icon"));
}

void FileOperationServiceTest::autoRenameRollsBackPartialDirectoryCopy() {
#if !defined(Q_OS_UNIX)
    QSKIP("Permission-based partial-copy rollback is only deterministic on Unix.");
#else
    QTemporaryDir sourceRoot;
    QTemporaryDir destinationRoot;
    QVERIFY(sourceRoot.isValid());
    QVERIFY(destinationRoot.isValid());

    const QString sourceDirectory = QDir(sourceRoot.path()).filePath("assets");
    QVERIFY(QDir().mkdir(sourceDirectory));
    QVERIFY2(writeFile(QDir(sourceDirectory).filePath("a-readable.txt"), "readable").isEmpty(), "readable source should be written");
    const QString unreadablePath = QDir(sourceDirectory).filePath("b-unreadable.txt");
    QVERIFY2(writeFile(unreadablePath, "unreadable").isEmpty(), "unreadable source should be written");
    QFile unreadableFile(unreadablePath);
    QVERIFY(unreadableFile.setPermissions(QFileDevice::Permissions()));

    FileOperationService service;
    QString error;
    QVERIFY(!service.copyWithAutoRename({sourceDirectory}, destinationRoot.path(), &error));
    QVERIFY2(!QFileInfo::exists(QDir(destinationRoot.path()).filePath("assets")), "failed copy must not leave a partial target");
    QVERIFY2(!QFileInfo::exists(QDir(destinationRoot.path()).filePath("assets (copy)")), "failed auto-renamed copy must not leave a partial target");

    unreadableFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
}

void FileOperationServiceTest::rejectsInvalidAndExistingCreationTargets() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FileOperationService service;
    QString error;

    QVERIFY(!service.createFolder(root.path(), QStringLiteral("  "), &error));
    QVERIFY(error.contains(QStringLiteral("Invalid folder name")));
    QVERIFY(!service.createTextFile(root.path(), QStringLiteral("bad/name.txt"), &error));
    QVERIFY(error.contains(QStringLiteral("Invalid file name")));
    QVERIFY2(service.createTextFile(root.path(), QStringLiteral("notes.txt"), &error), qPrintable(error));
    QVERIFY(!service.createTextFile(root.path(), QStringLiteral("notes.txt"), &error));
    QVERIFY(error.contains(QStringLiteral("Could not create text file")));
    QVERIFY2(service.createFolder(root.path(), QStringLiteral("folder"), &error), qPrintable(error));
    QVERIFY(!service.createFolder(root.path(), QStringLiteral("folder"), &error));
    QVERIFY(error.contains(QStringLiteral("Could not create folder")));
}

void FileOperationServiceTest::movesFilesBetweenTemporaryDirectories() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());
    const QString sourcePath = QDir(sourceDir.path()).filePath("move.txt");
    QVERIFY2(writeFile(sourcePath, "move me").isEmpty(), "source file should be written");

    FileOperationService service;
    QString error;

    QVERIFY2(service.move({sourcePath}, destinationDir.path(), &error), qPrintable(error));

    QVERIFY(!QFileInfo::exists(sourcePath));
    QCOMPARE(readFile(QDir(destinationDir.path()).filePath("move.txt")), QByteArray("move me"));
}

void FileOperationServiceTest::renamesAndCreatesFolders() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    FileOperationService service;
    QString error;

    QVERIFY2(service.createFolder(root.path(), "New Folder", &error), qPrintable(error));
    const QString folderPath = QDir(root.path()).filePath("New Folder");
    QVERIFY(QFileInfo(folderPath).isDir());

    const QString filePath = QDir(root.path()).filePath("old.txt");
    QVERIFY2(writeFile(filePath, "rename me").isEmpty(), "source file should be written");
    QVERIFY2(service.renamePath(filePath, "new.txt", &error), qPrintable(error));

    QVERIFY(!QFileInfo::exists(filePath));
    QCOMPARE(readFile(QDir(root.path()).filePath("new.txt")), QByteArray("rename me"));
}

void FileOperationServiceTest::deletesToTrashViaPlatformService() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("delete.txt");
    QVERIFY2(writeFile(filePath, "trash me").isEmpty(), "source file should be written");
    FakePlatformServices platform(true);
    FileOperationService service(&platform);
    QString error;

    QVERIFY2(service.deleteToTrash({filePath}, &error), qPrintable(error));

    QCOMPARE(platform.trashedPaths, QStringList{filePath});
}

void FileOperationServiceTest::trashFailureLeavesSourceInPlace() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString filePath = QDir(root.path()).filePath("keep.txt");
    QVERIFY2(writeFile(filePath, "keep me").isEmpty(), "source file should be written");
    FakePlatformServices platform(false);
    FileOperationService service(&platform);
    QString error;

    QVERIFY(!service.deleteToTrash({filePath}, &error));

    QCOMPARE(error, QString("trash failed"));
    QVERIFY(QFileInfo::exists(filePath));
    QCOMPARE(readFile(filePath), QByteArray("keep me"));
}

void FileOperationServiceTest::rejectsCopyingDirectoryIntoItsDescendant() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QDir rootDir(root.path());
    QVERIFY(rootDir.mkdir("source"));
    const QString sourcePath = rootDir.filePath("source");
    QVERIFY(QDir(sourcePath).mkdir("child"));
    const QString childPath = QDir(sourcePath).filePath("child");
    QVERIFY2(writeFile(QDir(sourcePath).filePath("file.txt"), "content").isEmpty(), "source file should be written");

    FileOperationService service;
    QString error;

    QVERIFY(!service.copy({sourcePath}, childPath, &error));

    QVERIFY(error.contains("inside itself"));
    QVERIFY(QFileInfo::exists(QDir(sourcePath).filePath("file.txt")));
    QVERIFY(!QFileInfo::exists(QDir(childPath).filePath("source")));
}

void FileOperationServiceTest::rejectsMovingDirectoryIntoItsDescendant() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QDir rootDir(root.path());
    QVERIFY(rootDir.mkdir("source"));
    const QString sourcePath = rootDir.filePath("source");
    QVERIFY(QDir(sourcePath).mkdir("child"));
    const QString childPath = QDir(sourcePath).filePath("child");
    QVERIFY2(writeFile(QDir(sourcePath).filePath("file.txt"), "content").isEmpty(), "source file should be written");

    FileOperationService service;
    QString error;

    QVERIFY(!service.move({sourcePath}, childPath, &error));

    QVERIFY(error.contains("inside itself"));
    QVERIFY(QFileInfo::exists(QDir(sourcePath).filePath("file.txt")));
    QVERIFY(!QFileInfo::exists(QDir(childPath).filePath("source")));
}

void FileOperationServiceTest::copiesDirectorySymlinkWithoutFollowingCycle() {
#if defined(Q_OS_UNIX)
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QDir rootDir(root.path());
    QVERIFY(rootDir.mkdir("source"));
    QVERIFY(rootDir.mkdir("dest"));
    const QString sourcePath = rootDir.filePath("source");
    const QString destPath = rootDir.filePath("dest");
    QVERIFY2(writeFile(QDir(sourcePath).filePath("file.txt"), "content").isEmpty(), "source file should be written");
    const QString loopPath = QDir(sourcePath).filePath("loop");
    QVERIFY(QFile::link(sourcePath, loopPath));

    FileOperationService service;
    QString error;

    QVERIFY2(service.copy({sourcePath}, destPath, &error), qPrintable(error));

    const QString copiedLoop = QDir(destPath).filePath("source/loop");
    QVERIFY(QFileInfo(copiedLoop).isSymLink());
    const QFileInfoList copiedEntries = QDir(QDir(destPath).filePath("source")).entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    QCOMPARE(copiedEntries.size(), 2);
#else
    QSKIP("Symlink cycle copy test is only supported on Unix in this suite.");
#endif
}

void FileOperationServiceTest::movingDirectorySymlinkAcrossFilesystemsDoesNotDeleteTarget() {
#if defined(Q_OS_UNIX)
    if (!QFileInfo::exists("/dev/shm") || !QFileInfo("/dev/shm").isWritable()) {
        QSKIP("/dev/shm is required to exercise cross-filesystem symlink move fallback.");
    }

    QTemporaryDir sourceRoot;
    QTemporaryDir destinationRoot("/dev/shm/filemanager_move_symlink_XXXXXX");
    QVERIFY(sourceRoot.isValid());
    QVERIFY(destinationRoot.isValid());
    QDir sourceDir(sourceRoot.path());
    QVERIFY(sourceDir.mkdir("targetdir"));
    const QString targetDir = sourceDir.filePath("targetdir");
    const QString payloadPath = QDir(targetDir).filePath("payload.txt");
    QVERIFY2(writeFile(payloadPath, "keep").isEmpty(), "payload file should be written");
    const QString linkPath = sourceDir.filePath("linkdir");
    QVERIFY(QFile::link(targetDir, linkPath));
    QVERIFY(QFileInfo(linkPath).isSymLink());

    FileOperationService service;
    QString error;

    QVERIFY2(service.move({linkPath}, destinationRoot.path(), &error), qPrintable(error));

    QVERIFY(QFileInfo::exists(payloadPath));
    QVERIFY(!QFileInfo::exists(linkPath));
    QVERIFY(QFileInfo(QDir(destinationRoot.path()).filePath("linkdir")).isSymLink());
#else
    QSKIP("Cross-filesystem symlink move test is only supported on Unix in this suite.");
#endif
}

void FileOperationServiceTest::copiesRelativeSymlinkWithoutRewritingTarget() {
#if defined(Q_OS_UNIX)
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QDir rootDir(root.path());
    QVERIFY(rootDir.mkdir("source"));
    QVERIFY(rootDir.mkdir("dest"));
    const QString sourcePath = rootDir.filePath("source");
    const QString destPath = rootDir.filePath("dest");
    QVERIFY2(writeFile(QDir(sourcePath).filePath("target.txt"), "content").isEmpty(), "target file should be written");
    const QString linkPath = QDir(sourcePath).filePath("relative-link.txt");
    QVERIFY(QFile::link("target.txt", linkPath));

    FileOperationService service;
    QString error;

    QVERIFY2(service.copy({sourcePath}, destPath, &error), qPrintable(error));

    const QString copiedLinkPath = QDir(destPath).filePath("source/relative-link.txt");
    QVERIFY(QFileInfo(copiedLinkPath).isSymLink());
    QCOMPARE(QString::fromStdString(std::filesystem::read_symlink(copiedLinkPath.toStdString()).generic_string()), QString("target.txt"));
    QCOMPARE(readFile(copiedLinkPath), QByteArray("content"));
#else
    QSKIP("Relative symlink copy test is only supported on Unix in this suite.");
#endif
}

void FileOperationServiceTest::copiesAndMovesDanglingSymlinkAsEntry() {
#if defined(Q_OS_UNIX)
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QDir rootDir(root.path());
    QVERIFY(rootDir.mkdir("source"));
    QVERIFY(rootDir.mkdir("copyDest"));
    QVERIFY(rootDir.mkdir("moveDest"));
    const QString sourceDir = rootDir.filePath("source");
    const QString linkPath = QDir(sourceDir).filePath("dangling-link");
    QVERIFY(QFile::link("missing-target", linkPath));
    QVERIFY(QFileInfo(linkPath).isSymLink());
    QVERIFY(!QFileInfo::exists(linkPath));

    FileOperationService service;
    QString error;

    QVERIFY2(service.copy({linkPath}, rootDir.filePath("copyDest"), &error), qPrintable(error));
    const QString copiedLink = QDir(rootDir.filePath("copyDest")).filePath("dangling-link");
    QVERIFY(QFileInfo(copiedLink).isSymLink());
    QVERIFY(!QFileInfo::exists(copiedLink));

    QVERIFY2(service.move({linkPath}, rootDir.filePath("moveDest"), &error), qPrintable(error));
    const QString movedLink = QDir(rootDir.filePath("moveDest")).filePath("dangling-link");
    QVERIFY(!QFileInfo(linkPath).isSymLink());
    QVERIFY(QFileInfo(movedLink).isSymLink());
    QVERIFY(!QFileInfo::exists(movedLink));
#else
    QSKIP("Dangling symlink operation test is only supported on Unix in this suite.");
#endif
}

void FileOperationServiceTest::rejectsMoveWhenDestinationIsDanglingSymlink() {
#if defined(Q_OS_UNIX)
    QTemporaryDir sourceRoot;
    QTemporaryDir destinationRoot;
    QVERIFY(sourceRoot.isValid());
    QVERIFY(destinationRoot.isValid());
    const QString sourcePath = QDir(sourceRoot.path()).filePath("conflict");
    QVERIFY2(writeFile(sourcePath, "source").isEmpty(), "source file should be written");
    const QString destinationLink = QDir(destinationRoot.path()).filePath("conflict");
    QVERIFY(QFile::link("missing-target", destinationLink));
    QVERIFY(QFileInfo(destinationLink).isSymLink());
    QVERIFY(!QFileInfo::exists(destinationLink));

    FileOperationService service;
    QString error;

    QVERIFY(!service.move({sourcePath}, destinationRoot.path(), &error));

    QVERIFY(error.contains("already exists"));
    QVERIFY(QFileInfo::exists(sourcePath));
    QVERIFY(QFileInfo(destinationLink).isSymLink());
#else
    QSKIP("Dangling symlink destination test is only supported on Unix in this suite.");
#endif
}

void FileOperationServiceTest::renamesDanglingSymlinkAsEntry() {
#if defined(Q_OS_UNIX)
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString linkPath = QDir(root.path()).filePath("dangling-link");
    QVERIFY(QFile::link("missing-target", linkPath));
    QVERIFY(QFileInfo(linkPath).isSymLink());
    QVERIFY(!QFileInfo::exists(linkPath));

    FileOperationService service;
    QString error;

    QVERIFY2(service.renamePath(linkPath, "renamed-link", &error), qPrintable(error));

    const QString renamedPath = QDir(root.path()).filePath("renamed-link");
    QVERIFY(!QFileInfo(linkPath).isSymLink());
    QVERIFY(QFileInfo(renamedPath).isSymLink());
    QVERIFY(!QFileInfo::exists(renamedPath));
#else
    QSKIP("Dangling symlink rename test is only supported on Unix in this suite.");
#endif
}

QTEST_MAIN(FileOperationServiceTest)
#include "test_file_operations.moc"
