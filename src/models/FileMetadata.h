#pragma once

#include <QDateTime>
#include <QFileDevice>
#include <QString>

class FileMetadata {
public:
    static constexpr const char *Unavailable = "Unavailable";

    QString name;
    QString path;
    QString type = Unavailable;
    qint64 size = -1;
    QDateTime created;
    QDateTime modified;
    QDateTime accessed;
    QFileDevice::Permissions permissions = {};
    QString owner = Unavailable;
    QString group = Unavailable;
    QString extension = Unavailable;
    QString symlinkTarget;
    qint64 rootCapacity = -1;
    qint64 rootFreeSpace = -1;

    static FileMetadata fromPath(const QString &path);
    static QString formatSize(qint64 bytes);
    static QString formatPermissions(QFileDevice::Permissions permissions);
    static QString formatTimestamp(const QDateTime &timestamp);

    QString displaySize() const;
    QString displayCreated() const;
    QString displayModified() const;
    QString displayAccessed() const;
    QString displayPermissions() const;
    QString displaySymlinkTarget() const;
    QString displayRootCapacity() const;
    QString displayRootFreeSpace() const;
};
