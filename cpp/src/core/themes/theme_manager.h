#pragma once
#include <QObject>
#include <QString>
#include <QMap>
namespace ncktv {
class ThemeManager : public QObject {
    Q_OBJECT
public:
    explicit ThemeManager(QObject* parent = nullptr);
    QStringList availableThemes() const;
    QString currentTheme() const { return m_current; }
    bool applyTheme(const QString& themeId);
    bool installThemePackage(const QString& packagePath);
    bool uninstallTheme(const QString& themeId);
    QString getStyleSheet(const QString& themeId) const;
signals:
    void themeChanged(const QString& themeId);
private:
    QString m_current = "builtin-dark";
    QMap<QString, QString> m_themes;   // id → stylesheet content
};
} // namespace ncktv
