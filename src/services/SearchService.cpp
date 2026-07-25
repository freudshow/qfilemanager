#include "services/SearchService.h"

#include <QDirIterator>
#include <QFileInfo>

SearchService::SearchService(QObject *parent)
    : QObject(parent) {
}

QStringList SearchService::search(const QString &rootPath, const QString &query) {
    QStringList results;
    const QFileInfo rootInfo(rootPath);
    const QString trimmedQuery = query.trimmed();
    if (!rootInfo.exists() || !rootInfo.isDir() || trimmedQuery.isEmpty()) {
        emit searchFinished(results);
        return results;
    }

    QDirIterator iterator(rootInfo.absoluteFilePath(), QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (QFileInfo(path).fileName().contains(trimmedQuery, Qt::CaseInsensitive)) {
            results.append(path);
            emit resultFound(path);
        }
    }

    emit searchFinished(results);
    return results;
}
