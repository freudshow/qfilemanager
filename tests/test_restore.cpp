#include <QtTest/QtTest>

#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>

#include "MainWindow.h"
#include "services/SearchService.h"
#include "services/SettingsStore.h"
#include "ui/FileBrowserWidget.h"
#include "ui/TabStrip.h"

class RestoreTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void restoresAndPersistsWindowTabsFavoritesAndSplitters();
    void searchServiceLimitsResultsToCurrentPath();
};

namespace {

void configureTestAppIdentity() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("FileManagerQt");
    QCoreApplication::setApplicationName("FileManager");
}

void removeTestSettings() {
    SettingsStore store;
    QFile::remove(store.settingsPath());
    QFile::remove(store.settingsPath() + ".bak");
}

QSplitter *findWorkspaceSplitter(MainWindow &window) {
    return window.centralWidget()->findChild<QSplitter *>("mainWorkspaceSplitter");
}

QTabWidget *findTabWidget(MainWindow &window) {
    auto *splitter = findWorkspaceSplitter(window);
    if (splitter == nullptr) {
        return nullptr;
    }
    auto *tabStrip = qobject_cast<TabStrip *>(splitter->widget(1));
    if (tabStrip == nullptr) {
        return nullptr;
    }
    return tabStrip->findChild<QTabWidget *>("tabPlaceholder");
}

} // namespace

void RestoreTest::init() {
    configureTestAppIdentity();
    removeTestSettings();
}

void RestoreTest::cleanup() {
    removeTestSettings();
}

void RestoreTest::restoresAndPersistsWindowTabsFavoritesAndSplitters() {
    QTemporaryDir first;
    QTemporaryDir second;
    QTemporaryDir favorite;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(favorite.isValid());

    AppSettings settings;
    settings.tabs.append({first.path(), "name", "ascending"});
    settings.tabs.append({second.path(), "name", "descending"});
    settings.favorites.append({"Favorite", favorite.path()});
    settings.splitterSizes = {450, 150, 300};
    settings.showHiddenFiles = true;
    SettingsStore store;
    QString error;
    QVERIFY2(store.save(settings, &error), qPrintable(error));

    {
        MainWindow window;
        auto *tabs = findTabWidget(window);
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(qobject_cast<FileBrowserWidget *>(tabs->widget(0))->currentPath(), first.path());
        QCOMPARE(qobject_cast<FileBrowserWidget *>(tabs->widget(1))->currentPath(), second.path());
        auto *splitter = findWorkspaceSplitter(window);
        QVERIFY(splitter != nullptr);
        splitter->setSizes({450, 150, 300});
        const QList<int> expectedSizes = splitter->sizes();
        qobject_cast<FileBrowserWidget *>(tabs->widget(1))->setCurrentPath(first.path());
        QCloseEvent closeEvent;
        QCoreApplication::sendEvent(&window, &closeEvent);

        AppSettings intermediate;
        QVERIFY2(store.load(intermediate, &error), qPrintable(error));
        QCOMPARE(intermediate.splitterSizes, QVector<int>(expectedSizes.begin(), expectedSizes.end()));
    }

    AppSettings saved;
    QVERIFY2(store.load(saved, &error), qPrintable(error));
    QCOMPARE(saved.tabs.size(), 2);
    QCOMPARE(saved.tabs[1].path, first.path());
    QCOMPARE(saved.favorites.size(), 1);
    QCOMPARE(saved.favorites[0].path, favorite.path());
    QCOMPARE(saved.splitterSizes.size(), 3);
    QVERIFY(saved.splitterSizes[0] > 0);
    QVERIFY(saved.splitterSizes[1] > 0);
    QVERIFY(saved.splitterSizes[2] > 0);
    QVERIFY(saved.showHiddenFiles);
}

void RestoreTest::searchServiceLimitsResultsToCurrentPath() {
    QTemporaryDir scoped;
    QTemporaryDir outside;
    QVERIFY(scoped.isValid());
    QVERIFY(outside.isValid());
    QFile scopedFile(QDir(scoped.path()).filePath("needle.txt"));
    QVERIFY(scopedFile.open(QIODevice::WriteOnly));
    scopedFile.write("inside");
    scopedFile.close();
    QFile outsideFile(QDir(outside.path()).filePath("needle.txt"));
    QVERIFY(outsideFile.open(QIODevice::WriteOnly));
    outsideFile.write("outside");
    outsideFile.close();

    SearchService service;
    QSignalSpy foundSpy(&service, &SearchService::resultFound);
    QSignalSpy finishedSpy(&service, &SearchService::searchFinished);

    const QStringList results = service.search(scoped.path(), "needle");

    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first(), QDir(scoped.path()).filePath("needle.txt"));
    QCOMPARE(foundSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
}

QTEST_MAIN(RestoreTest)
#include "test_restore.moc"
