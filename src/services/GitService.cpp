#include "services/GitService.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <memory>

bool GitCommandResult::succeeded() const {
    return started && exitCode == 0;
}

GitService::GitService(QObject *parent)
    : QObject(parent) {
}

QString GitService::findRepositoryRoot(const QString &path) const {
    const QFileInfo pathInfo(path);
    if (!pathInfo.exists()) {
        return {};
    }

    QDir directory = pathInfo.isDir() ? QDir(pathInfo.absoluteFilePath()) : pathInfo.absoluteDir();
    while (directory.exists()) {
        const QFileInfo gitMarker(directory.filePath(QStringLiteral(".git")));
        if (gitMarker.exists() && (gitMarker.isDir() || gitMarker.isFile())) {
            return QDir::cleanPath(directory.absolutePath());
        }
        if (!directory.cdUp()) {
            break;
        }
    }

    return {};
}

bool GitService::isDirtyPorcelainOutput(const QString &output) {
    return !output.trimmed().isEmpty();
}

void GitService::run(const QString &repositoryRoot, const QStringList &arguments, CommandCallback callback) const {
    if (repositoryRoot.isEmpty()) {
        callback({false, -1, {}, {}, QStringLiteral("Repository root is empty.")});
        return;
    }

    if (commandRunner_) {
        commandRunner_(repositoryRoot, arguments, std::move(callback));
        return;
    }

    auto *process = new QProcess(const_cast<GitService *>(this));
    process->setWorkingDirectory(repositoryRoot);
    const auto delivered = std::make_shared<bool>(false);
    const auto complete = [process, callback = std::move(callback), delivered](GitCommandResult result) {
        if (*delivered) {
            return;
        }
        *delivered = true;
        callback(result);
        process->deleteLater();
    };

    connect(process, &QProcess::errorOccurred, process, [process, complete](QProcess::ProcessError error) mutable {
        if (error == QProcess::FailedToStart) {
            GitCommandResult result;
            result.startError = QStringLiteral("Unable to start git: %1").arg(process->errorString());
            complete(std::move(result));
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
            [process, complete](int exitCode, QProcess::ExitStatus) mutable {
                GitCommandResult result;
                result.started = true;
                result.exitCode = exitCode;
                result.standardOutput = QString::fromLocal8Bit(process->readAllStandardOutput());
                result.standardError = QString::fromLocal8Bit(process->readAllStandardError());
                complete(std::move(result));
            });
    process->start(QStringLiteral("git"), arguments);
}

void GitService::setCommandRunner(CommandRunner runner) {
    commandRunner_ = std::move(runner);
}
