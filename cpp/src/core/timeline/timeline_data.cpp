/*
 * NC-KTV Core — Timeline Data Implementation
 * Port of timeline_data.py (443 lines)
 */

#include "timeline_data.h"

#include <QUuid>
#include <algorithm>

namespace ncktv {

static std::string qs(const QString& s) { return s.toStdString(); }
static QString fromStd(const std::string& s) { return QString::fromStdString(s); }

// ─── Enum String Conversions ─────────────────────────────────────────────────

QString trackTypeToString(TrackType t) {
    switch (t) {
        case TrackType::Audio:   return "audio";
        case TrackType::Video:   return "video";
        case TrackType::Effects: return "effects";
        case TrackType::Lyrics:  return "lyrics";
    }
    return "audio";
}

TrackType trackTypeFromString(const QString& s) {
    if (s == "video")   return TrackType::Video;
    if (s == "effects") return TrackType::Effects;
    if (s == "lyrics")  return TrackType::Lyrics;
    return TrackType::Audio;
}

QString effectTypeToString(EffectType t) {
    switch (t) {
        case EffectType::FadeIn:     return "fade_in";
        case EffectType::FadeOut:    return "fade_out";
        case EffectType::SlideLeft:  return "slide_left";
        case EffectType::SlideRight: return "slide_right";
        case EffectType::ZoomIn:     return "zoom_in";
        case EffectType::ZoomOut:    return "zoom_out";
        case EffectType::Blur:       return "blur";
        case EffectType::ColorShift: return "color_shift";
        case EffectType::Custom:     return "custom";
    }
    return "fade_in";
}

EffectType effectTypeFromString(const QString& s) {
    if (s == "fade_out")    return EffectType::FadeOut;
    if (s == "slide_left")  return EffectType::SlideLeft;
    if (s == "slide_right") return EffectType::SlideRight;
    if (s == "zoom_in")     return EffectType::ZoomIn;
    if (s == "zoom_out")    return EffectType::ZoomOut;
    if (s == "blur")        return EffectType::Blur;
    if (s == "color_shift") return EffectType::ColorShift;
    if (s == "custom")      return EffectType::Custom;
    return EffectType::FadeIn;
}

QString easingCurveToString(EasingCurve c) {
    switch (c) {
        case EasingCurve::Linear:       return "linear";
        case EasingCurve::EaseIn:       return "ease_in";
        case EasingCurve::EaseOut:      return "ease_out";
        case EasingCurve::EaseInOut:    return "ease_in_out";
        case EasingCurve::CustomBezier: return "custom_bezier";
    }
    return "linear";
}

EasingCurve easingCurveFromString(const QString& s) {
    if (s == "ease_in")       return EasingCurve::EaseIn;
    if (s == "ease_out")      return EasingCurve::EaseOut;
    if (s == "ease_in_out")   return EasingCurve::EaseInOut;
    if (s == "custom_bezier") return EasingCurve::CustomBezier;
    return EasingCurve::Linear;
}

// ─── Effect JSON ─────────────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const Effect& e) {
    j = {
        {"effect_type", qs(effectTypeToString(e.effectType))},
        {"start_time",  e.startTime},
        {"duration",    e.duration},
        {"easing",      qs(easingCurveToString(e.easing))},
        {"properties",  e.properties}
    };

    if (!e.customCurvePoints.isEmpty()) {
        auto pts = nlohmann::json::array();
        for (const auto& p : e.customCurvePoints)
            pts.push_back({p.first, p.second});
        j["custom_curve_points"] = pts;
    }
}

void from_json(const nlohmann::json& j, Effect& e) {
    e.effectType = effectTypeFromString(fromStd(j.at("effect_type").get<std::string>()));
    e.startTime  = j.at("start_time").get<double>();
    e.duration   = j.at("duration").get<double>();
    e.easing     = easingCurveFromString(fromStd(j.value("easing", "linear")));
    e.properties = j.value("properties", nlohmann::json::object());

    e.customCurvePoints.clear();
    if (j.contains("custom_curve_points")) {
        for (const auto& p : j["custom_curve_points"])
            e.customCurvePoints.append({p[0].get<double>(), p[1].get<double>()});
    }
}

// ─── Clip ────────────────────────────────────────────────────────────────────

void Clip::moveTo(double newStartTime) {
    startTime = std::max(0.0, newStartTime);
}

void Clip::resize(double newDuration, bool fromStart) {
    newDuration = std::max(0.01, newDuration);   // Minimum 10ms

    if (fromStart) {
        double oldEnd = endTime();
        double newStart = oldEnd - newDuration;
        if (newStart < 0.0) {
            newStart = 0.0;
            newDuration = oldEnd;
        }
        startTime = newStart;
    }
    duration = newDuration;
}

std::optional<Clip> Clip::splitAt(double time) {
    if (time <= startTime || time >= endTime())
        return std::nullopt;

    // Create right portion
    Clip right;
    right.clipId      = QUuid::createUuid().toString(QUuid::WithoutBraces);
    right.trackId     = trackId;
    right.startTime   = time;
    right.duration    = endTime() - time;
    right.sourceFile  = sourceFile;
    right.sourceStart = sourceStart + (time - startTime);
    right.sourceEnd   = sourceEnd;
    right.metadata    = metadata;

    // Adjust left portion (this clip)
    duration = time - startTime;

    // Split effects between left and right
    QVector<Effect> leftEffects, rightEffects;
    for (const auto& eff : effects) {
        double effAbsStart = startTime + eff.startTime;
        double effAbsEnd   = effAbsStart + eff.duration;

        if (effAbsEnd <= time) {
            leftEffects.append(eff);
        } else if (effAbsStart >= time) {
            Effect adjusted = eff;
            adjusted.startTime = eff.startTime - (time - startTime);
            rightEffects.append(adjusted);
        } else {
            // Effect spans the split — duplicate on both sides
            Effect leftEff  = eff;
            leftEff.duration = time - effAbsStart;
            leftEffects.append(leftEff);

            Effect rightEff  = eff;
            rightEff.startTime = 0.0;
            rightEff.duration  = effAbsEnd - time;
            rightEffects.append(rightEff);
        }
    }

    effects       = leftEffects;
    right.effects = rightEffects;

    return right;
}

void Clip::addEffect(const Effect& effect) {
    effects.append(effect);
}

bool Clip::removeEffect(int index) {
    if (index < 0 || index >= effects.size()) return false;
    effects.removeAt(index);
    return true;
}

QVector<const Effect*> Clip::getEffectsAtTime(double time) const {
    QVector<const Effect*> result;
    for (const auto& eff : effects) {
        if (eff.startTime <= time && time <= eff.endTime())
            result.append(&eff);
    }
    return result;
}

nlohmann::json Clip::toJson() const {
    nlohmann::json j = {
        {"clip_id",      qs(clipId)},
        {"track_id",     qs(trackId)},
        {"start_time",   startTime},
        {"duration",     duration},
        {"source_start", sourceStart},
        {"metadata",     metadata}
    };

    if (sourceFile.has_value())
        j["source_file"] = qs(sourceFile.value());
    if (sourceEnd.has_value())
        j["source_end"] = sourceEnd.value();

    auto effs = nlohmann::json::array();
    for (const auto& e : effects) {
        nlohmann::json ej;
        to_json(ej, e);
        effs.push_back(ej);
    }
    j["effects"] = effs;

    return j;
}

Clip Clip::fromJson(const nlohmann::json& j) {
    Clip c;
    c.clipId      = fromStd(j.at("clip_id").get<std::string>());
    c.trackId     = fromStd(j.at("track_id").get<std::string>());
    c.startTime   = j.at("start_time").get<double>();
    c.duration    = j.at("duration").get<double>();
    c.sourceStart = j.value("source_start", 0.0);
    c.metadata    = j.value("metadata", nlohmann::json::object());

    if (j.contains("source_file") && !j["source_file"].is_null())
        c.sourceFile = fromStd(j["source_file"].get<std::string>());
    if (j.contains("source_end") && !j["source_end"].is_null())
        c.sourceEnd = j["source_end"].get<double>();

    if (j.contains("effects")) {
        for (const auto& ej : j["effects"]) {
            Effect e;
            from_json(ej, e);
            c.effects.append(e);
        }
    }

    return c;
}

// ─── Track ───────────────────────────────────────────────────────────────────

void Track::addClip(const Clip& clip) {
    clips.append(clip);
    std::sort(clips.begin(), clips.end(),
              [](const Clip& a, const Clip& b) { return a.startTime < b.startTime; });
}

bool Track::removeClip(const QString& clipId) {
    auto it = std::find_if(clips.begin(), clips.end(),
                           [&](const Clip& c) { return c.clipId == clipId; });
    if (it == clips.end()) return false;
    clips.erase(it);
    return true;
}

Clip* Track::getClip(const QString& clipId) {
    for (auto& c : clips)
        if (c.clipId == clipId) return &c;
    return nullptr;
}

const Clip* Track::getClip(const QString& clipId) const {
    for (const auto& c : clips)
        if (c.clipId == clipId) return &c;
    return nullptr;
}

QVector<const Clip*> Track::getClipsAtTime(double time) const {
    QVector<const Clip*> result;
    for (const auto& c : clips) {
        if (c.startTime <= time && time < c.endTime())
            result.append(&c);
    }
    return result;
}

bool Track::checkOverlap(const Clip& clip, const QString& excludeClipId) const {
    for (const auto& existing : clips) {
        if (!excludeClipId.isEmpty() && existing.clipId == excludeClipId)
            continue;
        if (clip.startTime < existing.endTime() && clip.endTime() > existing.startTime)
            return true;
    }
    return false;
}

double Track::findNextFreeSlot(double duration, double afterTime) const {
    double candidate = afterTime;

    // Sort clips by start time (already sorted after addClip, but be safe)
    QVector<const Clip*> sorted;
    for (const auto& c : clips) sorted.append(&c);
    std::sort(sorted.begin(), sorted.end(),
              [](const Clip* a, const Clip* b) { return a->startTime < b->startTime; });

    for (const auto* c : sorted) {
        if (c->endTime() <= candidate) continue;   // clip is before our candidate
        if (candidate + duration <= c->startTime)
            return candidate;   // gap found
        candidate = c->endTime();   // skip past this clip
    }

    return candidate;
}

nlohmann::json Track::toJson() const {
    nlohmann::json j = {
        {"track_id",   qs(trackId)},
        {"track_type", qs(trackTypeToString(trackType))},
        {"name",       qs(name)},
        {"is_muted",   isMuted},
        {"is_solo",    isSolo},
        {"is_locked",  isLocked}
    };

    auto clipsArr = nlohmann::json::array();
    for (const auto& c : clips)
        clipsArr.push_back(c.toJson());
    j["clips"] = clipsArr;

    return j;
}

Track Track::fromJson(const nlohmann::json& j) {
    Track t;
    t.trackId   = fromStd(j.at("track_id").get<std::string>());
    t.trackType = trackTypeFromString(fromStd(j.at("track_type").get<std::string>()));
    t.name      = fromStd(j.at("name").get<std::string>());
    t.isMuted   = j.value("is_muted", false);
    t.isSolo    = j.value("is_solo", false);
    t.isLocked  = j.value("is_locked", false);

    if (j.contains("clips")) {
        for (const auto& cj : j["clips"])
            t.clips.append(Clip::fromJson(cj));
    }

    return t;
}

// ─── TimelineData ────────────────────────────────────────────────────────────

Track& TimelineData::addTrack(const QString& id, TrackType type, const QString& name) {
    Track t;
    t.trackId   = id;
    t.trackType = type;
    t.name      = name;
    tracks.append(t);
    return tracks.last();
}

bool TimelineData::removeTrack(const QString& trackId) {
    auto it = std::find_if(tracks.begin(), tracks.end(),
                           [&](const Track& t) { return t.trackId == trackId; });
    if (it == tracks.end()) return false;
    tracks.erase(it);
    return true;
}

Track* TimelineData::getTrack(const QString& trackId) {
    for (auto& t : tracks)
        if (t.trackId == trackId) return &t;
    return nullptr;
}

const Clip* TimelineData::getClipAtTime(double time) const {
    for (const auto& track : tracks) {
        if (track.isMuted) continue;
        auto clipsAtTime = track.getClipsAtTime(time);
        if (!clipsAtTime.isEmpty())
            return clipsAtTime.first();
    }
    return nullptr;
}

void TimelineData::recalculateDuration() {
    totalDuration = 0.0;
    for (const auto& track : tracks)
        for (const auto& clip : track.clips)
            totalDuration = std::max(totalDuration, clip.endTime());
}

nlohmann::json TimelineData::toJson() const {
    nlohmann::json j;
    j["total_duration"] = totalDuration;

    auto arr = nlohmann::json::array();
    for (const auto& t : tracks)
        arr.push_back(t.toJson());
    j["tracks"] = arr;

    return j;
}

TimelineData TimelineData::fromJson(const nlohmann::json& j) {
    TimelineData td;
    td.totalDuration = j.value("total_duration", 0.0);

    if (j.contains("tracks")) {
        for (const auto& tj : j["tracks"])
            td.tracks.append(Track::fromJson(tj));
    }

    return td;
}

} // namespace ncktv
