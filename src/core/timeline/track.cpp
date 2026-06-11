#include "track.h"
#include <QUuid>
#include <algorithm>

namespace ncktv {

Track::Track(QObject* parent)
    : QObject(parent),
      m_trackId(QUuid::createUuid().toString(QUuid::Id128)),
      m_trackType(Audio),
      m_name(QStringLiteral("New Track")),
      m_volume(1.0f) {
}

Track::Track(const QString& id, Type type, const QString& name, QObject* parent)
    : QObject(parent),
      m_trackId(id.isEmpty() ? QUuid::createUuid().toString(QUuid::Id128) : id),
      m_trackType(type),
      m_name(name),
      m_volume(1.0f) {
}

Track::~Track() {
    qDeleteAll(m_clips);
    m_clips.clear();
}

void Track::setName(const QString& name) {
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

void Track::setIsMuted(bool mute) {
    if (m_isMuted != mute) {
        m_isMuted = mute;
        emit isMutedChanged();
    }
}

void Track::setIsSolo(bool solo) {
    if (m_isSolo != solo) {
        m_isSolo = solo;
        emit isSoloChanged();
    }
}

void Track::setIsLocked(bool lock) {
    if (m_isLocked != lock) {
        m_isLocked = lock;
        emit isLockedChanged();
    }
}

void Track::setVolume(float volume) {
    float clamped = std::clamp(volume, 0.0f, 1.0f);
    if (m_volume != clamped) {
        m_volume = clamped;
        emit volumeChanged();
    }
}

bool Track::addClip(Clip* clip) {
    if (!clip || m_isLocked) {
        return false;
    }

    if (m_trackType != Lyrics && checkOverlap(clip->startTime(), clip->duration(), clip->clipId())) {
        return false;
    }

    clip->setParent(this);
    clip->setTrackId(m_trackId);

    auto it = std::upper_bound(m_clips.begin(), m_clips.end(), clip, [](const Clip* a, const Clip* b) {
        return a->startTime() < b->startTime();
    });
    
    m_clips.insert(it, clip);

    connect(clip, &Clip::startTimeChanged, this, [this]() {
        std::stable_sort(m_clips.begin(), m_clips.end(), [](const Clip* a, const Clip* b) {
            return a->startTime() < b->startTime();
        });
        emit clipsChanged();
    });
    connect(clip, &Clip::durationChanged, this, &Track::clipsChanged);
    connect(clip, &Clip::sourceFileChanged, this, &Track::clipsChanged);
    connect(clip, &Clip::lyricTextChanged, this, &Track::clipsChanged);
    connect(clip, &Clip::syllablesChanged, this, &Track::clipsChanged);

    emit clipAdded(clip);
    emit clipsChanged();
    return true;
}

bool Track::removeClip(const QString& clipId) {
    if (m_isLocked) {
        return false;
    }

    for (int i = 0; i < m_clips.size(); ++i) {
        if (m_clips[i]->clipId() == clipId) {
            Clip* clip = m_clips[i];
            m_clips.removeAt(i);
            
            clip->disconnect(this);
            
            emit clipRemoved(clipId);
            emit clipsChanged();
            
            clip->deleteLater();
            return true;
        }
    }
    return false;
}

void Track::clearClips() {
    if (m_isLocked || m_clips.isEmpty()) {
        return;
    }

    QList<Clip*> tempClips = m_clips;
    m_clips.clear();

    for (Clip* clip : tempClips) {
        clip->disconnect(this);
        clip->deleteLater();
    }

    emit clipsChanged();
}

Clip* Track::getClip(const QString& clipId) const {
    for (Clip* clip : m_clips) {
        if (clip->clipId() == clipId) {
            return clip;
        }
    }
    return nullptr;
}

QList<Clip*> Track::getClipsAtTime(qint64 timeMicroseconds) const {
    QList<Clip*> result;
    for (Clip* clip : m_clips) {
        if (timeMicroseconds >= clip->startTime() && timeMicroseconds <= clip->endTime()) {
            result.append(clip);
        }
    }
    return result;
}

bool Track::checkOverlap(qint64 startMicroseconds, qint64 durationMicroseconds, const QString& excludeClipId) const {
    qint64 endMicroseconds = startMicroseconds + durationMicroseconds;
    for (const Clip* clip : m_clips) {
        if (!excludeClipId.isEmpty() && clip->clipId() == excludeClipId) {
            continue;
        }
        
        qint64 clipStart = clip->startTime();
        qint64 clipEnd = clip->endTime();

        if (startMicroseconds < clipEnd && clipStart < endMicroseconds) {
            return true;
        }
    }
    return false;
}

qint64 Track::findNextFreeSlot(qint64 durationMicroseconds, qint64 afterTimeMicroseconds) const {
    if (durationMicroseconds <= 0) {
        return afterTimeMicroseconds;
    }

    qint64 candidateStart = afterTimeMicroseconds;
    
    for (const Clip* clip : m_clips) {
        qint64 clipStart = clip->startTime();
        qint64 clipEnd = clip->endTime();

        if (clipEnd <= candidateStart) {
            continue;
        }

        if (clipStart >= candidateStart + durationMicroseconds) {
            return candidateStart;
        }

        candidateStart = std::max(candidateStart, clipEnd);
    }

    return candidateStart;
}

nlohmann::json Track::toJson() const {
    nlohmann::json j = nlohmann::json::object();
    j["trackId"] = m_trackId.toStdString();
    j["trackType"] = static_cast<int>(m_trackType);
    j["name"] = m_name.toStdString();
    j["isMuted"] = m_isMuted;
    j["isSolo"] = m_isSolo;
    j["isLocked"] = m_isLocked;
    j["volume"] = m_volume;

    nlohmann::json clipsJson = nlohmann::json::array();
    for (const Clip* clip : m_clips) {
        clipsJson.push_back(clip->toJson());
    }
    j["clips"] = clipsJson;

    return j;
}

Track* Track::fromJson(const nlohmann::json& j, QObject* parent) {
    QString id = QString::fromStdString(j.value("trackId", ""));
    int typeVal = j.value("trackType", 0);
    QString name = QString::fromStdString(j.value("name", ""));

    Track* track = new Track(id, static_cast<Type>(typeVal), name, parent);
    track->setIsMuted(j.value("isMuted", false));
    track->setIsSolo(j.value("isSolo", false));
    track->setIsLocked(j.value("isLocked", false));
    track->setVolume(j.value("volume", 1.0f));

    if (j.contains("clips") && j["clips"].is_array()) {
        for (const auto& clipJson : j["clips"]) {
            Clip* clip = Clip::fromJson(clipJson, track);
            if (clip) {
                clip->setTrackId(track->trackId());
                track->m_clips.append(clip);
            }
        }
        
        std::stable_sort(track->m_clips.begin(), track->m_clips.end(), [](const Clip* a, const Clip* b) {
            return a->startTime() < b->startTime();
        });
        
        for (Clip* clip : track->m_clips) {
            connect(clip, &Clip::startTimeChanged, track, [track]() {
                std::stable_sort(track->m_clips.begin(), track->m_clips.end(), [](const Clip* a, const Clip* b) {
                    return a->startTime() < b->startTime();
                });
                emit track->clipsChanged();
            });
            connect(clip, &Clip::durationChanged, track, &Track::clipsChanged);
            connect(clip, &Clip::sourceFileChanged, track, &Track::clipsChanged);
            connect(clip, &Clip::lyricTextChanged, track, &Track::clipsChanged);
            connect(clip, &Clip::syllablesChanged, track, &Track::clipsChanged);
        }
    }

    return track;
}

} // namespace ncktv
