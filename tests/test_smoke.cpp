#include <QtTest/QtTest>

#include <QMainWindow>

#include <type_traits>

#include "MainWindow.h"

class SmokeTest : public QObject {
    Q_OBJECT

private slots:
    void projectSkeletonBuilds();
};

void SmokeTest::projectSkeletonBuilds() {
    QVERIFY((std::is_base_of_v<QMainWindow, MainWindow>));
}

QTEST_MAIN(SmokeTest)
#include "test_smoke.moc"
