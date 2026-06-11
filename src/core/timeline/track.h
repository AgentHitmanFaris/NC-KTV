#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "clip.h"
#include <nlohmann/json.hpp>

namespace ncktv {

class Track : public QObject {
    Q_OBJECT
    
    Q_PROPERTY(QString trackId READ trackId CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int trackType READ trackType CONSTANT)
    Q_PROPERTY(bool isMuted READ isMuted WRITE setIsMuted NOTIFY isMutedChanged)
    Q_PROPERTY(bool isSolo READ isSolo WRITE setIsSolo NOTIFY isSoloChanged)
    Q_PROPERTY(bool isLocked READ isLocked WRITE setIsLocked NOTIFY isLockedChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    enum Type {
        Audio = 0,
        Video = 1,
        Lyrics = 2
    };
    Q_ENUM(Type)

    explicit Track(QObject* parent = nullptr);
    Track(const QString& id, Type type, const QString& name, QObject* parent = nullptr);
    virtual ~Track() override;

    // Getters
    [[nodiscard]] QString trackId() const { return m_trackId; }
    [[nodiscard]] QString name() const { return m_name; }
    [[nodiscard]] int trackType() const { return static_cast<int>(m_trackType); }
    [[nodiscard]] Type type() const { return m_trackType; }
    [[nodiscard]] bool isMuted() const { return m_isMuted; }
    [[nodiscard]] bool isSolo() const { return m_isSolo; }
    [[nodiscard]] bool isLocked() const { return m_isLocked; }
    [[nodiscard]] float volume() const { return m_volume; }
    [[nodiscard]] Q_INVOKABLE QList<Clip*> clips() const { return m_clips; }

    // Setters
    void setName(const QString& name);
    void setIsMuted(bool mute);
    void setIsSolo(bool solo);
    void setIsLocked(bool lock);
    void setVolume(float volume);

    // Clip Management
    Q_INVOKABLE bool addClip(Clip* clip);
    Q_INVOKABLE bool removeClip(const QString& clipId);
    Q_INVOKABLE void clearClips();
    Q_INVOKABLE Clip* getClip(const QString& clipId) const;
    Q_INVOKABLE QList<Clip*> getClipsAtTime(qint64 timeMicroseconds) const;
    
    // Gap and overlap validation calculations
    Q_INVOKABLE bool checkOverlap(qint64 startMicroseconds, qint64 durationMicroseconds, const QString& excludeClipId = {}) const;
    Q_INVOKABLE qint64 findNextFreeSlot(qint64 durationMicroseconds, qint64 afterTimeMicroseconds = 0) const;

    // Serialization
    [[nodiscard]] nlohmann::json toJson() const;
    static Track* fromJson(const nlohmann::json& j, QObject* parent = nullptr);

signals:
    void nameChanged();
    void isMutedChanged();
    void isSoloChanged();
    void isLockedChanged();
    void volumeChanged();
    
    // Emitted when clips are added/removed internally (allows list model updates)
    void clipsChanged();
    void clipAdded(Clip* clip);
    void clipRemoved(const QString& clipId);

private:
    QString m_trackId;
    Type m_trackType = Audio;
    QString m_name;
    QList<Clip*> m_clips;

    bool m_isMuted = false;
    bool m_isSolo = false;
    bool m_isLocked = false;
    float m_volume = 1.0f; // Defaults to full volume (100%)
};

} // namespace ncktv
