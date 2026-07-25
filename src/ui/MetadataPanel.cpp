#include "ui/MetadataPanel.h"

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
const QStringList metadataKeys() {
    return {
        QStringLiteral("Name"),
        QStringLiteral("Path"),
        QStringLiteral("Type"),
        QStringLiteral("Size"),
        QStringLiteral("Created"),
        QStringLiteral("Modified"),
        QStringLiteral("Accessed"),
        QStringLiteral("Permissions"),
        QStringLiteral("Owner"),
        QStringLiteral("Group"),
        QStringLiteral("Extension"),
        QStringLiteral("Symlink Target"),
        QStringLiteral("Root Capacity"),
        QStringLiteral("Root Free Space"),
    };
}
}

MetadataPanel::MetadataPanel(QWidget *parent)
    : QWidget(parent) {
    setObjectName("metadataPanel");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *label = new QLabel(tr("Metadata"), this);
    label->setObjectName("metadataPanelLabel");
    layout->addWidget(label);

    auto *form = new QFormLayout();
    form->setObjectName("metadataForm");
    for (const QString &key : metadataKeys()) {
        auto *value = new QLabel(QString::fromLatin1(FileMetadata::Unavailable), this);
        value->setObjectName(QStringLiteral("metadata%1Value").arg(key.simplified().remove(QLatin1Char(' '))));
        value->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        value->setWordWrap(true);
        valueLabels_.insert(key, value);
        form->addRow(tr("%1:").arg(key), value);
    }

    layout->addLayout(form);
    layout->addStretch(1);
}

void MetadataPanel::setMetadata(const FileMetadata &metadata) {
    setValue(QStringLiteral("Name"), metadata.name);
    setValue(QStringLiteral("Path"), metadata.path);
    setValue(QStringLiteral("Type"), metadata.type);
    setValue(QStringLiteral("Size"), metadata.displaySize());
    setValue(QStringLiteral("Created"), metadata.displayCreated());
    setValue(QStringLiteral("Modified"), metadata.displayModified());
    setValue(QStringLiteral("Accessed"), metadata.displayAccessed());
    setValue(QStringLiteral("Permissions"), metadata.displayPermissions());
    setValue(QStringLiteral("Owner"), metadata.owner);
    setValue(QStringLiteral("Group"), metadata.group);
    setValue(QStringLiteral("Extension"), metadata.extension);
    setValue(QStringLiteral("Symlink Target"), metadata.displaySymlinkTarget());
    setValue(QStringLiteral("Root Capacity"), metadata.displayRootCapacity());
    setValue(QStringLiteral("Root Free Space"), metadata.displayRootFreeSpace());
}

void MetadataPanel::clear() {
    for (const QString &key : metadataKeys()) {
        setValue(key, QString::fromLatin1(FileMetadata::Unavailable));
    }
}

QString MetadataPanel::displayedValue(const QString &key) const {
    const auto it = valueLabels_.constFind(key);
    if (it == valueLabels_.constEnd()) {
        return QString();
    }
    return (*it)->text();
}

void MetadataPanel::setValue(const QString &key, const QString &value) {
    const auto it = valueLabels_.find(key);
    if (it == valueLabels_.end()) {
        return;
    }
    (*it)->setText(value.isEmpty() ? QString::fromLatin1(FileMetadata::Unavailable) : value);
}
