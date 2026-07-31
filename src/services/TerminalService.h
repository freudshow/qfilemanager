#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class TerminalService {
public:
    using Launcher = std::function<bool(const QString &program, const QStringList &arguments, const QString &workingDirectory)>;

    TerminalService();

    bool open(const QString &directory, QString *errorMessage = nullptr);
    void setLauncher(Launcher launcher);

private:
    bool selectTerminal(QString &program, QStringList &arguments, QString *errorMessage) const;

    Launcher launcher_;
    bool launcherInjected_ = false;
};
