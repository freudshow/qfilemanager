#include "models/FileMetadata.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QLocale>

FileMetadata FileMetadata::fromPath(const QString &path) {
    FileMetadata metadata;
    const QFileInfo info(path);
    metadata.name = info.fileName().isEmpty() ? path : info.fileName();
    metadata.path = QDir::cleanPath(info.absoluteFilePath());

    const QStorageInfo storage(info.exists() ? info.absoluteFilePath() : QFileInfo(path).absolutePath());
    if (storage.isValid()) {
        metadata.rootCapacity = storage.bytesTotal();
        metadata.rootFreeSpace = storage.bytesAvailable();
    }

    if (!info.exists() && !info.isSymLink()) {
        return metadata;
    }

    metadata.type = info.isSymLink() ? QStringLiteral("Symlink") : (info.isDir() ? QStringLiteral("Folder") : QStringLiteral("File"));
    metadata.size = info.isDir() ? -1 : info.size();
    metadata.created = info.birthTime();
    metadata.modified = info.lastModified();
    metadata.accessed = info.lastRead();
    metadata.permissions = info.permissions();
    metadata.owner = info.owner().isEmpty() ? QString::fromLatin1(Unavailable) : info.owner();
    metadata.group = info.group().isEmpty() ? QString::fromLatin1(Unavailable) : info.group();

    if (info.isDir()) {
        metadata.extension = QString::fromLatin1(Unavailable);
    } else {
        metadata.extension = info.suffix().isEmpty() ? QString::fromLatin1(Unavailable) : info.suffix();
    }

    if (info.isSymLink()) {
        metadata.symlinkTarget = info.symLinkTarget();
    }

    return metadata;
}

QString FileMetadata::formatSize(qint64 bytes) {
    if (bytes < 0) {
        return QString::fromLatin1(Unavailable);
    }
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }

    static const char *units[] = {"KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unitIndex = -1;
    do {
        value /= 1024.0;
        ++unitIndex;
    } while (value >= 1024.0 && unitIndex < 3);

    return QStringLiteral("%1 %2").arg(QLocale::c().toString(value, 'f', value < 10.0 ? 1 : 0), QString::fromLatin1(units[unitIndex]));
}

QString FileMetadata::formatPermissions(QFileDevice::Permissions permissions) {
    if (permissions == QFileDevice::Permissions()) {
        return QString::fromLatin1(Unavailable);
    }

    QString result;
    result.reserve(9);
    result += permissions.testFlag(QFileDevice::ReadOwner) ? QLatin1Char('r') : QLatin1Char('-');
    result += permissions.testFlag(QFileDevice::WriteOwner) ? QLatin1Char('w') : QLatin1Char('-');
    result += permissions.testFlag(QFileDevice::ExeOwner) ? QLatin1Char('x') : QLatin1Char('-');
    result += permissions.testFlag(QFileDevice::ReadGroup) ? QLatin1Char('r') : QLatin1Char('-');
    result += permissions.testFlag(QFileDevice::WriteGroup) ? QLatin1Char('w') : QLatin1Char('-');
    result += permissions.testFlag(QFileDevice::ExeGroup) ? QLatin1Char('x') : QLatin1Char('-');
    result += permissions.testFlag(QFileDevice::ReadOther) ? QLatin1Char('r') : QLatin1Char('-');
    result += permissions.testFlag(QFileDevice::WriteOther) ? QLatin1Char('w') : QLatin1Char('-');
    result += permissions.testFlag(QFileDevice::ExeOther) ? QLatin1Char('x') : QLatin1Char('-');
    return result;
}

QString FileMetadata::formatTimestamp(const QDateTime &timestamp) {
    if (!timestamp.isValid()) {
        return QString::fromLatin1(Unavailable);
    }
    return timestamp.toLocalTime().toString(Qt::ISODate);
}

QString FileMetadata::displaySize() const {
    return formatSize(size);
}

QString FileMetadata::displayCreated() const {
    return formatTimestamp(created);
}

QString FileMetadata::displayModified() const {
    return formatTimestamp(modified);
}

QString FileMetadata::displayAccessed() const {
    return formatTimestamp(accessed);
}

QString FileMetadata::displayPermissions() const {
    return formatPermissions(permissions);
}

QString FileMetadata::displaySymlinkTarget() const {
    return symlinkTarget.isEmpty() ? QString::fromLatin1(Unavailable) : symlinkTarget;
}

QString FileMetadata::displayRootCapacity() const {
    return formatSize(rootCapacity);
}

QString FileMetadata::displayRootFreeSpace() const {
    return formatSize(rootFreeSpace);
}
