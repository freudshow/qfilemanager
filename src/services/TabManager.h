#pragma once

#include "services/SettingsStore.h"

#include <QObject>

class FileBrowserWidget;
class QTabWidget;

class TabManager : public QObject {
    Q_OBJECT

public:
    explicit TabManager(QObject *parent = nullptr);

    void setTabWidget(QTabWidget *tabWidget);
    QTabWidget *tabWidget() const;
    FileBrowserWidget *addTab(const QString &path);
    void closeTab(int index);
    void restoreTabs(const AppSettings &settings);
    void setOpenWithDefaults(const QHash<QString, QString> &defaults);
    QVector<TabState> tabStates() const;
    int count() const;
    FileBrowserWidget *browserAt(int index) const;

signals:
    void tabAdded(FileBrowserWidget *browser);

private:
    QTabWidget *tabWidget_ = nullptr;
    QHash<QString, QString> openWithDefaults_;
};
