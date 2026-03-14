#pragma once

#include <QString>
#include <QVariant>
#include <QSettings>

namespace ncktv {

class ConfigManager {
public:
    explicit ConfigManager(const QString& configPath = "config.ini") 
        : m_settings(configPath, QSettings::IniFormat) 
    {}

    template<typename T>
    T get(const QString& key, const T& defaultValue = T{}) const {
        return m_settings.value(key, QVariant::fromValue(defaultValue)).template value<T>();
    }

    template<typename T>
    void set(const QString& key, const T& value) {
        m_settings.setValue(key, QVariant::fromValue(value));
    }

    void save() { m_settings.sync(); }
    void reload() { m_settings.sync(); }
    bool contains(const QString& key) const { return m_settings.contains(key); }

private:
    QSettings m_settings;
};

// Specialized template for QString to avoid QVariant issues in some Qt versions
template<>
inline QString ConfigManager::get<QString>(const QString& key, const QString& defaultValue) const {
    return m_settings.value(key, defaultValue).toString();
}

template<>
inline void ConfigManager::set<QString>(const QString& key, const QString& value) {
    m_settings.setValue(key, value);
}

} // namespace ncktv
