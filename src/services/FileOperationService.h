#pragma once

#include <QString>
#include <QStringList>

#include <memory>

class PlatformServices;

class FileOperationService {
public:
    explicit FileOperationService(PlatformServices *platformServices = nullptr);
    ~FileOperationService();

    bool copy(const QStringList &sources, const QString &destination, QString *errorMessage = nullptr);
    bool move(const QStringList &sources, const QString &destination, QString *errorMessage = nullptr);
    bool renamePath(const QString &source, const QString &targetName, QString *errorMessage = nullptr);
    bool createFolder(const QString &parentDir, const QString &name, QString *errorMessage = nullptr);
    bool deleteToTrash(const QStringList &paths, QString *errorMessage = nullptr);

private:
    bool copyOne(const QString &source, const QString &target, QString *errorMessage);
    bool copyDirectory(const QString &source, const QString &target, QString *errorMessage);
    bool removePath(const QString &path, QString *errorMessage);
    bool targetIsInsideSource(const QString &source, const QString &target) const;
    QString destinationPathForSource(const QString &source, const QString &destination) const;

    PlatformServices *platformServices_ = nullptr;
    std::unique_ptr<PlatformServices> ownedPlatformServices_;
};
