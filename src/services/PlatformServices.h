#pragma once

#include <QString>
#include <QStringList>

class QUrl;

class PlatformServices {
public:
    virtual ~PlatformServices() = default;

    virtual bool openPath(const QString &path, QString *errorMessage = nullptr);
    virtual bool trashPaths(const QStringList &paths, QString *errorMessage = nullptr);
    virtual bool showProperties(const QString &path, QString *errorMessage = nullptr);
};
