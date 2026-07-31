#include "services/TerminalService.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

void setError(QString *errorMessage, const QString &message) {
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

TerminalService::TerminalService()
    : launcher_([](const QString &program, const QStringList &arguments, const QString &workingDirectory) {
        return QProcess::startDetached(program, arguments, workingDirectory);
    }) {
}

bool TerminalService::open(const QString &directory, QString *errorMessage) {
    const QFileInfo info(directory);
    if (!info.exists() || !info.isDir()) {
        setError(errorMessage, QStringLiteral("Terminal target directory does not exist: %1").arg(directory));
        return false;
    }

    QString program;
    QStringList arguments;
    if (!selectTerminal(program, arguments, errorMessage)) {
        if (!launcherInjected_) {
            return false;
        }
        program = QStringLiteral("terminal");
        arguments.clear();
    }
    if (!launcher_ || !launcher_(program, arguments, info.absoluteFilePath())) {
        setError(errorMessage, QStringLiteral("Unable to launch terminal: %1").arg(program));
        return false;
    }
    return true;
}

void TerminalService::setLauncher(Launcher launcher) {
    launcher_ = std::move(launcher);
    launcherInjected_ = true;
}

bool TerminalService::selectTerminal(QString &program, QStringList &arguments, QString *errorMessage) const {
#if defined(Q_OS_WIN)
    if (QStandardPaths::findExecutable(QStringLiteral("wt.exe")).isEmpty() == false) {
        program = QStringLiteral("wt.exe");
        arguments = {QStringLiteral("-d"), QStringLiteral(".")};
        return true;
    }
    if (!QStandardPaths::findExecutable(QStringLiteral("powershell.exe")).isEmpty()) {
        program = QStringLiteral("powershell.exe");
        arguments = {QStringLiteral("-NoLogo"), QStringLiteral("-NoExit")};
        return true;
    }
#else
    const QStringList configuredParts = QProcess::splitCommand(QString::fromLocal8Bit(qgetenv("TERMINAL")).trimmed());
    if (!configuredParts.isEmpty()) {
        program = configuredParts.constFirst();
        arguments = configuredParts.mid(1);
        return true;
    }
    const QStringList candidates = {
        QStringLiteral("x-terminal-emulator"), QStringLiteral("gnome-terminal"), QStringLiteral("konsole"),
        QStringLiteral("xfce4-terminal"), QStringLiteral("mate-terminal"), QStringLiteral("kitty"),
        QStringLiteral("alacritty"), QStringLiteral("xterm")
    };
    for (const QString &candidate : candidates) {
        if (!QStandardPaths::findExecutable(candidate).isEmpty()) {
            program = candidate;
            return true;
        }
    }
#endif

    setError(errorMessage, QStringLiteral("No supported terminal application was found."));
    return false;
}
