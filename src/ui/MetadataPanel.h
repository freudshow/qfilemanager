#pragma once

#include <QWidget>

#include <QHash>

#include "models/FileMetadata.h"

class QLabel;

class MetadataPanel : public QWidget {
    Q_OBJECT

public:
    explicit MetadataPanel(QWidget *parent = nullptr);

    void setMetadata(const FileMetadata &metadata);
    void clear();
    QString displayedValue(const QString &key) const;

private:
    void setValue(const QString &key, const QString &value);

    QHash<QString, QLabel *> valueLabels_;
};
