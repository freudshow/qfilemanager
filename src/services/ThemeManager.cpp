#include "services/ThemeManager.h"

namespace {

QString buildStyleSheet(const QString &windowBackground,
                        const QString &surface,
                        const QString &surfaceRaised,
                        const QString &border,
                        const QString &text,
                        const QString &mutedText,
                        const QString &accent,
                        const QString &accentText,
                        const QString &selection) {
    return QStringLiteral(
        "QMainWindow, QWidget { background: %1; color: %5; }"
        "QToolBar#mainToolbar { background: %2; border: none; border-bottom: 1px solid %4; spacing: 6px; padding: 7px 10px; }"
        "QToolBar#mainToolbar QToolButton { background: transparent; border: 1px solid transparent; border-radius: 8px; padding: 6px 10px; color: %5; }"
        "QToolBar#mainToolbar QToolButton:hover { background: %3; border-color: %4; }"
        "QToolBar#mainToolbar QToolButton:checked { background: %8; color: %7; }"
        "QToolButton#themesToolButton { background: %7; color: %8; border: none; font-weight: 600; padding: 7px 14px; }"
        "QToolButton#themesToolButton:hover { background: %8; color: %7; }"
        "QWidget#addressBarContainer { background: %2; border: 1px solid %4; border-radius: 10px; }"
        "QLineEdit#addressBar { background: %1; border: 1px solid %4; border-radius: 8px; padding: 6px 10px; color: %5; }"
        "QToolButton#upButton, QToolButton#refreshButton { background: %3; border: 1px solid %4; border-radius: 8px; padding: 5px 10px; color: %5; }"
        "QToolButton#upButton:hover, QToolButton#refreshButton:hover { background: %8; color: %7; }"
        "QListView#favoritesListView { background: transparent; border: none; outline: none; }"
        "QListView#favoritesListView::item { margin: 3px 0; padding: 7px 10px; border-radius: 8px; color: %7; }"
        "QListView#favoritesListView::item:hover { background: %3; }"
        "QListView#favoritesListView::item:selected { background: %8; color: %7; }"
        "QTableView, QListView { background: %1; alternate-background-color: %2; border: 1px solid %4; border-radius: 10px; gridline-color: %4; outline: none; }"
        "QTableView::item, QListView::item { padding: 5px; }"
        "QTableView::item:selected, QListView::item:selected { background: %9; color: %5; }"
        "QHeaderView::section { background: %2; color: %5; border: none; border-bottom: 1px solid %4; padding: 7px; font-weight: 600; }"
        "QTabWidget::pane { background: %1; border: 1px solid %4; border-radius: 10px; }"
        "QTabBar::tab { background: %2; color: %6; padding: 7px 14px; margin-right: 3px; border: 1px solid %4; border-bottom: none; border-radius: 8px 8px 0 0; }"
        "QTabBar::tab:selected { background: %1; color: %5; }"
        "QLabel#favoritesSidebarLabel, QLabel#metadataPanelLabel, QLabel#tabStripLabel { color: %5; font-weight: 700; }"
        "QMenu { background: %2; color: %5; border: 1px solid %4; padding: 5px; }"
        "QMenu::item { padding: 7px 28px 7px 10px; border-radius: 6px; }"
        "QMenu::item:selected { background: %8; color: %7; }"
        "QMenu::separator { height: 1px; background: %4; margin: 5px 8px; }"
        "QSplitter::handle { background: %4; }"
        "QMessageBox, QInputDialog { background: %2; }")
        .arg(windowBackground, surface, surfaceRaised, border, text, mutedText, accent, accentText, selection);
}

} // namespace

ThemeManager::Theme ThemeManager::fromName(const QString &name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("graphite")) {
        return Theme::Graphite;
    }
    if (normalized == QStringLiteral("clearwater")) {
        return Theme::Clearwater;
    }
    return Theme::Aurora;
}

QString ThemeManager::name(Theme theme) {
    switch (theme) {
    case Theme::Graphite:
        return QStringLiteral("graphite");
    case Theme::Clearwater:
        return QStringLiteral("clearwater");
    case Theme::Aurora:
    default:
        return QStringLiteral("aurora");
    }
}

QString ThemeManager::displayName(Theme theme) {
    switch (theme) {
    case Theme::Graphite:
        return QStringLiteral("Graphite Ember");
    case Theme::Clearwater:
        return QStringLiteral("Clearwater");
    case Theme::Aurora:
    default:
        return QStringLiteral("Aurora Garden");
    }
}

QString ThemeManager::styleSheet(Theme theme) {
    switch (theme) {
    case Theme::Graphite:
        return buildStyleSheet(QStringLiteral("#20252b"), QStringLiteral("#2c333b"), QStringLiteral("#3b4650"),
                               QStringLiteral("#4d5962"), QStringLiteral("#f1e9dc"), QStringLiteral("#c5cbd0"),
                               QStringLiteral("#e0b777"), QStringLiteral("#27231f"), QStringLiteral("#8a6b3c"));
    case Theme::Clearwater:
        return buildStyleSheet(QStringLiteral("#f4f8fb"), QStringLiteral("#ffffff"), QStringLiteral("#e6f0f6"), QStringLiteral("#c6d7e4"),
                               QStringLiteral("#203342"), QStringLiteral("#647889"), QStringLiteral("#267a9b"), QStringLiteral("#e2f4fa"), QStringLiteral("#b9dce9"));
    case Theme::Aurora:
    default:
        return buildStyleSheet(QStringLiteral("#edf4f2"), QStringLiteral("#ffffff"), QStringLiteral("#e5f2ee"), QStringLiteral("#c9ded8"),
                               QStringLiteral("#183832"), QStringLiteral("#6b817b"), QStringLiteral("#d38a55"), QStringLiteral("#d6eee7"), QStringLiteral("#b8dfd3"));
    }
}
