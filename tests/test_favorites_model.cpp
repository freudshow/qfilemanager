#include <QtTest/QtTest>

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "models/FavoritesModel.h"

class FavoritesModelTest : public QObject {
    Q_OBJECT

private slots:
    void addRemoveAndExposeFavorites();
    void rejectsBlankNamesAndStoresTrimmedNames();
    void setFavoritesRejectsBlankNamesAndStoresTrimmedNames();
    void rejectsDuplicatePathsAfterNormalization();
    void missingPathsRemainVisibleButUnavailable();
    void exposesModelRoles();
    void invalidRemoveReturnsFalseWithoutChangingModel();
};

void FavoritesModelTest::addRemoveAndExposeFavorites() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FavoritesModel model;
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);

    QVERIFY(model.addFavorite("Project", directory.path()));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(model.favoriteAt(0).name, QString("Project"));
    QCOMPARE(model.favoriteAt(0).path, QDir::cleanPath(directory.path()));
    QCOMPARE(model.favorites().size(), 1);

    QVERIFY(model.removeFavorite(0));
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(removeSpy.count(), 1);
    QVERIFY(model.favorites().isEmpty());
}

void FavoritesModelTest::rejectsBlankNamesAndStoresTrimmedNames() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FavoritesModel model;
    QVERIFY(!model.addFavorite("   ", directory.path()));
    QCOMPARE(model.rowCount(), 0);

    QVERIFY(model.addFavorite("  Project  ", directory.path()));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.favoriteAt(0).name, QString("Project"));
}

void FavoritesModelTest::setFavoritesRejectsBlankNamesAndStoresTrimmedNames() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    FavoritesModel model;
    model.setFavorites({{"  First  ", first.path()}, {"   ", second.path()}});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.favoriteAt(0).name, QString("First"));
    QCOMPARE(model.favoriteAt(0).path, QDir::cleanPath(first.path()));
}

void FavoritesModelTest::rejectsDuplicatePathsAfterNormalization() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FavoritesModel model;
    QVERIFY(model.addFavorite("Project", directory.path()));
    QVERIFY(!model.addFavorite("Same Project", QDir(directory.path()).filePath(".")));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.favoriteAt(0).name, QString("Project"));
}

void FavoritesModelTest::missingPathsRemainVisibleButUnavailable() {
    const QString missingPath = QDir::cleanPath(QDir::temp().filePath("filemanager-qt-missing-favorite-path"));
    QVERIFY(!QFileInfo::exists(missingPath));

    FavoritesModel model;
    QVERIFY(model.addFavorite("Missing", missingPath));

    const QModelIndex index = model.index(0, 0);
    QVERIFY(index.isValid());
    QCOMPARE(model.data(index, FavoritesModel::NameRole).toString(), QString("Missing"));
    QCOMPARE(model.data(index, FavoritesModel::PathRole).toString(), missingPath);
    QCOMPARE(model.data(index, FavoritesModel::AvailableRole).toBool(), false);
    QCOMPARE(model.data(index, FavoritesModel::MissingRole).toBool(), true);
    QVERIFY(model.data(index, Qt::DisplayRole).toString().contains("Missing"));
}

void FavoritesModelTest::exposesModelRoles() {
    FavoritesModel model;
    const QHash<int, QByteArray> roles = model.roleNames();

    QCOMPARE(roles.value(FavoritesModel::NameRole), QByteArray("name"));
    QCOMPARE(roles.value(FavoritesModel::PathRole), QByteArray("path"));
    QCOMPARE(roles.value(FavoritesModel::AvailableRole), QByteArray("available"));
    QCOMPARE(roles.value(FavoritesModel::MissingRole), QByteArray("missing"));
}

void FavoritesModelTest::invalidRemoveReturnsFalseWithoutChangingModel() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    FavoritesModel model;
    QVERIFY(model.addFavorite("Project", directory.path()));

    QVERIFY(!model.removeFavorite(-1));
    QVERIFY(!model.removeFavorite(1));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.favoriteAt(-1).name, QString());
    QCOMPARE(model.favoriteAt(1).path, QString());
}

QTEST_MAIN(FavoritesModelTest)
#include "test_favorites_model.moc"
