#include "services/FileOperationService.h"

#include "services/PlatformServices.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <filesystem>

namespace {

void setError(QString *errorMessage, const QString &message) {
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool pathExistsOrIsSymlink(const QString &path) {
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

bool validateSources(const QStringList &sources, QString *errorMessage) {
    if (sources.isEmpty()) {
        setError(errorMessage, QStringLiteral("No source paths were provided."));
        return false;
    }

    for (const QString &source : sources) {
        if (!pathExistsOrIsSymlink(source)) {
            setError(errorMessage, QStringLiteral("Source path does not exist: %1").arg(source));
            return false;
        }
    }
    return true;
}

} // namespace

FileOperationService::FileOperationService(PlatformServices *platformServices) {
    if (platformServices == nullptr) {
        ownedPlatformServices_ = std::make_unique<PlatformServices>();
        platformServices_ = ownedPlatformServices_.get();
    } else {
        platformServices_ = platformServices;
    }
}

FileOperationService::~FileOperationService() = default;

bool FileOperationService::copy(const QStringList &sources, const QString &destination, QString *errorMessage) {
    if (!validateSources(sources, errorMessage)) {
        return false;
    }

    const QFileInfo destinationInfo(destination);
    if (!destinationInfo.exists() || !destinationInfo.isDir()) {
        setError(errorMessage, QStringLiteral("Destination directory does not exist: %1").arg(destination));
        return false;
    }

    for (const QString &source : sources) {
        if (!copyOne(source, destinationPathForSource(source, destination), errorMessage)) {
            return false;
        }
    }
    return true;
}

bool FileOperationService::move(const QStringList &sources, const QString &destination, QString *errorMessage) {
    if (!validateSources(sources, errorMessage)) {
        return false;
    }

    const QFileInfo destinationInfo(destination);
    if (!destinationInfo.exists() || !destinationInfo.isDir()) {
        setError(errorMessage, QStringLiteral("Destination directory does not exist: %1").arg(destination));
        return false;
    }

    for (const QString &source : sources) {
        const QString target = destinationPathForSource(source, destination);
        if (pathExistsOrIsSymlink(target)) {
            setError(errorMessage, QStringLiteral("Target path already exists: %1").arg(target));
            return false;
        }
        if (QFile::rename(source, target)) {
            continue;
        }
        if (!copyOne(source, target, errorMessage)) {
            return false;
        }
        if (!removePath(source, errorMessage)) {
            return false;
        }
    }
    return true;
}

bool FileOperationService::renamePath(const QString &source, const QString &targetName, QString *errorMessage) {
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.exists() && !sourceInfo.isSymLink()) {
        setError(errorMessage, QStringLiteral("Source path does not exist: %1").arg(source));
        return false;
    }

    const QString cleanName = targetName.trimmed();
    if (cleanName.isEmpty() || cleanName.contains(QLatin1Char('/')) || cleanName.contains(QLatin1Char('\\'))) {
        setError(errorMessage, QStringLiteral("Invalid target name: %1").arg(targetName));
        return false;
    }

    const QString target = QDir(sourceInfo.absolutePath()).filePath(cleanName);
    if (pathExistsOrIsSymlink(target)) {
        setError(errorMessage, QStringLiteral("Target path already exists: %1").arg(target));
        return false;
    }

    if (!QFile::rename(source, target)) {
        setError(errorMessage, QStringLiteral("Could not rename %1 to %2").arg(source, target));
        return false;
    }
    return true;
}

bool FileOperationService::createFolder(const QString &parentDir, const QString &name, QString *errorMessage) {
    const QFileInfo parentInfo(parentDir);
    if (!parentInfo.exists() || !parentInfo.isDir()) {
        setError(errorMessage, QStringLiteral("Parent directory does not exist: %1").arg(parentDir));
        return false;
    }

    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty() || cleanName.contains(QLatin1Char('/')) || cleanName.contains(QLatin1Char('\\'))) {
        setError(errorMessage, QStringLiteral("Invalid folder name: %1").arg(name));
        return false;
    }

    QDir parent(parentDir);
    if (!parent.mkdir(cleanName)) {
        setError(errorMessage, QStringLiteral("Could not create folder: %1").arg(parent.filePath(cleanName)));
        return false;
    }
    return true;
}

bool FileOperationService::deleteToTrash(const QStringList &paths, QString *errorMessage) {
    if (!validateSources(paths, errorMessage)) {
        return false;
    }
    return platformServices_->trashPaths(paths, errorMessage);
}

bool FileOperationService::copyOne(const QString &source, const QString &target, QString *errorMessage) {
    const QFileInfo sourceInfo(source);
    if (pathExistsOrIsSymlink(target)) {
        setError(errorMessage, QStringLiteral("Target path already exists: %1").arg(target));
        return false;
    }

    if (sourceInfo.isSymLink()) {
        const std::filesystem::path linkTarget = std::filesystem::read_symlink(source.toStdString());
        std::error_code errorCode;
        std::filesystem::create_symlink(linkTarget, target.toStdString(), errorCode);
        if (errorCode) {
            setError(errorMessage, QStringLiteral("Could not copy symlink %1 to %2").arg(source, target));
            return false;
        }
        return true;
    }

    if (sourceInfo.isDir()) {
        if (targetIsInsideSource(source, target)) {
            setError(errorMessage, QStringLiteral("Cannot copy or move a directory inside itself: %1").arg(source));
            return false;
        }
        return copyDirectory(source, target, errorMessage);
    }

    if (!QFile::copy(source, target)) {
        setError(errorMessage, QStringLiteral("Could not copy %1 to %2").arg(source, target));
        return false;
    }
    return true;
}

bool FileOperationService::copyDirectory(const QString &source, const QString &target, QString *errorMessage) {
    QDir sourceDir(source);
    if (!QDir().mkpath(target)) {
        setError(errorMessage, QStringLiteral("Could not create target directory: %1").arg(target));
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : entries) {
        const QString childTarget = QDir(target).filePath(entry.fileName());
        if (!copyOne(entry.absoluteFilePath(), childTarget, errorMessage)) {
            return false;
        }
    }
    return true;
}

bool FileOperationService::removePath(const QString &path, QString *errorMessage) {
    const QFileInfo info(path);
    bool removed = false;
    if (info.isSymLink()) {
        removed = QFile::remove(path);
    } else if (info.isDir()) {
        removed = QDir(path).removeRecursively();
    } else {
        removed = QFile::remove(path);
    }

    if (!removed) {
        setError(errorMessage, QStringLiteral("Could not remove source path after move: %1").arg(path));
        return false;
    }
    return true;
}

bool FileOperationService::targetIsInsideSource(const QString &source, const QString &target) const {
    const QString sourcePath = QDir(source).canonicalPath();
    if (sourcePath.isEmpty()) {
        return false;
    }

    QFileInfo targetInfo(target);
    QString targetPath = targetInfo.exists() ? targetInfo.canonicalFilePath() : QFileInfo(targetInfo.absolutePath()).canonicalFilePath();
    if (targetPath.isEmpty()) {
        targetPath = QDir::cleanPath(targetInfo.absoluteFilePath());
    }

    return targetPath == sourcePath || targetPath.startsWith(sourcePath + QLatin1Char('/'));
}

QString FileOperationService::destinationPathForSource(const QString &source, const QString &destination) const {
    return QDir(destination).filePath(QFileInfo(source).fileName());
}
