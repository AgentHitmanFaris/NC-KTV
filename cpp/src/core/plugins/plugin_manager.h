#pragma once
/*
 * NC-KTV Core — Plugin Manager
 * Port of plugin_manager.py — Discovers, loads, and manages plugins
 */

#include <QObject>
#include <QMap>
#include "plugin_base.h"

namespace ncktv {

class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* parent = nullptr);

    void discoverPlugins();
    bool loadPlugin(const QString& pluginId, const QString& pluginDir);
    bool unloadPlugin(const QString& pluginId);
    bool enablePlugin(const QString& pluginId);
    bool disablePlugin(const QString& pluginId);

    bool installPluginPackage(const QString& packagePath);
    bool uninstallPlugin(const QString& pluginId);

    QVector<PluginMetadata> getDiscoveredPlugins() const { return m_discovered; }
    PluginInterface* getPlugin(const QString& pluginId) const;
    QVector<PluginInterface*> getPluginsByType(PluginType type) const;

    void loadAllPlugins();

private:
    QMap<QString, PluginInterface*> m_loaded;
    QVector<PluginMetadata> m_discovered;
};

} // namespace ncktv
