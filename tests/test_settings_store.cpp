#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "services/SettingsStore.h"

class SettingsStoreTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void roundTripPreservesTabsFavoritesOptionsAndSplitters();
    void savesAndLoadsSelectedTheme();
    void legacySettingsDefaultToAuroraTheme();
    void invalidJsonCreatesBackupAndFallsBack();
    void invalidTopLevelJsonCreatesBackupAndFallsBack();
    void malformedFieldValuesCreateBackupAndFallback_data();
    void malformedFieldValuesCreateBackupAndFallback();
    void savesAndLoadsOpenWithDefaults();
    void rejectsInvalidOpenWithDefaults();
};

namespace {

void writeSettingsFile(const QString &path, const QByteArray &contents) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), contents.size());
    file.close();
}

} // namespace

void SettingsStoreTest::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("FileManagerQtTests");
    QCoreApplication::setApplicationName("SettingsStoreTest");
}

void SettingsStoreTest::roundTripPreservesTabsFavoritesOptionsAndSplitters() {
    SettingsStore store;
    QFile::remove(store.settingsPath());
    QFile::remove(store.settingsPath() + ".bak");

    AppSettings settings;
    settings.version = 1;
    settings.windowGeometry = QByteArray("geometry-data");
    settings.windowState = QByteArray("state-data");
    settings.splitterSizes = {180, 640, 260};
    settings.tabs = {{"/tmp/projects", "name", "ascending"}, {"/tmp/downloads", "modified", "descending"}};
    settings.favorites = {{"Projects", "/tmp/projects"}, {"Downloads", "/tmp/downloads"}};
    settings.showHiddenFiles = true;
    settings.confirmDeleteToTrash = false;

    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    AppSettings loaded;
    QVERIFY2(store.load(loaded, &error), qPrintable(error));

    QCOMPARE(loaded.version, 1);
    QCOMPARE(loaded.windowGeometry, QByteArray("geometry-data"));
    QCOMPARE(loaded.windowState, QByteArray("state-data"));
    QCOMPARE(loaded.splitterSizes, QVector<int>({180, 640, 260}));
    QCOMPARE(loaded.tabs.size(), 2);
    QCOMPARE(loaded.tabs[0].path, QString("/tmp/projects"));
    QCOMPARE(loaded.tabs[0].sortColumn, QString("name"));
    QCOMPARE(loaded.tabs[0].sortOrder, QString("ascending"));
    QCOMPARE(loaded.tabs[1].path, QString("/tmp/downloads"));
    QCOMPARE(loaded.tabs[1].sortColumn, QString("modified"));
    QCOMPARE(loaded.tabs[1].sortOrder, QString("descending"));
    QCOMPARE(loaded.favorites.size(), 2);
    QCOMPARE(loaded.favorites[0].name, QString("Projects"));
    QCOMPARE(loaded.favorites[0].path, QString("/tmp/projects"));
    QCOMPARE(loaded.favorites[1].name, QString("Downloads"));
    QCOMPARE(loaded.favorites[1].path, QString("/tmp/downloads"));
    QVERIFY(loaded.showHiddenFiles);
    QVERIFY(!loaded.confirmDeleteToTrash);
}

void SettingsStoreTest::savesAndLoadsSelectedTheme() {
    SettingsStore store;
    QFile::remove(store.settingsPath());
    QFile::remove(store.settingsPath() + ".bak");

    AppSettings settings;
    settings.theme = QStringLiteral("graphite");
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    AppSettings loaded;
    QVERIFY2(store.load(loaded, &error), qPrintable(error));
    QCOMPARE(loaded.theme, QStringLiteral("graphite"));
}

void SettingsStoreTest::legacySettingsDefaultToAuroraTheme() {
    SettingsStore store;
    QFile::remove(store.settingsPath());
    QFile::remove(store.settingsPath() + ".bak");
    writeSettingsFile(store.settingsPath(), QByteArray(R"({"version":1})"));

    AppSettings loaded;
    QString error;
    QVERIFY2(store.load(loaded, &error), qPrintable(error));
    QCOMPARE(loaded.theme, QStringLiteral("aurora"));
}

void SettingsStoreTest::invalidJsonCreatesBackupAndFallsBack() {
    SettingsStore store;
    QFile::remove(store.settingsPath());
    QFile::remove(store.settingsPath() + ".bak");
    const QByteArray invalidJson("{ invalid json");
    writeSettingsFile(store.settingsPath(), invalidJson);

    AppSettings loaded;
    loaded.showHiddenFiles = true;
    loaded.confirmDeleteToTrash = false;
    QString error;
    QVERIFY(!store.load(loaded, &error));
    QVERIFY2(!error.isEmpty(), "Invalid settings should produce an error message");
    QVERIFY(!QFile::exists(store.settingsPath()));
    QVERIFY(QFile::exists(store.settingsPath() + ".bak"));
    QCOMPARE(loaded.version, 1);
    QVERIFY(loaded.tabs.isEmpty());
    QVERIFY(loaded.favorites.isEmpty());
    QVERIFY(!loaded.showHiddenFiles);
    QVERIFY(loaded.confirmDeleteToTrash);
}

void SettingsStoreTest::invalidTopLevelJsonCreatesBackupAndFallsBack() {
    SettingsStore store;
    QFile::remove(store.settingsPath());
    QFile::remove(store.settingsPath() + ".bak");
    const QByteArray invalidTopLevelJson("[]");
    writeSettingsFile(store.settingsPath(), invalidTopLevelJson);

    AppSettings loaded;
    loaded.tabs = {{"/tmp/previous", "name", "ascending"}};
    QString error;
    QVERIFY(!store.load(loaded, &error));
    QVERIFY2(error.contains("top-level"), qPrintable(error));
    QVERIFY(!QFile::exists(store.settingsPath()));
    QVERIFY(QFile::exists(store.settingsPath() + ".bak"));
    QVERIFY(loaded.tabs.isEmpty());
    QVERIFY(loaded.favorites.isEmpty());
    QVERIFY(!loaded.showHiddenFiles);
    QVERIFY(loaded.confirmDeleteToTrash);
}

void SettingsStoreTest::malformedFieldValuesCreateBackupAndFallback_data() {
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("invalid window geometry base64") << QByteArray(R"({"window":{"geometry":"not base64!"}})") << QString("window.geometry");
    QTest::newRow("invalid window state base64") << QByteArray(R"({"window":{"state":"not base64!"}})") << QString("window.state");
    QTest::newRow("fractional splitter size") << QByteArray(R"({"window":{"splitters":[12.5]}})") << QString("splitter");
    QTest::newRow("negative splitter size") << QByteArray(R"({"window":{"splitters":[-1]}})") << QString("splitter");
    QTest::newRow("out of range splitter size") << QByteArray(R"({"window":{"splitters":[2147483648]}})") << QString("splitter");
    QTest::newRow("tab sortColumn not string") << QByteArray(R"({"tabs":[{"path":"/tmp","sortColumn":1}]})") << QString("sortColumn");
    QTest::newRow("tab sortOrder not string") << QByteArray(R"({"tabs":[{"path":"/tmp","sortOrder":false}]})") << QString("sortOrder");
    QTest::newRow("favorite name not string") << QByteArray(R"({"favorites":[{"name":false,"path":"/tmp"}]})") << QString("favorite");
    QTest::newRow("favorite path not string") << QByteArray(R"({"favorites":[{"name":"Tmp","path":42}]})") << QString("favorite");
    QTest::newRow("option showHiddenFiles not boolean") << QByteArray(R"({"options":{"showHiddenFiles":"true"}})") << QString("showHiddenFiles");
    QTest::newRow("option confirmDeleteToTrash not boolean") << QByteArray(R"({"options":{"confirmDeleteToTrash":1}})") << QString("confirmDeleteToTrash");
    QTest::newRow("fractional version") << QByteArray(R"({"version":1.5})") << QString("version");
    QTest::newRow("out of range version") << QByteArray(R"({"version":2147483648})") << QString("version");
    QTest::newRow("negative version") << QByteArray(R"({"version":-1})") << QString("version");
}

void SettingsStoreTest::malformedFieldValuesCreateBackupAndFallback() {
    QFETCH(QByteArray, json);
    QFETCH(QString, expectedError);

    SettingsStore store;
    QFile::remove(store.settingsPath());
    QFile::remove(store.settingsPath() + ".bak");
    writeSettingsFile(store.settingsPath(), json);

    AppSettings loaded;
    loaded.version = 99;
    loaded.windowGeometry = QByteArray("previous geometry");
    loaded.windowState = QByteArray("previous state");
    loaded.splitterSizes = {1, 2, 3};
    loaded.tabs = {{"/tmp/previous", "name", "ascending"}};
    loaded.favorites = {{"Previous", "/tmp/previous"}};
    loaded.showHiddenFiles = true;
    loaded.confirmDeleteToTrash = false;

    QString error;
    QVERIFY(!store.load(loaded, &error));
    QVERIFY2(error.contains(expectedError), qPrintable(error));
    QVERIFY(!QFile::exists(store.settingsPath()));
    QVERIFY(QFile::exists(store.settingsPath() + ".bak"));
    QCOMPARE(loaded.version, 1);
    QVERIFY(loaded.windowGeometry.isEmpty());
    QVERIFY(loaded.windowState.isEmpty());
    QVERIFY(loaded.splitterSizes.isEmpty());
    QVERIFY(loaded.tabs.isEmpty());
    QVERIFY(loaded.favorites.isEmpty());
    QVERIFY(!loaded.showHiddenFiles);
    QVERIFY(loaded.confirmDeleteToTrash);
}

void SettingsStoreTest::savesAndLoadsOpenWithDefaults() {
    QTemporaryDir configDir;
    QVERIFY(configDir.isValid());
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());

    AppSettings settings;
    settings.openWithDefaults.insert(QStringLiteral(".txt"), QStringLiteral("/usr/bin/kate"));
    settings.openWithDefaults.insert(QStringLiteral(".png"), QStringLiteral("C:/Program Files/Viewer/viewer.exe"));

    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    AppSettings loaded;
    QVERIFY2(store.load(loaded, &error), qPrintable(error));
    QCOMPARE(loaded.openWithDefaults.value(QStringLiteral(".txt")), QStringLiteral("/usr/bin/kate"));
    QCOMPARE(loaded.openWithDefaults.value(QStringLiteral(".png")), QStringLiteral("C:/Program Files/Viewer/viewer.exe"));
}

void SettingsStoreTest::rejectsInvalidOpenWithDefaults() {
    QTemporaryDir configDir;
    QVERIFY(configDir.isValid());
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());

    SettingsStore store;
    QFile settingsFile(store.settingsPath());
    QVERIFY(QDir().mkpath(QFileInfo(settingsFile).absolutePath()));
    QVERIFY(settingsFile.open(QIODevice::WriteOnly));
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("openWithDefaults"), QJsonObject{{QStringLiteral(".txt"), 42}}}};
    settingsFile.write(QJsonDocument(root).toJson());
    settingsFile.close();

    AppSettings loaded;
    QString error;
    QVERIFY(!store.load(loaded, &error));
    QVERIFY2(error.contains(QStringLiteral("openWithDefaults")), qPrintable(error));
}

QTEST_MAIN(SettingsStoreTest)
#include "test_settings_store.moc"
