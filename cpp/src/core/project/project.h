#pragma once
/*
 * NC-KTV Core — Project Management
 * Port of project.py
 */

#include <QString>
#include <QDir>
#include <optional>
#include <nlohmann/json.hpp>

#include "lyrics/lyrics_data.h"
#include "timeline/timeline_data.h"
#include "audio/audio_clock.h"

namespace ncktv {

// ─── Project Settings ────────────────────────────────────────────────────────

struct ProjectSettings {
    QString uvrModel      = "UVR_MDXNET_KARA_2.onnx";
    bool    useGpu        = true;
    int     sampleRate    = 44100;
    QString karaokeStyle  = "classic";
    QString outputFormat  = "mp4";

    nlohmann::json toJson() const;
    static ProjectSettings fromJson(const nlohmann::json& j);
};

// ─── Project ─────────────────────────────────────────────────────────────────

class Project {
public:
    static constexpr const char* PROJECT_VERSION = "0.7";
    static constexpr const char* PROJECT_EXT     = ".nctv";

    explicit Project(const QString& sourceFile = {});

    // ── Accessors ────────────────────────────────────────────────────────
    QString projectName() const { return m_projectName; }
    void    setProjectName(const QString& name) { m_projectName = name; }

    // Source file paths
    std::optional<QString> sourceFile;
    std::optional<QString> instrumentalPath;
    std::optional<QString> vocalsPath;
    std::optional<QString> originalAudioPath;

    // Sub-objects
    LyricsData     lyrics;
    TimelineData   timeline;
    AudioClock     audioClock;
    ProjectSettings settings;

    // Project file path (where it was saved/loaded)
    std::optional<QString> projectFilePath;

    // Dirty flag
    bool isDirty = false;

    // ── I/O ──────────────────────────────────────────────────────────────
    nlohmann::json toJson() const;
    static Project fromJson(const nlohmann::json& j);

    void save(const QString& filePath);
    static Project load(const QString& filePath);

    // ── Temp directory ───────────────────────────────────────────────────
    QString getTempDir() const;
    void    cleanupTempFiles();

    // ── Validation ───────────────────────────────────────────────────────
    struct ValidationResult {
        bool isValid = true;
        QString errorMessage;
    };
    ValidationResult validate() const;

private:
    QString  m_projectName;
    QString  m_createdAt;
    QString  m_version = PROJECT_VERSION;
};

} // namespace ncktv
