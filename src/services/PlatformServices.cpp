#include "services/PlatformServices.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace {

bool pathExistsOrIsSymlink(const QString &path) {
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

} // namespace

bool PlatformServices::openPath(const QString &path, QString *errorMessage) {
    if (!pathExistsOrIsSymlink(path)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Path does not exist: %1").arg(path);
        }
        return false;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not open path: %1").arg(path);
        }
        return false;
    }
    return true;
}

bool PlatformServices::trashPaths(const QStringList &paths, QString *errorMessage) {
    for (const QString &path : paths) {
        if (!pathExistsOrIsSymlink(path)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Path does not exist: %1").arg(path);
            }
            return false;
        }
    }

    for (const QString &path : paths) {
        if (!QFile::moveToTrash(path)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Could not move to trash: %1").arg(path);
            }
            return false;
        }
    }
    return true;
}

bool PlatformServices::showProperties(const QString &path, QString *errorMessage) {
    if (!pathExistsOrIsSymlink(path)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Path does not exist: %1").arg(path);
        }
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Properties dialog is not available on this platform yet.");
    }
    return false;
}
