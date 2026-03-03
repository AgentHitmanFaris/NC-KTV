/*
 * NC-KTV Core — Config Manager Implementation
 */

#include "config_manager.h"

#include <QFile>
#include <QDir>
#include <fstream>

namespace ncktv {

ConfigManager::ConfigManager(const QString& configPath)
    : m_configPath(configPath)
{
    reload();
}

void ConfigManager::reload() {
    if (QFile::exists(m_configPath)) {
        try {
            m_root = YAML::LoadFile(m_configPath.toStdString());
        } catch (const YAML::Exception&) {
            m_root = YAML::Node(YAML::NodeType::Map);
        }
    } else {
        m_root = YAML::Node(YAML::NodeType::Map);
    }
}

void ConfigManager::save() {
    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    std::ofstream fout(m_configPath.toStdString());
    if (fout.is_open()) {
        fout << m_root;
    }
}

bool ConfigManager::contains(const QString& key) const {
    try {
        YAML::Node node = navigateTo(key);
        return node.IsDefined() && !node.IsNull();
    } catch (...) {
        return false;
    }
}

YAML::Node ConfigManager::navigateTo(const QString& key) const {
    QStringList parts = key.split('.');
    YAML::Node current = YAML::Clone(m_root);

    for (const auto& part : parts) {
        if (!current.IsMap())
            return YAML::Node();
        current = current[part.toStdString()];
    }

    return current;
}

YAML::Node& ConfigManager::navigateOrCreate(const QString& key) {
    QStringList parts = key.split('.');
    static YAML::Node dummy;   // fallback for single-key case

    if (parts.size() == 1) {
        return m_root[key.toStdString()];
    }

    // Navigate/create intermediate nodes
    YAML::Node* current = &m_root;
    for (int i = 0; i < parts.size() - 1; ++i) {
        std::string part = parts[i].toStdString();
        if (!(*current)[part].IsDefined())
            (*current)[part] = YAML::Node(YAML::NodeType::Map);
        current = &((*current)[part]);
    }

    std::string lastKey = parts.last().toStdString();
    return (*current)[lastKey];
}

} // namespace ncktv
