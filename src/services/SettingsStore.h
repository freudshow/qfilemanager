#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

struct TabState {
    QString path;
    QString sortColumn;
    QString sortOrder;
};

struct FavoriteState {
    QString name;
    QString path;
};

struct AppSettings {
    int version = 1;
    QByteArray windowGeometry;
    QByteArray windowState;
    QVector<int> splitterSizes;
    QVector<TabState> tabs;
    QVector<FavoriteState> favorites;
    QHash<QString, QString> openWithDefaults;
    bool showHiddenFiles = false;
    bool confirmDeleteToTrash = true;
};

class SettingsStore {
public:
    QString settingsPath() const;
    bool load(AppSettings &settings, QString *errorMessage = nullptr);
    bool save(const AppSettings &settings, QString *errorMessage = nullptr);
};
