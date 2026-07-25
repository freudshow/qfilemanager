#include "services/TabManager.h"

#include "ui/FileBrowserWidget.h"

#include <QDir>
#include <QFileInfo>
#include <QTabWidget>

TabManager::TabManager(QObject *parent)
    : QObject(parent) {
}

void TabManager::setTabWidget(QTabWidget *tabWidget) {
    tabWidget_ = tabWidget;
}

QTabWidget *TabManager::tabWidget() const {
    return tabWidget_;
}

FileBrowserWidget *TabManager::addTab(const QString &path) {
    if (tabWidget_ == nullptr) {
        return nullptr;
    }

    auto *browser = new FileBrowserWidget(tabWidget_);
    const QString targetPath = QFileInfo(path).isDir() ? path : QDir::homePath();
    browser->setCurrentPath(targetPath);

    const auto updateTitle = [this, browser](const QString &newPath) {
        const int index = tabWidget_->indexOf(browser);
        if (index >= 0) {
            tabWidget_->setTabText(index, QFileInfo(newPath).fileName().isEmpty() ? newPath : QFileInfo(newPath).fileName());
        }
    };

    const QString tabTitle = QFileInfo(browser->currentPath()).fileName().isEmpty() ? browser->currentPath() : QFileInfo(browser->currentPath()).fileName();
    tabWidget_->addTab(browser, tabTitle);
    connect(browser, &FileBrowserWidget::pathChanged, this, updateTitle);
    connect(browser, &FileBrowserWidget::openPathInNewTabRequested, this, [this](const QString &newTabPath) {
        FileBrowserWidget *newBrowser = addTab(newTabPath);
        if (newBrowser != nullptr) {
            tabWidget_->setCurrentWidget(newBrowser);
        }
    });
    return browser;
}

void TabManager::closeTab(int index) {
    if (tabWidget_ == nullptr || index < 0 || index >= tabWidget_->count() || tabWidget_->count() <= 1) {
        return;
    }

    QWidget *widget = tabWidget_->widget(index);
    tabWidget_->removeTab(index);
    delete widget;
}

void TabManager::restoreTabs(const AppSettings &settings) {
    if (tabWidget_ == nullptr) {
        return;
    }

    while (tabWidget_->count() > 0) {
        QWidget *widget = tabWidget_->widget(0);
        tabWidget_->removeTab(0);
        delete widget;
    }

    bool restoredAny = false;
    for (const TabState &tab : settings.tabs) {
        if (QFileInfo(tab.path).isDir()) {
            addTab(tab.path);
            restoredAny = true;
        }
    }

    if (!restoredAny) {
        addTab(QDir::homePath());
    }
}

QVector<TabState> TabManager::tabStates() const {
    QVector<TabState> states;
    if (tabWidget_ == nullptr) {
        return states;
    }

    for (int i = 0; i < tabWidget_->count(); ++i) {
        auto *browser = qobject_cast<FileBrowserWidget *>(tabWidget_->widget(i));
        if (browser != nullptr) {
            states.append({browser->currentPath(), QStringLiteral("name"), QStringLiteral("ascending")});
        }
    }
    return states;
}

int TabManager::count() const {
    return tabWidget_ == nullptr ? 0 : tabWidget_->count();
}

FileBrowserWidget *TabManager::browserAt(int index) const {
    if (tabWidget_ == nullptr) {
        return nullptr;
    }
    return qobject_cast<FileBrowserWidget *>(tabWidget_->widget(index));
}
