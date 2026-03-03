#pragma once
/*
 * NC-KTV Core — Plugin Base Interface
 * Port of plugin_base.py
 */

#include <QString>
#include <QVector>
#include <nlohmann/json.hpp>
#include <QtPlugin>

namespace ncktv {

enum class PluginType { Effect, ExportTemplate, UIExtension };
enum class PluginPermission { FileAccess, NetworkAccess, SystemAccess };

struct PluginMetadata {
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;
    PluginType type = PluginType::Effect;
    QVector<PluginPermission> permissions;
};

/// Abstract base interface for all NC-KTV plugins
class PluginInterface {
public:
    virtual ~PluginInterface() = default;
    virtual PluginMetadata metadata() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
};

} // namespace ncktv

Q_DECLARE_INTERFACE(ncktv::PluginInterface, "com.ncktv.PluginInterface/1.0")
