#include <QtTest/QtTest>

#include "services/ThemeManager.h"

class ThemeManagerTest : public QObject {
    Q_OBJECT

private slots:
    void catalogHasStableNamesAndDisplayNames();
    void unknownNamesFallBackToAurora();
    void everyThemeProvidesAStyleSheet();
};

void ThemeManagerTest::catalogHasStableNamesAndDisplayNames() {
    const QList<ThemeManager::Theme> themes = {
        ThemeManager::Theme::Aurora,
        ThemeManager::Theme::Graphite,
        ThemeManager::Theme::Clearwater,
    };
    const QStringList names = {QStringLiteral("aurora"), QStringLiteral("graphite"), QStringLiteral("clearwater")};
    const QStringList displayNames = {QStringLiteral("Aurora Garden"), QStringLiteral("Graphite Ember"), QStringLiteral("Clearwater")};

    for (int i = 0; i < themes.size(); ++i) {
        QCOMPARE(ThemeManager::name(themes.at(i)), names.at(i));
        QCOMPARE(ThemeManager::displayName(themes.at(i)), displayNames.at(i));
        QCOMPARE(ThemeManager::fromName(names.at(i).toUpper()), themes.at(i));
    }
}

void ThemeManagerTest::unknownNamesFallBackToAurora() {
    QCOMPARE(ThemeManager::fromName(QStringLiteral("unknown")), ThemeManager::Theme::Aurora);
    QCOMPARE(ThemeManager::fromName(QStringLiteral("  ")), ThemeManager::Theme::Aurora);
}

void ThemeManagerTest::everyThemeProvidesAStyleSheet() {
    const QList<ThemeManager::Theme> themes = {
        ThemeManager::Theme::Aurora,
        ThemeManager::Theme::Graphite,
        ThemeManager::Theme::Clearwater,
    };
    for (const ThemeManager::Theme theme : themes) {
        QVERIFY(!ThemeManager::styleSheet(theme).trimmed().isEmpty());
    }
}

QTEST_APPLESS_MAIN(ThemeManagerTest)
#include "test_theme_manager.moc"
