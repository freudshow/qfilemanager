#include "ui/TabStrip.h"

#include "services/TabManager.h"

#include "ui/FileBrowserWidget.h"

#include <QLabel>
#include <QTabWidget>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDir>

TabStrip::TabStrip(QWidget *parent)
    : QWidget(parent)
    , tabWidget_(new QTabWidget(this))
    , newTabButton_(new QToolButton(this)) {
    setObjectName("tabStrip");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *headerLayout = new QHBoxLayout();
    auto *label = new QLabel(tr("Tabs and Files"), this);
    label->setObjectName("tabStripLabel");
    newTabButton_->setObjectName("newTabButton");
    newTabButton_->setText(QStringLiteral("+"));
    newTabButton_->setToolTip(tr("New tab"));
    headerLayout->addWidget(label);
    headerLayout->addStretch(1);
    headerLayout->addWidget(newTabButton_);
    layout->addLayout(headerLayout);

    tabWidget_->setObjectName("tabPlaceholder");
    tabWidget_->setTabsClosable(true);
    layout->addWidget(tabWidget_, 1);

}

QTabWidget *TabStrip::tabWidget() const {
    return tabWidget_;
}

void TabStrip::setTabManager(TabManager *manager) {
    if (manager != nullptr) {
        manager->setTabWidget(tabWidget_);
        connect(newTabButton_, &QToolButton::clicked, manager, [manager] {
            manager->addTab(QDir::homePath());
            QTabWidget *tabWidget = manager->tabWidget();
            if (tabWidget != nullptr) {
                tabWidget->setCurrentIndex(tabWidget->count() - 1);
            }
        });
        connect(tabWidget_, &QTabWidget::tabCloseRequested, manager, &TabManager::closeTab);
    }
}

int TabStrip::addTab(FileBrowserWidget *browser, const QString &title) {
    return tabWidget_->addTab(browser, title);
}
