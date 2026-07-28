#include "services/GitService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QTimer>

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
        if (gitMarker.isDir()) {
            return QDir::cleanPath(directory.absolutePath());
        }
        if (gitMarker.isFile()) {
            QFile markerFile(gitMarker.absoluteFilePath());
            if (markerFile.open(QIODevice::ReadOnly)) {
                const QString firstLine = QString::fromUtf8(markerFile.readLine()).trimmed();
                const QString prefix = QStringLiteral("gitdir: ");
                if (firstLine.startsWith(prefix)) {
                    const QString gitDirectory = firstLine.sliced(prefix.size()).trimmed();
                    const QFileInfo gitDirectoryInfo(QDir(directory).absoluteFilePath(gitDirectory));
                    if (!gitDirectory.isEmpty() && gitDirectoryInfo.exists() && gitDirectoryInfo.isDir()) {
                        return QDir::cleanPath(directory.absolutePath());
                    }
                }
            }
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

void GitService::run(const QString &repositoryRoot, const QStringList &arguments, CommandCallback callback) {
    const auto delivered = std::make_shared<bool>(false);
    const auto complete = [this, callback = std::move(callback), delivered](const GitCommandResult &result) {
        QMetaObject::invokeMethod(this, [callback, delivered, result] {
            if (*delivered) {
                return;
            }
            *delivered = true;
            callback(result);
        }, Qt::QueuedConnection);
    };

    if (repositoryRoot.isEmpty()) {
        complete({false, -1, {}, {}, QStringLiteral("Repository root is empty.")});
        return;
    }

    if (commandRunner_) {
        commandRunner_(repositoryRoot, arguments, complete);
        return;
    }

    auto *process = new QProcess(this);
    process->setWorkingDirectory(repositoryRoot);
    auto *timeoutTimer = new QTimer(process);
    timeoutTimer->setSingleShot(true);

    connect(process, &QProcess::errorOccurred, process, [process, timeoutTimer, complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            timeoutTimer->stop();
            GitCommandResult result;
            result.startError = QStringLiteral("Unable to start git: %1").arg(process->errorString());
            complete(result);
            process->deleteLater();
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
            [process, timeoutTimer, complete](int exitCode, QProcess::ExitStatus) {
                timeoutTimer->stop();
                GitCommandResult result;
                result.started = true;
                result.exitCode = exitCode;
                result.standardOutput = QString::fromLocal8Bit(process->readAllStandardOutput());
                result.standardError = QString::fromLocal8Bit(process->readAllStandardError());
                complete(result);
                process->deleteLater();
            });
    connect(timeoutTimer, &QTimer::timeout, process, [process, complete, timeout = commandTimeoutMilliseconds_] {
        GitCommandResult result;
        result.started = true;
        result.startError = QStringLiteral("Git command timed out after %1 ms.").arg(timeout);
        complete(result);
        if (process->state() != QProcess::NotRunning) {
            process->terminate();
            QTimer::singleShot(1000, process, [process] {
                if (process->state() != QProcess::NotRunning) {
                    process->kill();
                }
            });
        }
    });
    process->start(QStringLiteral("git"), arguments);
    timeoutTimer->start(commandTimeoutMilliseconds_);
}

void GitService::setCommandRunner(CommandRunner runner) {
    commandRunner_ = std::move(runner);
}

void GitService::setCommandTimeout(int timeoutMilliseconds) {
    commandTimeoutMilliseconds_ = qMax(1, timeoutMilliseconds);
}
