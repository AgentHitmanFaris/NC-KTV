#pragma once

#include <QString>
#include <QVariant>
#include <QVariantMap>

namespace ncktv {

class ConfigManager {
public:
    explicit ConfigManager(const QString& configPath = "config.yaml") { Q_UNUSED(configPath); }

    template<typename T>
    T get(const QString& key, const T& defaultValue = T{}) const { Q_UNUSED(key); return defaultValue; }

    template<typename T>
    void set(const QString& key, const T& value) { Q_UNUSED(key); Q_UNUSED(value); }

    void save() {}
    void reload() {}
    bool contains(const QString& /*key*/) const { return false; }
};

template<>
inline QString ConfigManager::get<QString>(const QString& key, const QString& defaultValue) const {
    Q_UNUSED(key);
    return defaultValue;
}

template<>
inline void ConfigManager::set<QString>(const QString& key, const QString& value) {
    Q_UNUSED(key);
    Q_UNUSED(value);
}

} // namespace ncktv
