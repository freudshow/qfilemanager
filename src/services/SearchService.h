#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class SearchService : public QObject {
    Q_OBJECT

public:
    explicit SearchService(QObject *parent = nullptr);

    QStringList search(const QString &rootPath, const QString &query);

signals:
    void resultFound(const QString &path);
    void searchFinished(const QStringList &results);
};
