#pragma once

#include <QString>

class ThemeManager {
public:
    enum class Theme {
        Aurora,
        Graphite,
        Clearwater,
    };

    static Theme fromName(const QString &name);
    static QString name(Theme theme);
    static QString displayName(Theme theme);
    static QString styleSheet(Theme theme);
};
