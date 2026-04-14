/*
 * NC-KTV Core — Project Implementation
 * Port of project.py
 */

#include "project.h"
#include "project/nctv_format.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

namespace ncktv {

static std::string qs(const QString& s) { return s.toStdString(); }
static QString fromStd(const std::string& s) { return QString::fromStdString(s); }

// ─── ProjectSettings ─────────────────────────────────────────────────────────

nlohmann::json ProjectSettings::toJson() const {
    return {
        {"uvr_model",      qs(uvrModel)},
        {"use_gpu",        useGpu},
        {"sample_rate",    sampleRate},
        {"karaoke_style",  qs(karaokeStyle)},
        {"output_format",  qs(outputFormat)},
        {"transcription_lang", qs(transcriptionLang)}
    };
}

ProjectSettings ProjectSettings::fromJson(const nlohmann::json& j) {
    ProjectSettings s;
    if (j.contains("uvr_model"))     s.uvrModel     = fromStd(j["uvr_model"].get<std::string>());
    if (j.contains("use_gpu"))       s.useGpu       = j["use_gpu"].get<bool>();
    if (j.contains("sample_rate"))   s.sampleRate   = j["sample_rate"].get<int>();
    if (j.contains("karaoke_style")) s.karaokeStyle = fromStd(j["karaoke_style"].get<std::string>());
    if (j.contains("output_format")) s.outputFormat = fromStd(j["output_format"].get<std::string>());
    if (j.contains("transcription_lang")) s.transcriptionLang = fromStd(j["transcription_lang"].get<std::string>());
    return s;
}

// ─── Project ─────────────────────────────────────────────────────────────────

Project::Project(const QString& source) {
    m_createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_version   = PROJECT_VERSION;

    if (!source.isEmpty()) {
        sourceFile = source;
        m_projectName = QFileInfo(source).completeBaseName();
    } else {
        m_projectName = "Untitled";
    }
}

nlohmann::json Project::toJson() const {
    nlohmann::json j = {
        {"version",      qs(m_version)},
        {"project_name", qs(m_projectName)},
        {"created_at",   qs(m_createdAt)},
        {"settings",     settings.toJson()},
        {"lyrics",       lyrics.toJson()},
        {"timeline",     timeline.toJson()},
        {"audio_clock",  audioClock.toJson()}
    };

    if (sourceFile.has_value())       j["source_file"]       = qs(sourceFile.value());
    if (instrumentalPath.has_value()) j["instrumental_path"] = qs(instrumentalPath.value());
    if (vocalsPath.has_value())       j["vocals_path"]       = qs(vocalsPath.value());
    if (originalAudioPath.has_value())j["original_audio_path"]= qs(originalAudioPath.value());
    if (transcriptionJson.has_value()) j["transcription_json"] = qs(transcriptionJson.value());
    if (rawLyrics.has_value()) j["raw_lyrics"] = qs(rawLyrics.value());

    return j;
}

Project Project::fromJson(const nlohmann::json& j) {
    Project p;
    p.m_version     = fromStd(j.value("version", PROJECT_VERSION));
    p.m_projectName = fromStd(j.value("project_name", "Untitled"));
    p.m_createdAt   = fromStd(j.value("created_at", ""));

    if (j.contains("settings"))    p.settings   = ProjectSettings::fromJson(j["settings"]);
    if (j.contains("lyrics"))      p.lyrics     = LyricsData::fromJson(j["lyrics"]);
    if (j.contains("timeline"))    p.timeline   = TimelineData::fromJson(j["timeline"]);
    if (j.contains("audio_clock")) p.audioClock = AudioClock::fromJson(j["audio_clock"]);

    if (j.contains("source_file") && !j["source_file"].is_null())
        p.sourceFile = fromStd(j["source_file"].get<std::string>());
    if (j.contains("instrumental_path") && !j["instrumental_path"].is_null())
        p.instrumentalPath = fromStd(j["instrumental_path"].get<std::string>());
    if (j.contains("vocals_path") && !j["vocals_path"].is_null())
        p.vocalsPath = fromStd(j["vocals_path"].get<std::string>());
    if (j.contains("original_audio_path") && !j["original_audio_path"].is_null())
        p.originalAudioPath = fromStd(j["original_audio_path"].get<std::string>());
    if (j.contains("transcription_json") && !j["transcription_json"].is_null())
        p.transcriptionJson = fromStd(j["transcription_json"].get<std::string>());
    if (j.contains("raw_lyrics") && !j["raw_lyrics"].is_null())
        p.rawLyrics = fromStd(j["raw_lyrics"].get<std::string>());

    return p;
}

void Project::save(const QString& filePath) {
    NCTVFormat::pack(*this, filePath);
    projectFilePath = filePath;
    isDirty = false;
}

Project Project::load(const QString& filePath) {
    Project p = NCTVFormat::unpack(filePath);
    p.projectFilePath = filePath;
    p.isDirty = false;
    return p;
}

QString Project::getTempDir() const {
    QString dir = QDir::tempPath() + "/ncktv/" + m_projectName;
    QDir().mkpath(dir);
    return dir;
}

void Project::cleanupTempFiles() {
    QDir tempDir(getTempDir());
    if (tempDir.exists())
        tempDir.removeRecursively();
}

Project::ValidationResult Project::validate() const {
    ValidationResult result;

    if (m_projectName.isEmpty()) {
        result.isValid = false;
        result.errorMessage = "Project name is empty";
        return result;
    }

    if (!sourceFile.has_value() || sourceFile.value().isEmpty()) {
        result.isValid = false;
        result.errorMessage = "No source file specified";
        return result;
    }

    if (!QFile::exists(sourceFile.value())) {
        result.isValid = false;
        result.errorMessage = "Source file does not exist: " + sourceFile.value();
        return result;
    }

    return result;
}

} // namespace ncktv
