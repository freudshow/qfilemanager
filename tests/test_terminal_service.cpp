#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include "services/TerminalService.h"

class TerminalServiceTest : public QObject {
    Q_OBJECT

private slots:
    void injectedLauncherReceivesDirectoryAndConfiguredArguments();
    void rejectsMissingDirectory();
    void reportsLauncherFailure();
    void injectedLauncherWorksWithoutTerminalDiscovery();
};

void TerminalServiceTest::injectedLauncherReceivesDirectoryAndConfiguredArguments() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("TERMINAL", QByteArray("fake-terminal --new-window"));

    QString program;
    QStringList arguments;
    QString workingDirectory;
    TerminalService service;
    service.setLauncher([&](const QString &requestedProgram, const QStringList &requestedArguments, const QString &requestedDirectory) {
        program = requestedProgram;
        arguments = requestedArguments;
        workingDirectory = requestedDirectory;
        return true;
    });
    QString error;

    QVERIFY2(service.open(directory.path(), &error), qPrintable(error));

    QCOMPARE(program, QStringLiteral("fake-terminal"));
    QCOMPARE(arguments, QStringList({QStringLiteral("--new-window")}));
    QCOMPARE(workingDirectory, QFileInfo(directory.path()).absoluteFilePath());
    qunsetenv("TERMINAL");
}

void TerminalServiceTest::rejectsMissingDirectory() {
    TerminalService service;
    QString error;

    QVERIFY(!service.open(QStringLiteral("/path/that/does/not/exist"), &error));
    QVERIFY(error.contains(QStringLiteral("does not exist")));
}

void TerminalServiceTest::reportsLauncherFailure() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("TERMINAL", QByteArray("fake-terminal"));

    TerminalService service;
    service.setLauncher([](const QString &, const QStringList &, const QString &) {
        return false;
    });
    QString error;

    QVERIFY(!service.open(directory.path(), &error));
    QVERIFY(error.contains(QStringLiteral("Unable to launch terminal")));
    qunsetenv("TERMINAL");
}

void TerminalServiceTest::injectedLauncherWorksWithoutTerminalDiscovery() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray originalPath = qgetenv("PATH");
    const QByteArray originalTerminal = qgetenv("TERMINAL");
    qunsetenv("TERMINAL");
    qputenv("PATH", QByteArray());

    bool launched = false;
    TerminalService service;
    service.setLauncher([&](const QString &, const QStringList &, const QString &) {
        launched = true;
        return true;
    });
    QString error;
    QVERIFY2(service.open(directory.path(), &error), qPrintable(error));
    QVERIFY(launched);

    qputenv("PATH", originalPath);
    if (originalTerminal.isEmpty()) {
        qunsetenv("TERMINAL");
    } else {
        qputenv("TERMINAL", originalTerminal);
    }
}

QTEST_MAIN(TerminalServiceTest)
#include "test_terminal_service.moc"
