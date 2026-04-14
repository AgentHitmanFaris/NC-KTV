/*
 * NC-KTV Core — Plugin Manager Implementation (Stub)
 * Full implementation will use QPluginLoader for .nckplugin packages
 */

#include "plugin_manager.h"
#include <QDir>
#include <QPluginLoader>
#include <QRegularExpression>

namespace ncktv {

PluginManager::PluginManager(QObject* parent) : QObject(parent) {}

void PluginManager::discoverPlugins() {
    m_discovered.clear();
    QDir pluginDir("plugins/installed");
    if (!pluginDir.exists()) return;

    for (const auto& entry : pluginDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString manifestPath = entry.absoluteFilePath() + "/manifest.json";
        if (QFile::exists(manifestPath)) {
            // TODO: Parse manifest and add to m_discovered
        }
    }
}

bool PluginManager::loadPlugin(const QString& /*pluginId*/, const QString& /*pluginDir*/) {
    // TODO: Use QPluginLoader to load shared library
    return false;
}

bool PluginManager::unloadPlugin(const QString& pluginId) {
    if (!m_loaded.contains(pluginId)) return false;
    auto* plugin = m_loaded.take(pluginId);
    plugin->shutdown();
    return true;
}

bool PluginManager::enablePlugin(const QString& pluginId) {
    if (auto* p = getPlugin(pluginId)) { p->setEnabled(true); return true; }
    return false;
}

bool PluginManager::disablePlugin(const QString& pluginId) {
    if (auto* p = getPlugin(pluginId)) { p->setEnabled(false); return true; }
    return false;
}

bool PluginManager::installPluginPackage(const QString& /*packagePath*/) {
    // TODO: Extract .nckplugin ZIP, validate, install
    return false;
}

bool PluginManager::uninstallPlugin(const QString& pluginId) {
    if (pluginId.isEmpty()) return false;

    // Sanitization: Only allow alphanumeric, underscores, and dashes
    QString safeId = pluginId;
    safeId.remove(QRegularExpression("[^a-zA-Z0-9_\\-]"));
    if (safeId.isEmpty()) return false;

    // 1. Unload if active
    unloadPlugin(safeId);

    // 2. Remove directory
    QDir pluginDir("plugins/installed/" + safeId);
    if (!pluginDir.exists()) return false;

    if (!pluginDir.removeRecursively()) {
        return false;
    }

    // 3. Remove from discovered list
    for (auto it = m_discovered.begin(); it != m_discovered.end(); ++it) {
        if (it->id == safeId) {
            m_discovered.erase(it);
            break;
        }
    }

    return true;
}

PluginInterface* PluginManager::getPlugin(const QString& pluginId) const {
    return m_loaded.value(pluginId, nullptr);
}

QVector<PluginInterface*> PluginManager::getPluginsByType(PluginType type) const {
    QVector<PluginInterface*> result;
    for (auto* p : m_loaded) {
        if (p->metadata().type == type && p->isEnabled())
            result.append(p);
    }
    return result;
}

void PluginManager::loadAllPlugins() {
    discoverPlugins();
    // TODO: Load each discovered plugin
}

} // namespace ncktv
