#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

namespace AppPaths {

inline QString configDir() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (path.isEmpty()) {
        path = QDir::home().filePath(".config/filemanager");
    }
    return path;
}

} // namespace AppPaths
