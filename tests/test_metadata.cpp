#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QStorageInfo>
#include <QTemporaryDir>

#include "models/FileMetadata.h"
#include "ui/MetadataPanel.h"

class MetadataTest : public QObject {
    Q_OBJECT

private slots:
    void extractsRegularFileMetadata();
    void extractsFolderMetadata();
    void extractsSymlinkMetadata();
    void handlesMissingFileWithoutFailing();
    void formatsSizePermissionsAndTimestamps();
    void metadataPanelUpdatesAndClearsValues();
};

void MetadataTest::extractsRegularFileMetadata() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = QDir(directory.path()).filePath("report.txt");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("hello"), qsizetype(5));
    file.close();

    const FileMetadata metadata = FileMetadata::fromPath(filePath);

    QCOMPARE(metadata.name, QString("report.txt"));
    QCOMPARE(metadata.path, filePath);
    QCOMPARE(metadata.type, QString("File"));
    QCOMPARE(metadata.size, qint64(5));
    QCOMPARE(metadata.extension, QString("txt"));
    QVERIFY(metadata.modified.isValid());
    QVERIFY(metadata.permissions != QFileDevice::Permissions());
    QVERIFY(metadata.symlinkTarget.isEmpty());
    QVERIFY(metadata.rootCapacity >= metadata.rootFreeSpace);
}

void MetadataTest::extractsFolderMetadata() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString childPath = QDir(directory.path()).filePath("child");
    QVERIFY(QDir(directory.path()).mkdir("child"));

    const FileMetadata metadata = FileMetadata::fromPath(childPath);

    QCOMPARE(metadata.name, QString("child"));
    QCOMPARE(metadata.type, QString("Folder"));
    QCOMPARE(metadata.extension, QString("Unavailable"));
    QVERIFY(metadata.modified.isValid());
}

void MetadataTest::extractsSymlinkMetadata() {
#if defined(Q_OS_UNIX)
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString targetPath = QDir(directory.path()).filePath("target.txt");
    QFile target(targetPath);
    QVERIFY(target.open(QIODevice::WriteOnly));
    target.write("target");
    target.close();
    const QString linkPath = QDir(directory.path()).filePath("link.txt");
    QVERIFY(QFile::link(targetPath, linkPath));

    const FileMetadata metadata = FileMetadata::fromPath(linkPath);

    QCOMPARE(metadata.type, QString("Symlink"));
    QCOMPARE(metadata.symlinkTarget, targetPath);
#else
    QSKIP("Symlink metadata test is only supported on Unix in this suite.");
#endif
}

void MetadataTest::handlesMissingFileWithoutFailing() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString missingPath = QDir(directory.path()).filePath("missing.bin");

    const FileMetadata metadata = FileMetadata::fromPath(missingPath);

    QCOMPARE(metadata.name, QString("missing.bin"));
    QCOMPARE(metadata.path, missingPath);
    QCOMPARE(metadata.type, QString("Unavailable"));
    QCOMPARE(metadata.displaySize(), QString("Unavailable"));
    QCOMPARE(metadata.displayModified(), QString("Unavailable"));
    QCOMPARE(metadata.displayPermissions(), QString("Unavailable"));
}

void MetadataTest::formatsSizePermissionsAndTimestamps() {
    FileMetadata metadata;
    metadata.size = 1536;
    metadata.permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                           QFileDevice::ReadGroup | QFileDevice::ReadOther;
    metadata.modified = QDateTime(QDate(2026, 7, 24), QTime(12, 30), QTimeZone::UTC);

    QCOMPARE(metadata.displaySize(), QString("1.5 KB"));
    QCOMPARE(metadata.displayPermissions(), QString("rwxr--r--"));
    QVERIFY(metadata.displayModified().contains("2026"));
}

void MetadataTest::metadataPanelUpdatesAndClearsValues() {
    MetadataPanel panel;
    FileMetadata metadata;
    metadata.name = "example.txt";
    metadata.path = "/tmp/example.txt";
    metadata.type = "File";
    metadata.size = 12;
    metadata.extension = "txt";

    panel.setMetadata(metadata);

    QCOMPARE(panel.displayedValue("Name"), QString("example.txt"));
    QCOMPARE(panel.displayedValue("Path"), QString("/tmp/example.txt"));
    QCOMPARE(panel.displayedValue("Size"), QString("12 B"));
    QCOMPARE(panel.displayedValue("Extension"), QString("txt"));

    panel.clear();
    QCOMPARE(panel.displayedValue("Name"), QString("Unavailable"));
    QCOMPARE(panel.displayedValue("Size"), QString("Unavailable"));
}

QTEST_MAIN(MetadataTest)
#include "test_metadata.moc"
