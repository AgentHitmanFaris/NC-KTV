#pragma once
/*
 * NC-KTV Core — Timeline Data Structures
 * Port of timeline_data.py
 *
 * Enums: TrackType, EffectType, EasingCurve
 * Structs: Effect, Clip, Track, TimelineData
 */

#include <QString>
#include <QVector>
#include <optional>
#include <nlohmann/json.hpp>

namespace ncktv {

// ─── Enums ──────────────────────────────────────────────────────────────────

enum class TrackType {
    Audio,
    Video,
    Effects,
    Lyrics
};

enum class EffectType {
    FadeIn,
    FadeOut,
    SlideLeft,
    SlideRight,
    ZoomIn,
    ZoomOut,
    Blur,
    ColorShift,
    Custom
};

enum class EasingCurve {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    CustomBezier
};

// String conversion helpers
QString       trackTypeToString(TrackType t);
TrackType     trackTypeFromString(const QString& s);
QString       effectTypeToString(EffectType t);
EffectType    effectTypeFromString(const QString& s);
QString       easingCurveToString(EasingCurve c);
EasingCurve   easingCurveFromString(const QString& s);

// ─── Effect ─────────────────────────────────────────────────────────────────

struct Effect {
    EffectType  effectType = EffectType::FadeIn;
    double      startTime  = 0.0;
    double      duration   = 1.0;
    EasingCurve easing     = EasingCurve::Linear;

    // Optional custom Bezier control points: {x, y} pairs
    QVector<QPair<double, double>> customCurvePoints;

    // Effect-specific properties (e.g., slide distance, blur radius)
    nlohmann::json properties = nlohmann::json::object();

    [[nodiscard]] double endTime() const { return startTime + duration; }
};

void to_json(nlohmann::json& j, const Effect& e);
void from_json(const nlohmann::json& j, Effect& e);

// ─── Clip ───────────────────────────────────────────────────────────────────

class Clip {
public:
    QString clipId;
    QString trackId;
    double  startTime   = 0.0;
    double  duration    = 0.0;

    std::optional<QString> sourceFile;
    double  sourceStart = 0.0;
    std::optional<double>  sourceEnd;

    QVector<Effect> effects;
    nlohmann::json  metadata = nlohmann::json::object();

    // ── Computed ─────────────────────────────────────────────────────────
    [[nodiscard]] double endTime() const { return startTime + duration; }

    // ── Manipulation ─────────────────────────────────────────────────────
    void moveTo(double newStartTime);
    void resize(double newDuration, bool fromStart = false);

    /// Split clip at specified time. Returns the right portion (new clip), or nullopt if invalid.
    std::optional<Clip> splitAt(double time);

    // ── Effects ──────────────────────────────────────────────────────────
    void addEffect(const Effect& effect);
    bool removeEffect(int index);
    [[nodiscard]] QVector<const Effect*> getEffectsAtTime(double time) const;

    // ── Serialization ────────────────────────────────────────────────────
    [[nodiscard]] nlohmann::json toJson() const;
    static Clip fromJson(const nlohmann::json& j);
};

// ─── Track ──────────────────────────────────────────────────────────────────

class Track {
public:
    QString         trackId;
    TrackType       trackType = TrackType::Audio;
    QString         name;
    QVector<Clip>   clips;

    bool isMuted  = false;
    bool isSolo   = false;
    bool isLocked = false;

    // ── Clip management ──────────────────────────────────────────────────
    void        addClip(const Clip& clip);
    bool        removeClip(const QString& clipId);
    Clip*       getClip(const QString& clipId);
    const Clip* getClip(const QString& clipId) const;
    QVector<const Clip*> getClipsAtTime(double time) const;

    /// Check if a clip would overlap with existing clips
    bool checkOverlap(const Clip& clip, const QString& excludeClipId = {}) const;

    /// Find the next free slot of given duration after a given time
    double findNextFreeSlot(double duration, double afterTime = 0.0) const;

    // ── Serialization ────────────────────────────────────────────────────
    [[nodiscard]] nlohmann::json toJson() const;
    static Track fromJson(const nlohmann::json& j);
};

// ─── TimelineData ───────────────────────────────────────────────────────────

class TimelineData {
public:
    QVector<Track> tracks;
    double totalDuration = 0.0;

    // ── Track management ─────────────────────────────────────────────────
    Track& addTrack(const QString& id, TrackType type, const QString& name);
    bool   removeTrack(const QString& trackId);
    Track* getTrack(const QString& trackId);

    /// Get the clip (and its track) at a given time across all tracks
    const Clip* getClipAtTime(double time) const;

    /// Recalculate total duration from all clips
    void recalculateDuration();

    // ── Serialization ────────────────────────────────────────────────────
    [[nodiscard]] nlohmann::json toJson() const;
    static TimelineData fromJson(const nlohmann::json& j);
};

} // namespace ncktv
