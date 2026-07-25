#include "ui/TabStrip.h"

#include "services/TabManager.h"

#include "ui/FileBrowserWidget.h"

#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

TabStrip::TabStrip(QWidget *parent)
    : QWidget(parent)
    , tabWidget_(new QTabWidget(this)) {
    setObjectName("tabStrip");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *label = new QLabel(tr("Tabs and Files"), this);
    label->setObjectName("tabStripLabel");
    layout->addWidget(label);

    tabWidget_->setObjectName("tabPlaceholder");
    layout->addWidget(tabWidget_, 1);
}

QTabWidget *TabStrip::tabWidget() const {
    return tabWidget_;
}

void TabStrip::setTabManager(TabManager *manager) {
    if (manager != nullptr) {
        manager->setTabWidget(tabWidget_);
    }
}

int TabStrip::addTab(FileBrowserWidget *browser, const QString &title) {
    return tabWidget_->addTab(browser, title);
}
