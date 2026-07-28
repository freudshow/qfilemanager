#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

struct GitCommandResult {
    bool started = false;
    int exitCode = -1;
    QString standardOutput;
    QString standardError;
    QString startError;

    bool succeeded() const;
};

class GitService : public QObject {
    Q_OBJECT

public:
    using CommandCallback = std::function<void(const GitCommandResult &)>;
    using CommandRunner = std::function<void(const QString &workingDirectory, const QStringList &arguments, CommandCallback)>;

    explicit GitService(QObject *parent = nullptr);

    QString findRepositoryRoot(const QString &path) const;
    static bool isDirtyPorcelainOutput(const QString &output);
    void run(const QString &repositoryRoot, const QStringList &arguments, CommandCallback callback);
    void setCommandRunner(CommandRunner runner);
    void setCommandTimeout(int timeoutMilliseconds);

private:
    CommandRunner commandRunner_;
    int commandTimeoutMilliseconds_ = 30000;
};
