#include <QtTest/QtTest>

#include <QDir>
#include <QListView>
#include <QSignalSpy>

#include "models/FavoritesModel.h"
#include "ui/FavoritesSidebar.h"

class FavoritesSidebarTest : public QObject {
    Q_OBJECT

private slots:
    void doesNotActivateUnavailableFavorite();
};

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

QTEST_MAIN(FavoritesSidebarTest)
#include "test_favorites_sidebar.moc"
