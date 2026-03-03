#include "theme_manager.h"
#include <QDir>
#include <QFile>
namespace ncktv {
ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    // Load built-in dark theme
    QFile f(":/styles/dark_theme.qss");
    if (f.open(QIODevice::ReadOnly)) { m_themes["builtin-dark"] = f.readAll(); }
}
QStringList ThemeManager::availableThemes() const { return m_themes.keys(); }
bool ThemeManager::applyTheme(const QString& themeId) {
    if (!m_themes.contains(themeId)) return false;
    m_current = themeId;
    emit themeChanged(themeId);
    return true;
}
bool ThemeManager::installThemePackage(const QString&) { return false; /* TODO */ }
bool ThemeManager::uninstallTheme(const QString&) { return false; /* TODO */ }
QString ThemeManager::getStyleSheet(const QString& themeId) const {
    return m_themes.value(themeId);
}
} // namespace ncktv
