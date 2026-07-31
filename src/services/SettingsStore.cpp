#include "services/SettingsStore.h"

#include "services/AppPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <cmath>
#include <limits>

namespace {

void setError(QString *errorMessage, const QString &message) {
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

QString backupPathFor(const QString &path) {
    return path + ".bak";
}

void backupInvalidSettingsFile(const QString &path) {
    const QString backupPath = backupPathFor(path);
    QFile::remove(backupPath);
    QFile::rename(path, backupPath);
}

QJsonArray splitterSizesToJson(const QVector<int> &sizes) {
    QJsonArray array;
    for (const int size : sizes) {
        array.append(size);
    }
    return array;
}

QJsonArray tabsToJson(const QVector<TabState> &tabs) {
    QJsonArray array;
    for (const TabState &tab : tabs) {
        QJsonObject object;
        object["path"] = tab.path;
        object["sortColumn"] = tab.sortColumn;
        object["sortOrder"] = tab.sortOrder;
        array.append(object);
    }
    return array;
}

QJsonArray favoritesToJson(const QVector<FavoriteState> &favorites) {
    QJsonArray array;
    for (const FavoriteState &favorite : favorites) {
        QJsonObject object;
        object["name"] = favorite.name;
        object["path"] = favorite.path;
        array.append(object);
    }
    return array;
}

QJsonObject openWithDefaultsToJson(const QHash<QString, QString> &defaults) {
    QJsonObject object;
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        object.insert(it.key(), it.value());
    }
    return object;
}

bool readBase64Field(const QJsonObject &object, const QString &key, const QString &fieldName, QByteArray &value, QString *errorMessage) {
    const QJsonValue fieldValue = object.value(key);
    if (fieldValue.isUndefined()) {
        value.clear();
        return true;
    }
    if (!fieldValue.isString()) {
        setError(errorMessage, QString("Invalid settings: %1 must be a string.").arg(fieldName));
        return false;
    }

    const QByteArray encoded = fieldValue.toString().toLatin1();
    const QByteArray::FromBase64Result decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.decodingStatus != QByteArray::Base64DecodingStatus::Ok) {
        setError(errorMessage, QString("Invalid settings: %1 must be valid base64.").arg(fieldName));
        return false;
    }

    value = decoded.decoded;
    return true;
}

bool readVersion(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    const QJsonValue versionValue = root.value("version");
    if (versionValue.isUndefined()) {
        return true;
    }
    if (!versionValue.isDouble()) {
        setError(errorMessage, "Invalid settings: version must be a number.");
        return false;
    }

    const double version = versionValue.toDouble();
    if (!std::isfinite(version) || std::floor(version) != version || version < 1.0 || version > std::numeric_limits<int>::max()) {
        setError(errorMessage, "Invalid settings: version must be an integral positive integer within range.");
        return false;
    }

    settings.version = static_cast<int>(version);
    return true;
}

bool readTheme(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    const QJsonValue themeValue = root.value("theme");
    if (themeValue.isUndefined()) {
        return true;
    }
    if (!themeValue.isString()) {
        setError(errorMessage, "Invalid settings: theme must be a string.");
        return false;
    }

    settings.theme = themeValue.toString().trimmed().toLower();
    if (settings.theme.isEmpty()) {
        settings.theme = QStringLiteral("aurora");
    }
    return true;
}

bool readWindowObject(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    const QJsonValue windowValue = root.value("window");
    if (windowValue.isUndefined()) {
        return true;
    }
    if (!windowValue.isObject()) {
        setError(errorMessage, "Invalid settings: window must be an object.");
        return false;
    }

    const QJsonObject window = windowValue.toObject();
    if (!readBase64Field(window, "geometry", "window.geometry", settings.windowGeometry, errorMessage)
        || !readBase64Field(window, "state", "window.state", settings.windowState, errorMessage)) {
        return false;
    }
    if (window.contains("splitters") && !window.value("splitters").isArray()) {
        setError(errorMessage, "Invalid settings: window.splitters must be an array.");
        return false;
    }

    settings.splitterSizes.clear();
    const QJsonArray splitters = window.value("splitters").toArray();
    for (const QJsonValue &sizeValue : splitters) {
        if (!sizeValue.isDouble()) {
            setError(errorMessage, "Invalid settings: splitter sizes must be numbers.");
            return false;
        }
        const double size = sizeValue.toDouble();
        if (!std::isfinite(size) || std::floor(size) != size || size < 0.0 || size > std::numeric_limits<int>::max()) {
            setError(errorMessage, "Invalid settings: splitter sizes must be integral non-negative integers within range.");
            return false;
        }
        settings.splitterSizes.append(static_cast<int>(size));
    }
    return true;
}

bool readTabs(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    const QJsonValue tabsValue = root.value("tabs");
    if (tabsValue.isUndefined()) {
        return true;
    }
    if (!tabsValue.isArray()) {
        setError(errorMessage, "Invalid settings: tabs must be an array.");
        return false;
    }

    settings.tabs.clear();
    for (const QJsonValue &tabValue : tabsValue.toArray()) {
        if (!tabValue.isObject()) {
            setError(errorMessage, "Invalid settings: each tab must be an object.");
            return false;
        }
        const QJsonObject object = tabValue.toObject();
        if (!object.value("path").isString()) {
            setError(errorMessage, "Invalid settings: tab.path must be a string.");
            return false;
        }
        if (object.contains("sortColumn") && !object.value("sortColumn").isString()) {
            setError(errorMessage, "Invalid settings: tab.sortColumn must be a string.");
            return false;
        }
        if (object.contains("sortOrder") && !object.value("sortOrder").isString()) {
            setError(errorMessage, "Invalid settings: tab.sortOrder must be a string.");
            return false;
        }
        settings.tabs.append({object.value("path").toString(), object.value("sortColumn").toString(), object.value("sortOrder").toString()});
    }
    return true;
}

bool readFavorites(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    const QJsonValue favoritesValue = root.value("favorites");
    if (favoritesValue.isUndefined()) {
        return true;
    }
    if (!favoritesValue.isArray()) {
        setError(errorMessage, "Invalid settings: favorites must be an array.");
        return false;
    }

    settings.favorites.clear();
    for (const QJsonValue &favoriteValue : favoritesValue.toArray()) {
        if (!favoriteValue.isObject()) {
            setError(errorMessage, "Invalid settings: each favorite must be an object.");
            return false;
        }
        const QJsonObject object = favoriteValue.toObject();
        if (!object.value("name").isString() || !object.value("path").isString()) {
            setError(errorMessage, "Invalid settings: favorite name and path must be strings.");
            return false;
        }
        settings.favorites.append({object.value("name").toString(), object.value("path").toString()});
    }
    return true;
}

bool readOpenWithDefaults(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    const QJsonValue value = root.value("openWithDefaults");
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isObject()) {
        setError(errorMessage, "Invalid settings: openWithDefaults must be an object.");
        return false;
    }

    settings.openWithDefaults.clear();
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!it.value().isString()) {
            setError(errorMessage, "Invalid settings: openWithDefaults values must be strings.");
            return false;
        }
        if (!it.key().startsWith(QStringLiteral("."))) {
            setError(errorMessage, "Invalid settings: openWithDefaults keys must be file extensions.");
            return false;
        }
        settings.openWithDefaults.insert(it.key().toLower(), it.value().toString());
    }
    return true;
}

bool readOptions(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    const QJsonValue optionsValue = root.value("options");
    if (optionsValue.isUndefined()) {
        return true;
    }
    if (!optionsValue.isObject()) {
        setError(errorMessage, "Invalid settings: options must be an object.");
        return false;
    }

    const QJsonObject options = optionsValue.toObject();
    if (options.contains("showHiddenFiles") && !options.value("showHiddenFiles").isBool()) {
        setError(errorMessage, "Invalid settings: options.showHiddenFiles must be a boolean.");
        return false;
    }
    if (options.contains("confirmDeleteToTrash") && !options.value("confirmDeleteToTrash").isBool()) {
        setError(errorMessage, "Invalid settings: options.confirmDeleteToTrash must be a boolean.");
        return false;
    }

    settings.showHiddenFiles = options.value("showHiddenFiles").toBool(settings.showHiddenFiles);
    settings.confirmDeleteToTrash = options.value("confirmDeleteToTrash").toBool(settings.confirmDeleteToTrash);
    return true;
}

bool readSettingsObject(const QJsonObject &root, AppSettings &settings, QString *errorMessage) {
    settings = AppSettings{};
    return readVersion(root, settings, errorMessage) && readTheme(root, settings, errorMessage) && readWindowObject(root, settings, errorMessage) && readTabs(root, settings, errorMessage)
        && readFavorites(root, settings, errorMessage) && readOpenWithDefaults(root, settings, errorMessage) && readOptions(root, settings, errorMessage);
}

} // namespace

QString SettingsStore::settingsPath() const {
    return QDir(AppPaths::configDir()).filePath("settings.json");
}

bool SettingsStore::load(AppSettings &settings, QString *errorMessage) {
    setError(errorMessage, QString());
    const QString path = settingsPath();
    if (!QFile::exists(path)) {
        settings = AppSettings{};
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QString("Could not open settings file: %1").arg(file.errorString()));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        settings = AppSettings{};
        backupInvalidSettingsFile(path);
        setError(errorMessage, parseError.error == QJsonParseError::NoError
            ? QString("Invalid settings: top-level JSON value must be an object.")
            : QString("Invalid settings JSON: %1").arg(parseError.errorString()));
        return false;
    }

    AppSettings loaded;
    if (!readSettingsObject(document.object(), loaded, errorMessage)) {
        settings = AppSettings{};
        backupInvalidSettingsFile(path);
        return false;
    }

    settings = loaded;
    return true;
}

bool SettingsStore::save(const AppSettings &settings, QString *errorMessage) {
    setError(errorMessage, QString());
    const QString path = settingsPath();
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        setError(errorMessage, QString("Could not create settings directory: %1").arg(fileInfo.absolutePath()));
        return false;
    }

    QJsonObject window;
    window["geometry"] = QString::fromLatin1(settings.windowGeometry.toBase64());
    window["state"] = QString::fromLatin1(settings.windowState.toBase64());
    window["splitters"] = splitterSizesToJson(settings.splitterSizes);

    QJsonObject options;
    options["showHiddenFiles"] = settings.showHiddenFiles;
    options["confirmDeleteToTrash"] = settings.confirmDeleteToTrash;

    QJsonObject root;
    root["version"] = settings.version;
    root["theme"] = settings.theme;
    root["window"] = window;
    root["tabs"] = tabsToJson(settings.tabs);
    root["favorites"] = favoritesToJson(settings.favorites);
    root["openWithDefaults"] = openWithDefaultsToJson(settings.openWithDefaults);
    root["options"] = options;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage, QString("Could not open settings file for writing: %1").arg(file.errorString()));
        return false;
    }

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        setError(errorMessage, QString("Could not write settings file: %1").arg(file.errorString()));
        return false;
    }

    if (!file.commit()) {
        setError(errorMessage, QString("Could not commit settings file: %1").arg(file.errorString()));
        return false;
    }

    return true;
}
