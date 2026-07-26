#include <QtTest/QtTest>

#include <QDir>
#include <QListView>
#include <QSignalSpy>

#include "models/FavoritesModel.h"
#include "ui/FavoritesSidebar.h"

class FavoritesSidebarTest : public QObject {
    Q_OBJECT

private slots:
    void singleClickActivatesAvailableFavorite();
    void singleClickDoesNotActivateMissingFavorite();
    void doesNotActivateUnavailableFavorite();
    void requestsCurrentFolderFavoriteFromContextMenuAction();
};

void FavoritesSidebarTest::singleClickActivatesAvailableFavorite() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FavoritesModel model;
    model.addFavorite(QStringLiteral("Project"), directory.path());

    FavoritesSidebar sidebar;
    sidebar.setModel(&model);
    sidebar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sidebar));

    QSignalSpy activatedSpy(&sidebar, &FavoritesSidebar::favoriteActivated);
    const QModelIndex index = model.index(0, 0);
    const QRect rect = sidebar.listView()->visualRect(index);
    QVERIFY(rect.isValid());

    QTest::mouseClick(sidebar.listView()->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(activatedSpy.takeFirst().at(0).toString(), directory.path());
}

void FavoritesSidebarTest::singleClickDoesNotActivateMissingFavorite() {
    FavoritesModel model;
    model.addFavorite(QStringLiteral("Missing"), QStringLiteral("/path/that/does/not/exist"));

    FavoritesSidebar sidebar;
    sidebar.setModel(&model);
    sidebar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sidebar));

    QSignalSpy activatedSpy(&sidebar, &FavoritesSidebar::favoriteActivated);
    const QModelIndex index = model.index(0, 0);
    const QRect rect = sidebar.listView()->visualRect(index);
    QVERIFY(rect.isValid());

    QTest::mouseClick(sidebar.listView()->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

    QCOMPARE(activatedSpy.count(), 0);
}

void FavoritesSidebarTest::doesNotActivateUnavailableFavorite() {
    const QString missingPath = QDir::cleanPath(QDir::temp().filePath("filemanager-qt-missing-sidebar-favorite-path"));
    QVERIFY(!QFileInfo::exists(missingPath));

    FavoritesModel model;
    QVERIFY(model.addFavorite("Missing", missingPath));

    FavoritesSidebar sidebar;
    sidebar.setModel(&model);
    QSignalSpy activatedSpy(&sidebar, &FavoritesSidebar::favoriteActivated);

    emit sidebar.listView()->activated(model.index(0, 0));

    QCOMPARE(activatedSpy.count(), 0);
}

void FavoritesSidebarTest::requestsCurrentFolderFavoriteFromContextMenuAction() {
    FavoritesModel model;
    FavoritesSidebar sidebar;
    sidebar.setModel(&model);
    QSignalSpy addSpy(&sidebar, &FavoritesSidebar::addCurrentFolderRequested);

    QVERIFY(sidebar.addCurrentFolderAction() != nullptr);
    sidebar.addCurrentFolderAction()->trigger();

    QCOMPARE(addSpy.count(), 1);
}

QTEST_MAIN(FavoritesSidebarTest)
#include "test_favorites_sidebar.moc"
