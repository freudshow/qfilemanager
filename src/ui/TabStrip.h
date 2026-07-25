#pragma once

#include <QWidget>

class FileBrowserWidget;
class QToolButton;
class QTabWidget;
class TabManager;

class TabStrip : public QWidget {
    Q_OBJECT

public:
    explicit TabStrip(QWidget *parent = nullptr);
    QTabWidget *tabWidget() const;
    void setTabManager(TabManager *manager);
    int addTab(FileBrowserWidget *browser, const QString &title);

private:
    QTabWidget *tabWidget_ = nullptr;
    QToolButton *newTabButton_ = nullptr;
};
