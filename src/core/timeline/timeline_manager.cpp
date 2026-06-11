#include "timeline_manager.h"
#include "lyric_engine.h"
#include "romanizer.h"
#include "../audio/audio_engine.h"
#include "../audio/stem_separation_worker.h"
#include "../audio/render_worker.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QUuid>
#include <QStandardPaths>
#include <QSettings>
#include <QDirIterator>
#include <QCoreApplication>
#include <QProcess>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <QDateTime>

static void logDebug(const QString& message) {
    QFile file(QCoreApplication::applicationDirPath() + "/ncktv_debug.log");
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " - " << message << "\n";
    }
}

#if defined(NCKTV_HAS_ONNX) && NCKTV_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sysinfoapi.h>
#include <dxgi.h>
#endif

namespace ncktv {

TimelineManager::TimelineManager(QObject* parent)
    : QObject(parent),
      m_trackListModel(new TrackListModel(this)) {
      
    // Re-calculate total timeline duration whenever track clips are updated
    connect(m_trackListModel, &TrackListModel::trackAdded, this, [this](Track* track) {
        connect(track, &Track::clipsChanged, this, &TimelineManager::recalculateTotalDuration);
        recalculateTotalDuration();
    });

    connect(m_trackListModel, &TrackListModel::trackRemoved, this, [this](const QString& trackId) {
        if (m_clipModels.contains(trackId)) {
            ClipListModel* model = m_clipModels.take(trackId);
            model->deleteLater();
        }
        recalculateTotalDuration();
    });

    // Load last selected models directory and model path from settings
    QSettings settings("NC-KTV", "NC-KTV_V2");
    m_modelsDirPath = settings.value("modelsDirPath", "").toString();
    m_modelPath = settings.value("modelPath", "").toString();
    m_showSourceMonitor = settings.value("showSourceMonitor", true).toBool();

    if (m_modelsDirPath.isEmpty() || !QDir(m_modelsDirPath).exists()) {
        // Default to standard app local data location (models directory)
        QString appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        m_modelsDirPath = QDir(appLocal).filePath("models");
    }

    detectSystemInfo();
    scanModelsDir();

    // Construct the LyricEngine for centralized lyric synchronization
    m_lyricEngine = new LyricEngine(this, this);

    // Wire playhead updates to the lyric engine
    connect(this, &TimelineManager::currentPlayheadTimeChanged, this, [this]() {
        if (m_lyricEngine)
            m_lyricEngine->updatePlaybackPosition(m_currentPlayheadTime);
    });

    // Rebuild lyric cache when tracks/clips change
    connect(m_trackListModel, &TrackListModel::trackAdded, this, [this](Track* track) {
        connect(track, &Track::clipsChanged, m_lyricEngine, &LyricEngine::rebuildLineCache);
        connect(track, &Track::clipsChanged, this, &TimelineManager::timelineChanged);
        m_lyricEngine->rebuildLineCache();
    });
}

TimelineManager::~TimelineManager() {
    if (m_downloadReply) {
        m_downloadReply->disconnect();
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->remove();
        delete m_downloadFile;
    }
    clearProject();
}

void TimelineManager::setModelPath(const QString& path) {
    if (m_modelPath != path) {
        m_modelPath = path;
        QSettings settings("NC-KTV", "NC-KTV_V2");
        settings.setValue("modelPath", m_modelPath);
        emit modelPathChanged();
    }
}

void TimelineManager::setModelsDirPath(const QString& path) {
    if (m_modelsDirPath != path) {
        m_modelsDirPath = path;
        QSettings settings("NC-KTV", "NC-KTV_V2");
        settings.setValue("modelsDirPath", m_modelsDirPath);
        emit modelsDirPathChanged();
        scanModelsDir();
    }
}

void TimelineManager::setShowSourceMonitor(bool show) {
    if (m_showSourceMonitor != show) {
        m_showSourceMonitor = show;
        QSettings settings("NC-KTV", "NC-KTV_V2");
        settings.setValue("showSourceMonitor", m_showSourceMonitor);
        emit showSourceMonitorChanged();
    }
}

void TimelineManager::scanModelsDir() {
    m_discoveredModels.clear();
    m_discoveredModelPaths.clear();

#ifdef _WIN32
    // Automatically search and register CUDA/cuDNN DLL path to DLL search directories
    QStringList dllPathsToTry;
    dllPathsToTry.append(QDir::current().filePath("models/cudn12/bin"));
    dllPathsToTry.append(QDir(QCoreApplication::applicationDirPath()).filePath("models/cudn12/bin"));
    dllPathsToTry.append(QDir(QCoreApplication::applicationDirPath() + "/../models/cudn12/bin").cleanPath(QCoreApplication::applicationDirPath() + "/../models/cudn12/bin"));
    dllPathsToTry.append(QDir(QCoreApplication::applicationDirPath() + "/../../models/cudn12/bin").cleanPath(QCoreApplication::applicationDirPath() + "/../../models/cudn12/bin"));
    dllPathsToTry.append(QDir(QCoreApplication::applicationDirPath() + "/../../../models/cudn12/bin").cleanPath(QCoreApplication::applicationDirPath() + "/../../../models/cudn12/bin"));
    dllPathsToTry.append(QDir(QCoreApplication::applicationDirPath() + "/../../../../models/cudn12/bin").cleanPath(QCoreApplication::applicationDirPath() + "/../../../../models/cudn12/bin"));
    dllPathsToTry.append(QDir(QDir::currentPath() + "/../models/cudn12/bin").cleanPath(QDir::currentPath() + "/../models/cudn12/bin"));
    dllPathsToTry.append(QDir(QDir::currentPath() + "/../../models/cudn12/bin").cleanPath(QDir::currentPath() + "/../../models/cudn12/bin"));
    dllPathsToTry.append(QDir(QDir::currentPath() + "/../../../models/cudn12/bin").cleanPath(QDir::currentPath() + "/../../../models/cudn12/bin"));
    dllPathsToTry.append(QDir(QDir::currentPath() + "/../../../../models/cudn12/bin").cleanPath(QDir::currentPath() + "/../../../../models/cudn12/bin"));

    for (const QString& dllPath : dllPathsToTry) {
        if (QDir(dllPath).exists()) {
            QString nativeDllPath = QDir::toNativeSeparators(dllPath);
            std::wstring wDllPath = nativeDllPath.toStdWString();
            if (SetDllDirectoryW(wDllPath.c_str())) {
                std::cout << "[StemSeparator] Registered DLL Search Directory: " << nativeDllPath.toStdString() << "\n";
                // Prepend to PATH for dynamic ONNX dependencies loading
                QByteArray currentPathEnv = qgetenv("PATH");
                QByteArray newPathEnv = nativeDllPath.toLocal8Bit() + ";" + currentPathEnv;
                qputenv("PATH", newPathEnv);
                break;
            }
        }
    }
#endif

    // Collect candidate paths for project models
    QStringList modelPathsToScan;
    if (!m_modelsDirPath.isEmpty() && !modelPathsToScan.contains(m_modelsDirPath)) {
        modelPathsToScan.append(m_modelsDirPath);
    }
    modelPathsToScan.append(QDir::current().filePath("models"));
    modelPathsToScan.append(QDir(QCoreApplication::applicationDirPath()).filePath("models"));
    modelPathsToScan.append(QDir(QCoreApplication::applicationDirPath() + "/../models").cleanPath(QCoreApplication::applicationDirPath() + "/../models"));
    modelPathsToScan.append(QDir(QCoreApplication::applicationDirPath() + "/../../models").cleanPath(QCoreApplication::applicationDirPath() + "/../../models"));
    modelPathsToScan.append(QDir(QCoreApplication::applicationDirPath() + "/../../../models").cleanPath(QCoreApplication::applicationDirPath() + "/../../../models"));
    modelPathsToScan.append(QDir(QCoreApplication::applicationDirPath() + "/../../../../models").cleanPath(QCoreApplication::applicationDirPath() + "/../../../../models"));
    modelPathsToScan.append(QDir(QDir::currentPath() + "/../models").cleanPath(QDir::currentPath() + "/../models"));
    modelPathsToScan.append(QDir(QDir::currentPath() + "/../../models").cleanPath(QDir::currentPath() + "/../../models"));
    modelPathsToScan.append(QDir(QDir::currentPath() + "/../../../models").cleanPath(QDir::currentPath() + "/../../../models"));
    modelPathsToScan.append(QDir(QDir::currentPath() + "/../../../../models").cleanPath(QDir::currentPath() + "/../../../../models"));

    for (const QString& projectModelsPath : modelPathsToScan) {
        if (QDir(projectModelsPath).exists()) {
            QDir mDir(projectModelsPath);
            QDirIterator it(projectModelsPath, QStringList() << "*.onnx", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString fullPath = it.next();
                if (!m_discoveredModelPaths.contains(fullPath)) {
                    m_discoveredModelPaths.append(fullPath);
                    m_discoveredModels.append(mDir.relativeFilePath(fullPath));
                }
            }
        }
    }

    // Collect candidate paths for Ultimate Vocal Remover models
    QStringList uvrPathsToScan;
    uvrPathsToScan.append(QDir::current().filePath("Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QCoreApplication::applicationDirPath()).filePath("Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QCoreApplication::applicationDirPath() + "/../Ultimate Vocal Remover/models").cleanPath(QCoreApplication::applicationDirPath() + "/../Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QCoreApplication::applicationDirPath() + "/../../Ultimate Vocal Remover/models").cleanPath(QCoreApplication::applicationDirPath() + "/../../Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QCoreApplication::applicationDirPath() + "/../../../Ultimate Vocal Remover/models").cleanPath(QCoreApplication::applicationDirPath() + "/../../../Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QCoreApplication::applicationDirPath() + "/../../../../Ultimate Vocal Remover/models").cleanPath(QCoreApplication::applicationDirPath() + "/../../../../Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QDir::currentPath() + "/../Ultimate Vocal Remover/models").cleanPath(QDir::currentPath() + "/../Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QDir::currentPath() + "/../../Ultimate Vocal Remover/models").cleanPath(QDir::currentPath() + "/../../Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QDir::currentPath() + "/../../../Ultimate Vocal Remover/models").cleanPath(QDir::currentPath() + "/../../../Ultimate Vocal Remover/models"));
    uvrPathsToScan.append(QDir(QDir::currentPath() + "/../../../../Ultimate Vocal Remover/models").cleanPath(QDir::currentPath() + "/../../../../Ultimate Vocal Remover/models"));

    for (const QString& projectUvrPath : uvrPathsToScan) {
        if (QDir(projectUvrPath).exists()) {
            QDir uvrDir(projectUvrPath);
            QDirIterator it(projectUvrPath, QStringList() << "*.onnx", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString fullPath = it.next();
                if (!m_discoveredModelPaths.contains(fullPath)) {
                    m_discoveredModelPaths.append(fullPath);
                    m_discoveredModels.append("UVR: " + uvrDir.relativeFilePath(fullPath));
                }
            }
        }
    }

    emit discoveredModelsChanged();

    // If active modelPath is empty or invalid, try to pick the first discovered one
    if ((m_modelPath.isEmpty() || !QFile::exists(m_modelPath)) && !m_discoveredModelPaths.isEmpty()) {
        setModelPath(m_discoveredModelPaths.first());
    }
}

void TimelineManager::setCurrentPlayheadTime(qint64 timeMicroseconds) {
    if (timeMicroseconds < 0) {
        timeMicroseconds = 0;
    }
    if (m_currentPlayheadTime != timeMicroseconds) {
        m_currentPlayheadTime = timeMicroseconds;
        emit currentPlayheadTimeChanged();
    }
}

void TimelineManager::setTotalDuration(qint64 durationMicroseconds) {
    if (durationMicroseconds < 0) {
        durationMicroseconds = 0;
    }
    if (m_totalDuration != durationMicroseconds) {
        m_totalDuration = durationMicroseconds;
        emit totalDurationChanged();
    }
}

void TimelineManager::setFps(double fpsValue) {
    if (fpsValue <= 0.0) {
        fpsValue = 30.0;
    }
    if (std::abs(m_fps - fpsValue) > 0.0001) {
        m_fps = fpsValue;
        emit fpsChanged();
    }
}

void TimelineManager::setMediaList(const QVariantList& list) {
    if (m_mediaList != list) {
        m_mediaList = list;
        emit mediaListChanged();
    }
}

void TimelineManager::setIsDirty(bool dirty) {
    if (m_isDirty != dirty) {
        m_isDirty = dirty;
        emit isDirtyChanged();
    }
}

void TimelineManager::setSubtitleFontFamily(const QString& family) {
    if (m_subtitleFontFamily != family) {
        m_subtitleFontFamily = family;
        setIsDirty(true);
        emit subtitleFontFamilyChanged();
    }
}

void TimelineManager::setSubtitleFontSize(int size) {
    if (m_subtitleFontSize != size) {
        m_subtitleFontSize = size;
        setIsDirty(true);
        emit subtitleFontSizeChanged();
    }
}

void TimelineManager::setSubtitleFillColor(const QString& color) {
    if (m_subtitleFillColor != color) {
        m_subtitleFillColor = color;
        setIsDirty(true);
        emit subtitleFillColorChanged();
    }
}

void TimelineManager::setSubtitleActiveColor(const QString& color) {
    if (m_subtitleActiveColor != color) {
        m_subtitleActiveColor = color;
        setIsDirty(true);
        emit subtitleActiveColorChanged();
    }
}

void TimelineManager::setSubtitleOutlineColor(const QString& color) {
    if (m_subtitleOutlineColor != color) {
        m_subtitleOutlineColor = color;
        setIsDirty(true);
        emit subtitleOutlineColorChanged();
    }
}

void TimelineManager::setSubtitleOutlineWidth(int width) {
    if (m_subtitleOutlineWidth != width) {
        m_subtitleOutlineWidth = width;
        setIsDirty(true);
        emit subtitleOutlineWidthChanged();
    }
}

void TimelineManager::setMarkers(const QVariantList& list) {
    if (m_markers != list) {
        m_markers = list;
        setIsDirty(true);
        emit markersChanged();
    }
}

void TimelineManager::addMarker(qint64 timeUs, const QString& name, const QString& color) {
    QVariantMap marker;
    marker["id"] = QUuid::createUuid().toString(QUuid::Id128);
    marker["timeUs"] = timeUs;
    marker["name"] = name;
    marker["color"] = color;
    m_markers.append(marker);

    // Sort markers by timeUs
    std::sort(m_markers.begin(), m_markers.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap()["timeUs"].toLongLong() < b.toMap()["timeUs"].toLongLong();
    });

    setIsDirty(true);
    emit markersChanged();
}

void TimelineManager::removeMarker(const QString& markerId) {
    bool found = false;
    for (int i = 0; i < m_markers.size(); ++i) {
        if (m_markers[i].toMap()["id"].toString() == markerId) {
            m_markers.removeAt(i);
            found = true;
            break;
        }
    }
    if (found) {
        setIsDirty(true);
        emit markersChanged();
    }
}

void TimelineManager::updateMarker(const QString& markerId, const QString& name, const QString& color) {
    bool found = false;
    for (int i = 0; i < m_markers.size(); ++i) {
        QVariantMap marker = m_markers[i].toMap();
        if (marker["id"].toString() == markerId) {
            marker["name"] = name;
            marker["color"] = color;
            m_markers[i] = marker;
            found = true;
            break;
        }
    }
    if (found) {
        setIsDirty(true);
        emit markersChanged();
    }
}


void TimelineManager::setSongTitle(const QString& title) {
    if (m_songTitle != title) {
        m_songTitle = title;
        setIsDirty(true);
        emit songTitleChanged();
    }
}

void TimelineManager::setArtistName(const QString& name) {
    if (m_artistName != name) {
        m_artistName = name;
        setIsDirty(true);
        emit artistNameChanged();
    }
}

void TimelineManager::setIntroSplashDuration(int durationMs) {
    if (m_introSplashDuration != durationMs) {
        m_introSplashDuration = durationMs;
        setIsDirty(true);
        emit introSplashDurationChanged();
    }
}

void TimelineManager::setEndingVideoPath(const QString& path) {
    if (m_endingVideoPath != path) {
        m_endingVideoPath = path;
        setIsDirty(true);
        emit endingVideoPathChanged();
    }
}

QString TimelineManager::resolveEndingVideoPath() const {
    if (m_endingVideoPath.isEmpty()) {
        return QString();
    }

    // 1. Try QRC resource path first if m_endingVideoPath is default or fallback
    if (m_endingVideoPath == "splash_screen/end.mp4" && QFile::exists(":/ncktv/gui/end.mp4")) {
        return "qrc:/ncktv/gui/end.mp4";
    }

    // 2. Try absolute or exact relative path
    QFileInfo info(m_endingVideoPath);
    if (info.isAbsolute() && info.exists()) {
        return m_endingVideoPath;
    }

    // 3. Try relative to current working directory
    QString path1 = QDir::current().filePath(m_endingVideoPath);
    if (QFile::exists(path1)) {
        return path1;
    }

    // 4. Try space alternative relative to current working directory if default
    if (m_endingVideoPath == "splash_screen/end.mp4") {
        QString spacePath = QDir::current().filePath("splash screen/end.mp4");
        if (QFile::exists(spacePath)) {
            return spacePath;
        }
    }

    // 5. Try relative to application directory
    QString path2 = QDir(QCoreApplication::applicationDirPath()).filePath(m_endingVideoPath);
    if (QFile::exists(path2)) {
        return path2;
    }
    if (m_endingVideoPath == "splash_screen/end.mp4") {
        QString spacePath = QDir(QCoreApplication::applicationDirPath()).filePath("splash screen/end.mp4");
        if (QFile::exists(spacePath)) {
            return spacePath;
        }
    }

    // 6. Try absolute fallback path for ending video splash
    QString fallbackPath = "D:/Document/NC-Project/NC-KTV/NC-KTV_V2/splash screen/end.mp4";
    if (QFile::exists(fallbackPath)) {
        return fallbackPath;
    }

    // 7. General QRC fallback if not found elsewhere
    if (QFile::exists(":/ncktv/gui/end.mp4")) {
        return "qrc:/ncktv/gui/end.mp4";
    }

    return QString(); // Not found
}


QString TimelineManager::addTrack(int type, const QString& name, const QString& trackId) {
    auto castedType = static_cast<Track::Type>(type);
    Track* track = new Track(trackId, castedType, name, this);
    m_trackListModel->addTrack(track);
    setIsDirty(true);
    emit timelineChanged();
    return track->trackId();
}

bool TimelineManager::removeTrack(const QString& trackId) {
    bool result = m_trackListModel->removeTrack(trackId);
    if (result) {
        setIsDirty(true);
        emit timelineChanged();
    }
    return result;
}

QObject* TimelineManager::getClipModelForTrack(const QString& trackId) {
    if (m_clipModels.contains(trackId)) {
        return m_clipModels[trackId];
    }

    Track* track = m_trackListModel->getTrackById(trackId);
    if (!track) {
        return nullptr;
    }

    ClipListModel* model = new ClipListModel(track, this);
    m_clipModels[trackId] = model;
    return model;
}

bool TimelineManager::addClipToTrack(const QString& trackId, const QString& clipId, int type, qint64 startTime, qint64 duration, const QString& sourceFile, const QString& lyricText) {
    Track* track = m_trackListModel->getTrackById(trackId);
    if (!track || track->isLocked()) {
        return false;
    }

    qint64 clipDuration = duration;
    if ((type == 0 || type == 1) && clipDuration <= 0 && !sourceFile.isEmpty()) {
        double secs = 0.0;
        if (AudioEngine::instance()) {
            AudioReader* reader = AudioEngine::instance()->getReader(sourceFile);
            if (reader) {
                secs = reader->durationSeconds();
            }
        }
        if (secs <= 0.0) {
            AudioReader tempReader;
            if (tempReader.decodeFile(sourceFile)) {
                secs = tempReader.durationSeconds();
            }
        }
        if (secs > 0.0) {
            clipDuration = static_cast<qint64>(secs * 1000000.0);
        } else {
            clipDuration = 10000000LL; // 10 seconds default fallback
        }
    }

    Clip* clip = new Clip(clipId, static_cast<Clip::Type>(type), startTime, clipDuration, track);
    if (!sourceFile.isEmpty()) {
        clip->setSourceFile(sourceFile);
        clip->setSourceDuration(clipDuration);
        
        // Auto-guess song title and artist from filename if current metadata is empty or default
        if ((m_songTitle == "Untitled Song" || m_songTitle.isEmpty()) && 
            (m_artistName == "Unknown Artist" || m_artistName.isEmpty())) {
            guessMetadataFromFilename(sourceFile);
        }
    }
    if (!lyricText.isEmpty()) {
        clip->setLyricText(lyricText);
    }

    if (!track->addClip(clip)) {
        delete clip;
        return false;
    }
    setIsDirty(true);
    return true;
}

bool TimelineManager::addClipToTrackWithSourceStart(const QString& trackId, const QString& clipId, int type, qint64 startTime, qint64 duration, qint64 sourceStart, const QString& sourceFile, const QString& lyricText) {
    Track* track = m_trackListModel->getTrackById(trackId);
    if (!track || track->isLocked()) {
        return false;
    }

    qint64 fullSourceDuration = duration;
    double secs = 0.0;
    if ((type == 0 || type == 1) && !sourceFile.isEmpty()) {
        if (AudioEngine::instance()) {
            AudioReader* reader = AudioEngine::instance()->getReader(sourceFile);
            if (reader) {
                secs = reader->durationSeconds();
            }
        }
        if (secs <= 0.0) {
            AudioReader tempReader;
            if (tempReader.decodeFile(sourceFile)) {
                secs = tempReader.durationSeconds();
            }
        }
        if (secs > 0.0) {
            fullSourceDuration = static_cast<qint64>(secs * 1000000.0);
        }
    }

    qint64 clipDuration = duration;
    if (clipDuration <= 0) {
        clipDuration = fullSourceDuration - sourceStart;
        if (clipDuration <= 0) {
            clipDuration = 10000000LL; // 10 seconds default fallback
        }
    }

    Clip* clip = new Clip(clipId, static_cast<Clip::Type>(type), startTime, clipDuration, track);
    if (!sourceFile.isEmpty()) {
        clip->setSourceFile(sourceFile);
        clip->setSourceStart(sourceStart);
        clip->setSourceDuration(fullSourceDuration);
        
        // Auto-guess song title and artist from filename if current metadata is empty or default
        if ((m_songTitle == "Untitled Song" || m_songTitle.isEmpty()) && 
            (m_artistName == "Unknown Artist" || m_artistName.isEmpty())) {
            guessMetadataFromFilename(sourceFile);
        }
    }
    if (!lyricText.isEmpty()) {
        clip->setLyricText(lyricText);
    }

    if (!track->addClip(clip)) {
        delete clip;
        return false;
    }
    setIsDirty(true);
    return true;
}


bool TimelineManager::splitClip(const QString& trackId, const QString& clipId, qint64 splitTimeMicroseconds) {
    Track* track = m_trackListModel->getTrackById(trackId);
    if (!track || track->isLocked()) {
        return false;
    }

    Clip* clip = track->getClip(clipId);
    if (!clip) {
        return false;
    }

    // Split point must lie strictly within the clip boundary
    if (splitTimeMicroseconds <= clip->startTime() || splitTimeMicroseconds >= clip->endTime()) {
        return false;
    }

    qint64 originalEnd = clip->endTime();
    qint64 splitOffset = splitTimeMicroseconds - clip->startTime();
    qint64 newDuration = originalEnd - splitTimeMicroseconds;

    // Resize the original clip
    clip->resizeClip(splitOffset);

    // Create the second clip segment representing the right half
    Clip* nextSegment = new Clip(
        QString(), 
        clip->type(), 
        splitTimeMicroseconds, 
        newDuration, 
        track
    );
    
    // Copy primary asset references and offset source frame indices
    nextSegment->setSourceFile(clip->sourceFile());
    nextSegment->setLyricText(clip->lyricText());
    
    if (clip->sourceDuration() > 0) {
        nextSegment->setSourceStart(clip->sourceStart() + splitOffset);
        nextSegment->setSourceDuration(clip->sourceDuration() - splitOffset);
        
        // Adjust the original clip source duration to fit the left segment
        clip->setSourceDuration(splitOffset);
    }

    // Attempt to register the split segment on the track
    if (!track->addClip(nextSegment)) {
        // Rollback original clip changes on failure
        clip->resizeClip(splitOffset + newDuration);
        delete nextSegment;
        return false;
    }

    setIsDirty(true);
    return true;
}

qint64 TimelineManager::checkSnapping(const QString& excludeClipId, qint64 targetTimeMicroseconds, qint64 thresholdMicroseconds) const {
    qint64 draggedDuration = 0;
    if (!excludeClipId.isEmpty()) {
        for (Track* track : m_trackListModel->tracks()) {
            if (Clip* c = track->getClip(excludeClipId)) {
                draggedDuration = c->duration();
                break;
            }
        }
    }

    qint64 bestSnappedStart = targetTimeMicroseconds;
    qint64 minDelta = thresholdMicroseconds + 1; // Higher than threshold to begin

    // Collect list of all timeline snap anchor locations
    QList<qint64> snapPoints;
    snapPoints.append(0LL); // Start of the timeline (0s)
    snapPoints.append(m_currentPlayheadTime);

    for (const Track* track : m_trackListModel->tracks()) {
        for (const Clip* c : track->clips()) {
            if (!excludeClipId.isEmpty() && c->clipId() == excludeClipId) {
                continue;
            }
            snapPoints.append(c->startTime());
            snapPoints.append(c->endTime());
        }
    }

    for (qint64 point : snapPoints) {
        // Option 1: Dragged clip START snaps to this anchor
        qint64 deltaStart = std::abs(targetTimeMicroseconds - point);
        if (deltaStart < minDelta) {
            minDelta = deltaStart;
            bestSnappedStart = point;
        }

        // Option 2: Dragged clip END snaps to this anchor
        if (draggedDuration > 0) {
            qint64 deltaEnd = std::abs((targetTimeMicroseconds + draggedDuration) - point);
            if (deltaEnd < minDelta) {
                minDelta = deltaEnd;
                bestSnappedStart = point - draggedDuration;
            }
        }
    }

    if (minDelta <= thresholdMicroseconds) {
        return bestSnappedStart;
    }
    return targetTimeMicroseconds;
}

qint64 TimelineManager::checkCollisions(const QString& trackId, const QString& clipId, qint64 targetTimeMicroseconds) const {
    Track* track = m_trackListModel->getTrackById(trackId);
    if (!track || track->type() == Track::Lyrics) {
        return targetTimeMicroseconds;
    }

    Clip* clip = track->getClip(clipId);
    if (!clip) {
        return targetTimeMicroseconds;
    }

    qint64 duration = clip->duration();
    qint64 oldStart = clip->startTime();
    qint64 validTime = targetTimeMicroseconds;

    if (targetTimeMicroseconds > oldStart) { // Dragging right
        qint64 closestRightClipStart = -1;
        for (const Clip* c : track->clips()) {
            if (c->clipId() == clipId) {
                continue;
            }
            if (c->startTime() >= oldStart + duration) {
                if (closestRightClipStart == -1 || c->startTime() < closestRightClipStart) {
                    closestRightClipStart = c->startTime();
                }
            }
        }
        if (closestRightClipStart != -1 && (validTime + duration) > closestRightClipStart) {
            validTime = closestRightClipStart - duration;
        }
    } else if (targetTimeMicroseconds < oldStart) { // Dragging left
        qint64 closestLeftClipEnd = -1;
        for (const Clip* c : track->clips()) {
            if (c->clipId() == clipId) {
                continue;
            }
            if (c->endTime() <= oldStart) {
                if (closestLeftClipEnd == -1 || c->endTime() > closestLeftClipEnd) {
                    closestLeftClipEnd = c->endTime();
                }
            }
        }
        if (closestLeftClipEnd != -1 && validTime < closestLeftClipEnd) {
            validTime = closestLeftClipEnd;
        }
    }

    return validTime;
}

QString TimelineManager::formatTimecode(qint64 microseconds) const {
    double totalSeconds = static_cast<double>(microseconds) / 1000000.0;
    int hh = static_cast<int>(totalSeconds) / 3600;
    int mm = (static_cast<int>(totalSeconds) % 3600) / 60;
    int ss = static_cast<int>(totalSeconds) % 60;
    double fractionalPart = totalSeconds - static_cast<int>(totalSeconds);
    int ff = static_cast<int>(std::round(fractionalPart * m_fps)) % static_cast<int>(m_fps);

    return QStringLiteral("%1:%2:%3:%4")
        .arg(hh, 2, 10, QLatin1Char('0'))
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'))
        .arg(ff, 2, 10, QLatin1Char('0'));
}

qint64 TimelineManager::parseTimecode(const QString& timecode) const {
    QString cleaned = timecode.trimmed();
    // Allow colons or semicolons for frame dividers
    QStringList parts = cleaned.split(QRegularExpression(QStringLiteral("[:;]")));
    if (parts.isEmpty()) {
        return 0;
    }

    int hh = 0, mm = 0, ss = 0, ff = 0;
    
    if (parts.size() == 4) {
        hh = parts[0].toInt();
        mm = parts[1].toInt();
        ss = parts[2].toInt();
        ff = parts[3].toInt();
    } else if (parts.size() == 3) {
        mm = parts[0].toInt();
        ss = parts[1].toInt();
        ff = parts[2].toInt();
    } else if (parts.size() == 2) {
        ss = parts[0].toInt();
        ff = parts[1].toInt();
    } else if (parts.size() == 1) {
        ss = parts[0].toInt();
    }

    double seconds = hh * 3600.0 + mm * 60.0 + ss + (static_cast<double>(ff) / m_fps);
    return static_cast<qint64>(seconds * 1000000.0);
}

qint64 TimelineManager::timeToFrames(qint64 microseconds) const {
    return std::round((static_cast<double>(microseconds) / 1000000.0) * m_fps);
}

qint64 TimelineManager::framesToTime(qint64 frames) const {
    return static_cast<qint64>(std::round((static_cast<double>(frames) / m_fps) * 1000000.0));
}

bool TimelineManager::saveProject(const QString& filePath) {
    nlohmann::json j = nlohmann::json::object();
    j["version"] = 2;
    j["fps"] = m_fps;
    j["playhead"] = m_currentPlayheadTime;
    j["totalDuration"] = m_totalDuration;

    nlohmann::json tracksJson = nlohmann::json::array();
    for (Track* track : m_trackListModel->tracks()) {
        tracksJson.push_back(track->toJson());
    }
    j["tracks"] = tracksJson;

    // Serialize media library list
    nlohmann::json mediaJson = nlohmann::json::array();
    for (const QVariant& item : m_mediaList) {
        QVariantMap map = item.toMap();
        nlohmann::json mediaItem = nlohmann::json::object();
        mediaItem["name"] = map["name"].toString().toStdString();
        mediaItem["path"] = map["path"].toString().toStdString();
        mediaItem["type"] = map["type"].toString().toStdString();
        mediaItem["durationMs"] = map["durationMs"].toLongLong();
        mediaJson.push_back(mediaItem);
    }
    j["mediaList"] = mediaJson;

    // Save subtitle style
    nlohmann::json subtitleJson = nlohmann::json::object();
    subtitleJson["fontFamily"] = m_subtitleFontFamily.toStdString();
    subtitleJson["fontSize"] = m_subtitleFontSize;
    subtitleJson["fillColor"] = m_subtitleFillColor.toStdString();
    subtitleJson["activeColor"] = m_subtitleActiveColor.toStdString();
    subtitleJson["outlineColor"] = m_subtitleOutlineColor.toStdString();
    subtitleJson["outlineWidth"] = m_subtitleOutlineWidth;
    j["subtitleStyle"] = subtitleJson;

    // Save markers
    nlohmann::json markersJson = nlohmann::json::array();
    for (const QVariant& item : m_markers) {
        QVariantMap map = item.toMap();
        nlohmann::json markerObj = nlohmann::json::object();
        markerObj["id"] = map["id"].toString().toStdString();
        markerObj["timeUs"] = map["timeUs"].toLongLong();
        markerObj["name"] = map["name"].toString().toStdString();
        markerObj["color"] = map["color"].toString().toStdString();
        markersJson.push_back(markerObj);
    }
    j["markers"] = markersJson;

    // Save extra options
    j["showVideoBackground"] = m_showVideoBackground;
    j["lyricDisplayMode"] = m_lyricDisplayMode;
    // Legacy key for older readers
    j["lyricsOnlyMode"] = (m_lyricDisplayMode == 1);
    j["exportAudioMode"] = m_exportAudioMode;
    j["songTitle"] = m_songTitle.toStdString();
    j["artistName"] = m_artistName.toStdString();
    j["introSplashDuration"] = m_introSplashDuration;
    j["endingVideoPath"] = m_endingVideoPath.toStdString();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << QString::fromStdString(j.dump(4));
    file.close();
    
    setIsDirty(false); // Reset dirty flag on successful save
    return true;
}

bool TimelineManager::loadProject(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    QString rawContent = in.readAll();
    file.close();

    try {
        nlohmann::json j = nlohmann::json::parse(rawContent.toStdString());
        
        clearProject();

        m_fps = j.value("fps", 30.0);
        m_currentPlayheadTime = j.value("playhead", 0LL);
        m_totalDuration = j.value("totalDuration", 0LL);

        if (j.contains("tracks") && j["tracks"].is_array()) {
            for (const auto& trackJson : j["tracks"]) {
                Track* track = Track::fromJson(trackJson, this);
                if (track) {
                    m_trackListModel->addTrack(track);
                }
            }
        }

        // ─── Project Path Resolver (Relative Migration Check) ─────────────────
        QFileInfo projectFileInfo(filePath);
        QString projectDir = projectFileInfo.absolutePath();
        QString appDir = QCoreApplication::applicationDirPath();

        // 1. Resolve Clip file paths in loaded tracks
        for (Track* track : m_trackListModel->tracks()) {
            for (Clip* clip : track->clips()) {
                QString srcPath = clip->sourceFile();
                if (!srcPath.isEmpty() && !QFile::exists(srcPath)) {
                    QFileInfo srcInfo(srcPath);
                    QString fileName = srcInfo.fileName();
                    
                    QString option1 = projectDir + "/video/" + fileName;
                    QString option2 = projectDir + "/" + srcInfo.dir().dirName() + "/" + fileName;
                    QString option3 = projectDir + "/" + fileName;
                    
                    QString option4 = appDir + "/../video/" + fileName;
                    QString option5 = appDir + "/video/" + fileName;
                    QString option6 = appDir + "/../" + fileName;
                    
                    if (QFile::exists(option1)) {
                        clip->setSourceFile(option1);
                        std::cout << "[PATH RESOLVER] Resolved clip " << clip->clipId().toStdString() << " path to: " << option1.toStdString() << "\n";
                    } else if (QFile::exists(option2)) {
                        clip->setSourceFile(option2);
                        std::cout << "[PATH RESOLVER] Resolved clip " << clip->clipId().toStdString() << " path to: " << option2.toStdString() << "\n";
                    } else if (QFile::exists(option3)) {
                        clip->setSourceFile(option3);
                        std::cout << "[PATH RESOLVER] Resolved clip " << clip->clipId().toStdString() << " path to: " << option3.toStdString() << "\n";
                    } else if (QFile::exists(option4)) {
                        clip->setSourceFile(option4);
                        std::cout << "[PATH RESOLVER] Resolved clip " << clip->clipId().toStdString() << " path to: " << option4.toStdString() << "\n";
                    } else if (QFile::exists(option5)) {
                        clip->setSourceFile(option5);
                        std::cout << "[PATH RESOLVER] Resolved clip " << clip->clipId().toStdString() << " path to: " << option5.toStdString() << "\n";
                    } else if (QFile::exists(option6)) {
                        clip->setSourceFile(option6);
                        std::cout << "[PATH RESOLVER] Resolved clip " << clip->clipId().toStdString() << " path to: " << option6.toStdString() << "\n";
                    }
                }
            }
        }

        // 2. Deserialize & resolve media library list
        QVariantList mediaList;
        if (j.contains("mediaList") && j["mediaList"].is_array()) {
            for (const auto& mediaItem : j["mediaList"]) {
                QVariantMap map;
                map["name"] = QString::fromStdString(mediaItem.value("name", ""));
                QString path = QString::fromStdString(mediaItem.value("path", ""));
                
                if (!path.isEmpty() && !QFile::exists(path)) {
                    QFileInfo pathInfo(path);
                    QString fileName = pathInfo.fileName();
                    
                    QString option1 = projectDir + "/video/" + fileName;
                    QString option2 = projectDir + "/" + pathInfo.dir().dirName() + "/" + fileName;
                    QString option3 = projectDir + "/" + fileName;
                    
                    QString option4 = appDir + "/../video/" + fileName;
                    QString option5 = appDir + "/video/" + fileName;
                    QString option6 = appDir + "/../" + fileName;
                    
                    if (QFile::exists(option1)) {
                        path = option1;
                    } else if (QFile::exists(option2)) {
                        path = option2;
                    } else if (QFile::exists(option3)) {
                        path = option3;
                    } else if (QFile::exists(option4)) {
                        path = option4;
                    } else if (QFile::exists(option5)) {
                        path = option5;
                    } else if (QFile::exists(option6)) {
                        path = option6;
                    }
                }
                
                map["path"] = path;
                map["type"] = QString::fromStdString(mediaItem.value("type", ""));
                map["durationMs"] = mediaItem.value("durationMs", 180000LL);
                mediaList.append(map);
            }
        }
        setMediaList(mediaList);

        // Load subtitle style
        if (j.contains("subtitleStyle") && j["subtitleStyle"].is_object()) {
            const auto& subtitleJson = j["subtitleStyle"];
            m_subtitleFontFamily = QString::fromStdString(subtitleJson.value("fontFamily", "Outfit"));
            m_subtitleFontSize = subtitleJson.value("fontSize", 24);
            m_subtitleFillColor = QString::fromStdString(subtitleJson.value("fillColor", "#4A4A5A"));
            m_subtitleActiveColor = QString::fromStdString(subtitleJson.value("activeColor", "#00E676"));
            m_subtitleOutlineColor = QString::fromStdString(subtitleJson.value("outlineColor", "#08080A"));
            m_subtitleOutlineWidth = subtitleJson.value("outlineWidth", 2);
        } else {
            // Restore defaults
            m_subtitleFontFamily = "Outfit";
            m_subtitleFontSize = 24;
            m_subtitleFillColor = "#4A4A5A";
            m_subtitleActiveColor = "#00E676";
            m_subtitleOutlineColor = "#08080A";
            m_subtitleOutlineWidth = 2;
        }

        // Load markers
        QVariantList loadedMarkers;
        if (j.contains("markers") && j["markers"].is_array()) {
            for (const auto& markerObj : j["markers"]) {
                QVariantMap map;
                map["id"] = QString::fromStdString(markerObj.value("id", ""));
                map["timeUs"] = markerObj.value("timeUs", 0LL);
                map["name"] = QString::fromStdString(markerObj.value("name", "Marker"));
                map["color"] = QString::fromStdString(markerObj.value("color", "green"));
                loadedMarkers.append(map);
            }
            // Sort just in case
            std::sort(loadedMarkers.begin(), loadedMarkers.end(), [](const QVariant& a, const QVariant& b) {
                return a.toMap()["timeUs"].toLongLong() < b.toMap()["timeUs"].toLongLong();
            });
        }
        m_markers = loadedMarkers;

        // Load extra options
        m_showVideoBackground = j.value("showVideoBackground", true);
        // Load lyric display mode (with legacy fallback)
        if (j.contains("lyricDisplayMode")) {
            m_lyricDisplayMode = j.value("lyricDisplayMode", 0);
        } else {
            // Legacy migration: lyricsOnlyMode true → mode 1
            m_lyricDisplayMode = j.value("lyricsOnlyMode", false) ? 1 : 0;
        }
        m_exportAudioMode = j.value("exportAudioMode", 0);
        m_songTitle = QString::fromStdString(j.value("songTitle", "Untitled Song"));
        m_artistName = QString::fromStdString(j.value("artistName", "Unknown Artist"));
        m_introSplashDuration = j.value("introSplashDuration", 3000);
        m_endingVideoPath = QString::fromStdString(j.value("endingVideoPath", "splash_screen/end.mp4"));
        if (!m_endingVideoPath.isEmpty() && !QFile::exists(m_endingVideoPath)) {
            QFileInfo pathInfo(m_endingVideoPath);
            QString fileName = pathInfo.fileName();
            
            QString option1 = projectDir + "/splash_screen/" + fileName;
            QString option2 = projectDir + "/splash/" + fileName;
            QString option3 = projectDir + "/" + fileName;
            
            QString option4 = appDir + "/../splash_screen/" + fileName;
            QString option5 = appDir + "/../splash/" + fileName;
            QString option6 = appDir + "/../" + fileName;
            
            if (QFile::exists(option1)) {
                m_endingVideoPath = option1;
            } else if (QFile::exists(option2)) {
                m_endingVideoPath = option2;
            } else if (QFile::exists(option3)) {
                m_endingVideoPath = option3;
            } else if (QFile::exists(option4)) {
                m_endingVideoPath = option4;
            } else if (QFile::exists(option5)) {
                m_endingVideoPath = option5;
            } else if (QFile::exists(option6)) {
                m_endingVideoPath = option6;
            }
        }

        emit fpsChanged();
        emit currentPlayheadTimeChanged();
        emit totalDurationChanged();
        emit subtitleFontFamilyChanged();
        emit subtitleFontSizeChanged();
        emit subtitleFillColorChanged();
        emit subtitleActiveColorChanged();
        emit subtitleOutlineColorChanged();
        emit subtitleOutlineWidthChanged();
        emit markersChanged();
        emit showVideoBackgroundChanged();
        emit lyricDisplayModeChanged();
        emit exportAudioModeChanged();
        emit songTitleChanged();
        emit artistNameChanged();
        emit introSplashDurationChanged();
        emit endingVideoPathChanged();
        emit projectLoaded();
        
        setIsDirty(false); // Reset dirty flag on successful load
        return true;
        
    } catch (...) {
        return false;
    }
}

void TimelineManager::clearProject() {
    if (m_separationWorker) {
        m_separationWorker->requestInterruption();
        m_separationWorker->wait();
        delete m_separationWorker;
        m_separationWorker = nullptr;
    }

    if (m_renderWorker) {
        m_renderWorker->requestInterruption();
        m_renderWorker->wait();
        delete m_renderWorker;
        m_renderWorker = nullptr;
    }

    // Clear list models
    m_trackListModel->clear();
    
    // Remove and delete dynamic clip list models
    qDeleteAll(m_clipModels);
    m_clipModels.clear();

    m_currentPlayheadTime = 0;
    m_totalDuration = 0;
    m_isSeparating = false;
    m_separationProgress = 0.0;
    m_separationStatusText = "";

    m_isRendering = false;
    m_renderProgress = 0.0;
    m_renderStatusText = "";
    
    m_mediaList.clear();
    emit mediaListChanged();
    
    m_markers.clear();
    m_subtitleFontFamily = "Outfit";
    m_subtitleFontSize = 24;
    m_subtitleFillColor = "#4A4A5A";
    m_subtitleActiveColor = "#00E676";
    m_subtitleOutlineColor = "#08080A";
    m_subtitleOutlineWidth = 2;

    m_songTitle = "Untitled Song";
    m_artistName = "Unknown Artist";
    m_introSplashDuration = 3000;
    m_endingVideoPath = "splash_screen/end.mp4";

    setIsDirty(false);
    
    emit currentPlayheadTimeChanged();
    emit totalDurationChanged();
    emit isSeparatingChanged();
    emit separationProgressChanged();
    emit separationStatusTextChanged();
    emit isRenderingChanged();
    emit renderProgressChanged();
    emit renderStatusTextChanged();
    emit subtitleFontFamilyChanged();
    emit subtitleFontSizeChanged();
    emit subtitleFillColorChanged();
    emit subtitleActiveColorChanged();
    emit subtitleOutlineColorChanged();
    emit subtitleOutlineWidthChanged();
    emit markersChanged();
    emit songTitleChanged();
    emit artistNameChanged();
    emit introSplashDurationChanged();
    emit endingVideoPathChanged();
    emit projectCleared();
}

void TimelineManager::recalculateTotalDuration() {
    qint64 maxEnd = 0;
    for (const Track* track : m_trackListModel->tracks()) {
        for (const Clip* clip : track->clips()) {
            maxEnd = std::max(maxEnd, clip->endTime());
        }
    }
    setTotalDuration(maxEnd);
}

void TimelineManager::separateStems(const QString& clipId) {
    if (m_isSeparating) return;

    Clip* clip = nullptr;
    for (Track* track : m_trackListModel->tracks()) {
        if (Clip* c = track->getClip(clipId)) {
            clip = c;
            break;
        }
    }

    if (!clip) return;
    if (clip->sourceFile().isEmpty()) return;

    std::vector<float> inputSamples;
    if (AudioEngine::instance()) {
        AudioReader* reader = AudioEngine::instance()->getReader(clip->sourceFile());
        if (reader) {
            inputSamples = reader->samples();
        }
    }

    if (inputSamples.empty()) {
        AudioReader reader;
        if (reader.decodeFile(clip->sourceFile())) {
            inputSamples = reader.samples();
        }
    }

    if (inputSamples.empty()) {
        onSeparationFailed(clipId, "Could not decode audio from source file.");
        return;
    }

    QFileInfo fileInfo(clip->sourceFile());
    QString stemsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/NC-KTV Downloads/Stems";
    QDir().mkpath(stemsDir);
    QString vocalsPath = QDir(stemsDir).filePath(fileInfo.baseName() + "_vocals.wav");
    QString instrumentalPath = QDir(stemsDir).filePath(fileInfo.baseName() + "_instrumental.wav");

    m_isSeparating = true;
    m_separationProgress = 0.0;
    m_separationStatusText = "Initializing ONNX session...";
    emit isSeparatingChanged();
    emit separationProgressChanged();
    emit separationStatusTextChanged();

    if (m_separationWorker) {
        m_separationWorker->deleteLater();
    }

    m_separationWorker = new StemSeparationWorker(clipId, inputSamples, vocalsPath, instrumentalPath, m_modelPath, this);

    connect(m_separationWorker, &StemSeparationWorker::progressUpdated, this, &TimelineManager::onSeparationProgress);
    connect(m_separationWorker, &StemSeparationWorker::separationCompleted, this, &TimelineManager::onSeparationCompleted);
    connect(m_separationWorker, &StemSeparationWorker::separationFailed, this, &TimelineManager::onSeparationFailed);

    m_separationWorker->start();
}

void TimelineManager::separateStemsForFile(const QString& filePath) {
    logDebug("separateStemsForFile: entering with path: " + filePath);
    if (m_isSeparating) {
        logDebug("separateStemsForFile: already separating, aborting.");
        return;
    }
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        logDebug("separateStemsForFile: file is empty or does not exist: " + filePath);
        return;
    }

    std::vector<float> inputSamples;
    if (AudioEngine::instance()) {
        AudioReader* reader = AudioEngine::instance()->getReader(filePath);
        if (reader) {
            inputSamples = reader->samples();
            logDebug("separateStemsForFile: retrieved reader samples from AudioEngine cache. count: " + QString::number(inputSamples.size()));
        }
    }

    if (inputSamples.empty()) {
        logDebug("separateStemsForFile: cache is empty, decoding file from disk using AudioReader...");
        AudioReader reader;
        if (reader.decodeFile(filePath)) {
            inputSamples = reader.samples();
            logDebug("separateStemsForFile: decodeFile succeeded. samples count: " + QString::number(inputSamples.size()));
        } else {
            logDebug("separateStemsForFile: decodeFile failed.");
        }
    }

    if (inputSamples.empty()) {
        logDebug("separateStemsForFile: FAILED to decode audio from source file.");
        onSeparationFailed("", "Could not decode audio from source file.");
        return;
    }

    QFileInfo fileInfo(filePath);
    QString stemsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/NC-KTV Downloads/Stems";
    QDir().mkpath(stemsDir);
    QString vocalsPath = QDir(stemsDir).filePath(fileInfo.baseName() + "_vocals.wav");
    QString instrumentalPath = QDir(stemsDir).filePath(fileInfo.baseName() + "_instrumental.wav");
    logDebug("separateStemsForFile: output paths: vocals=" + vocalsPath + ", inst=" + instrumentalPath);

    m_isSeparating = true;
    m_separationProgress = 0.0;
    m_separationStatusText = "Initializing ONNX session...";
    emit isSeparatingChanged();
    emit separationProgressChanged();
    emit separationStatusTextChanged();

    if (m_separationWorker) {
        m_separationWorker->deleteLater();
    }

    logDebug("separateStemsForFile: spawning StemSeparationWorker...");
    m_separationWorker = new StemSeparationWorker(QString(), inputSamples, vocalsPath, instrumentalPath, m_modelPath, this);

    connect(m_separationWorker, &StemSeparationWorker::progressUpdated, this, &TimelineManager::onSeparationProgress);
    connect(m_separationWorker, &StemSeparationWorker::separationCompleted, this, &TimelineManager::onSeparationCompleted);
    connect(m_separationWorker, &StemSeparationWorker::separationFailed, this, &TimelineManager::onSeparationFailed);

    m_separationWorker->start();
}


void TimelineManager::onSeparationProgress(double fraction) {
    m_separationProgress = fraction;
    m_separationStatusText = QString("Separating stems... %1%").arg(static_cast<int>(fraction * 100));
    emit separationProgressChanged();
    emit separationStatusTextChanged();
}

void TimelineManager::onSeparationCompleted(const QString& clipId, const QString& vocalsPath, const QString& instPath) {
    logDebug("onSeparationCompleted: clipId=" + clipId + ", vocalsPath=" + vocalsPath + ", instPath=" + instPath);
    Clip* clip = nullptr;
    Track* parentTrack = nullptr;
    for (Track* track : m_trackListModel->tracks()) {
        if (Clip* c = track->getClip(clipId)) {
            clip = c;
            parentTrack = track;
            break;
        }
    }

    if (parentTrack) {
        parentTrack->setIsMuted(true);
    }

    QString fileName;
    qint64 startTime = 0;
    qint64 duration = 0;

    if (clip) {
        fileName = QFileInfo(clip->sourceFile()).fileName();
        startTime = clip->startTime();
        duration = clip->duration();
    } else {
        fileName = QFileInfo(vocalsPath).fileName().replace("_vocals.wav", "");
        startTime = m_currentPlayheadTime; // Start at the current playhead position
        duration = 0; // C++ will auto-detect full length
    }

    QString vocalsTrackId;
    QString instTrackId;

    // Search for existing matching audio tracks to reuse before making new ones
    for (Track* t : m_trackListModel->tracks()) {
        if (t->type() == Track::Audio) {
            QString nameLower = t->name().toLower();
            if (vocalsTrackId.isEmpty() && (nameLower.contains("vocals") || nameLower.contains("vocal"))) {
                vocalsTrackId = t->trackId();
            } else if (instTrackId.isEmpty() && (nameLower.contains("instrumental") || nameLower.contains("instrument") || nameLower.contains("bgm") || nameLower.contains("background"))) {
                instTrackId = t->trackId();
            }
        }
    }

    // Fall back to creating new tracks if matching ones aren't found in timeline
    if (vocalsTrackId.isEmpty()) {
        vocalsTrackId = addTrack(0, "Vocals (" + fileName + ")");
    } else {
        // Clear any existing clips on the matched track to prevent collisions
        Track* vTrack = m_trackListModel->getTrackById(vocalsTrackId);
        if (vTrack && !vTrack->isLocked()) {
            QList<Clip*> trackClips = vTrack->clips();
            for (Clip* c : trackClips) {
                vTrack->removeClip(c->clipId());
            }
        }
    }

    if (instTrackId.isEmpty()) {
        instTrackId = addTrack(0, "Instrumental (" + fileName + ")");
    } else {
        // Clear any existing clips on the matched track to prevent collisions
        Track* iTrack = m_trackListModel->getTrackById(instTrackId);
        if (iTrack && !iTrack->isLocked()) {
            QList<Clip*> trackClips = iTrack->clips();
            for (Clip* c : trackClips) {
                iTrack->removeClip(c->clipId());
            }
        }
    }

    if (AudioEngine::instance()) {
        AudioEngine::instance()->preloadFile(vocalsPath);
        AudioEngine::instance()->preloadFile(instPath);
    }

    addClipToTrack(vocalsTrackId, QString(), 0, startTime, duration, vocalsPath);
    addClipToTrack(instTrackId, QString(), 0, startTime, duration, instPath);

    if (!clip) {
        emit mediaSeparationCompleted(vocalsPath, instPath);
    }

    m_isSeparating = false;
    m_separationProgress = 1.0;
    m_separationStatusText = "Separation completed successfully!";
    emit isSeparatingChanged();
    emit separationProgressChanged();
    emit separationStatusTextChanged();
}

void TimelineManager::onSeparationFailed(const QString& clipId, const QString& errorMessage) {
    logDebug("onSeparationFailed: clipId=" + clipId + ", error=" + errorMessage);
    Q_UNUSED(clipId);
    m_isSeparating = false;
    m_separationProgress = 0.0;
    m_separationStatusText = "Error: " + errorMessage;
    emit isSeparatingChanged();
    emit separationProgressChanged();
    emit separationStatusTextChanged();
    emit mediaSeparationFailed(errorMessage);
}

void TimelineManager::startExport(const QString& outputPath, int width, int height, int fps, int videoBitrate, int audioBitrate, const QString& audioCodec) {
    if (m_isRendering) return;

    m_isRendering = true;
    m_renderProgress = 0.0;
    m_renderStatusText = "Initializing export thread...";
    emit isRenderingChanged();
    emit renderProgressChanged();
    emit renderStatusTextChanged();

    if (m_renderWorker) {
        m_renderWorker->deleteLater();
    }

    m_renderWorker = new RenderWorker(this, AudioEngine::instance(), outputPath, width, height, fps, videoBitrate, audioBitrate, audioCodec, this);

    connect(m_renderWorker, &RenderWorker::progressUpdated, this, &TimelineManager::onRenderProgress);
    connect(m_renderWorker, &RenderWorker::statusTextChanged, this, &TimelineManager::onRenderStatusText);
    connect(m_renderWorker, &RenderWorker::renderCompleted, this, &TimelineManager::onRenderCompleted);
    connect(m_renderWorker, &RenderWorker::renderFailed, this, &TimelineManager::onRenderFailed);

    m_renderWorker->start();
}

void TimelineManager::cancelExport() {
    if (!m_isRendering || !m_renderWorker) return;
    m_renderStatusText = "Cancelling export...";
    emit renderStatusTextChanged();
    m_renderWorker->requestInterruption();
}

void TimelineManager::onRenderProgress(double fraction) {
    m_renderProgress = fraction;
    emit renderProgressChanged();
}

void TimelineManager::onRenderStatusText(const QString& text) {
    m_renderStatusText = text;
    emit renderStatusTextChanged();
}

void TimelineManager::onRenderCompleted(const QString& outputPath, const QString& videoCodec, const QString& audioCodec) {
    Q_UNUSED(videoCodec);
    Q_UNUSED(audioCodec);
    m_isRendering = false;
    m_renderProgress = 1.0;
    m_renderStatusText = "Export completed!";
    emit isRenderingChanged();
    emit renderProgressChanged();
    emit renderStatusTextChanged();
    emit exportCompleted(outputPath);
}

void TimelineManager::onRenderFailed(const QString& errorMessage) {
    m_isRendering = false;
    m_renderProgress = 0.0;
    m_renderStatusText = errorMessage;
    emit isRenderingChanged();
    emit renderProgressChanged();
    emit renderStatusTextChanged();
    emit exportFailed(errorMessage);
}

bool TimelineManager::importLyricsFromFile(const QString& trackId, const QString& filePath) {
    Track* track = m_trackListModel->getTrackById(trackId);
    if (!track || track->isLocked()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    bool isSrt = filePath.endsWith(".srt", Qt::CaseInsensitive);
    bool isLrc = filePath.endsWith(".lrc", Qt::CaseInsensitive);

    if (!isSrt && !isLrc) {
        if (content.contains("-->")) {
            isSrt = true;
        } else if (content.contains("[") && content.contains("]")) {
            isLrc = true;
        } else {
            return false;
        }
    }

    if (isLrc) {
        QStringList lines = content.split('\n');
        struct LrcEntry {
            qint64 startTimeUs;
            QString text;
        };
        QList<LrcEntry> entries;

        QRegularExpression timeRegex(R"(\[(\d+):(\d+(?:\.\d+)?)\])");

        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;

            QRegularExpressionMatchIterator it = timeRegex.globalMatch(trimmed);
            QList<qint64> times;
            int lastTagEnd = 0;
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                int mins = match.captured(1).toInt();
                double secs = match.captured(2).toDouble();
                qint64 timeUs = static_cast<qint64>((mins * 60.0 + secs) * 1000000.0);
                times.append(timeUs);
                lastTagEnd = match.capturedEnd();
            }

            if (!times.isEmpty()) {
                QString text = trimmed.mid(lastTagEnd).trimmed();
                for (qint64 timeUs : times) {
                    entries.append({timeUs, text});
                }
            }
        }

        if (entries.isEmpty()) return false;

        std::sort(entries.begin(), entries.end(), [](const LrcEntry& a, const LrcEntry& b) {
            return a.startTimeUs < b.startTimeUs;
        });

        track->blockSignals(true);
        for (int i = 0; i < entries.size(); ++i) {
            qint64 start = entries[i].startTimeUs;
            qint64 duration = 4000000LL;
            if (i < entries.size() - 1) {
                qint64 diff = entries[i + 1].startTimeUs - start;
                if (diff > 0) {
                    duration = diff;
                }
            }
            QString clipId = QString("clip_lyr_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
            addClipToTrack(trackId, clipId, 2, start, duration, "", entries[i].text);
        }
        track->blockSignals(false);
        emit track->clipsChanged();

        if (m_clipModels.contains(trackId)) {
            m_clipModels[trackId]->refresh();
        }

        recalculateTotalDuration();
        return true;
    } else if (isSrt) {
        QStringList lines = content.split('\n');
        int state = 0;
        qint64 startUs = 0;
        qint64 durationUs = 0;
        QString lyricText = "";

        QRegularExpression timecodeRegex(R"((\d{2}):(\d{2}):(\d{2})[,.](\d{3})\s*-->\s*(\d{2}):(\d{2}):(\d{2})[,.](\d{3}))");

        track->blockSignals(true);
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (state == 0) {
                if (trimmed.isEmpty()) continue;
                bool ok = false;
                trimmed.toInt(&ok);
                if (ok) {
                    state = 1;
                }
            } else if (state == 1) {
                QRegularExpressionMatch match = timecodeRegex.match(trimmed);
                if (match.hasMatch()) {
                    int sh = match.captured(1).toInt();
                    int sm = match.captured(2).toInt();
                    int ss = match.captured(3).toInt();
                    int sms = match.captured(4).toInt();

                    int eh = match.captured(5).toInt();
                    int em = match.captured(6).toInt();
                    int es = match.captured(7).toInt();
                    int ems = match.captured(8).toInt();

                    qint64 start = (sh * 3600LL + sm * 60LL + ss) * 1000000LL + sms * 1000LL;
                    qint64 end = (eh * 3600LL + em * 60LL + es) * 1000000LL + ems * 1000LL;

                    startUs = start;
                    durationUs = (end > start) ? (end - start) : 4000000LL;
                    lyricText.clear();
                    state = 2;
                } else {
                    state = 0;
                }
            } else if (state == 2) {
                if (trimmed.isEmpty()) {
                    if (!lyricText.isEmpty()) {
                        QString clipId = QString("clip_lyr_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
                        addClipToTrack(trackId, clipId, 2, startUs, durationUs, "", lyricText.trimmed());
                    }
                    state = 0;
                } else {
                    if (!lyricText.isEmpty()) lyricText += " ";
                    lyricText += trimmed;
                }
            }
        }
        if (state == 2 && !lyricText.isEmpty()) {
            QString clipId = QString("clip_lyr_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
            addClipToTrack(trackId, clipId, 2, startUs, durationUs, "", lyricText.trimmed());
        }
        track->blockSignals(false);
        emit track->clipsChanged();

        if (m_clipModels.contains(trackId)) {
            m_clipModels[trackId]->refresh();
        }

        recalculateTotalDuration();
        return true;
    }

    return false;
}

bool TimelineManager::importLyricsFromString(const QString& trackId, const QString& rawLrcContent) {
    Track* track = m_trackListModel->getTrackById(trackId);
    if (!track || track->isLocked()) {
        return false;
    }

    QString content = rawLrcContent;
    bool isSrt = content.contains("-->");
    bool isLrc = content.contains("[") && content.contains("]");

    if (!isSrt && !isLrc) {
        return false;
    }

    if (isLrc) {
        QStringList lines = content.split('\n');
        struct LrcEntry {
            qint64 startTimeUs;
            QString text;
        };
        QList<LrcEntry> entries;

        QRegularExpression timeRegex(R"(\[(\d+):(\d+(?:\.\d+)?)\])");

        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;

            QRegularExpressionMatchIterator it = timeRegex.globalMatch(trimmed);
            QList<qint64> times;
            int lastTagEnd = 0;
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                int mins = match.captured(1).toInt();
                double secs = match.captured(2).toDouble();
                qint64 timeUs = static_cast<qint64>((mins * 60.0 + secs) * 1000000.0);
                times.append(timeUs);
                lastTagEnd = match.capturedEnd();
            }

            if (!times.isEmpty()) {
                QString text = trimmed.mid(lastTagEnd).trimmed();
                for (qint64 timeUs : times) {
                    entries.append({timeUs, text});
                }
            }
        }

        if (entries.isEmpty()) return false;

        std::sort(entries.begin(), entries.end(), [](const LrcEntry& a, const LrcEntry& b) {
            return a.startTimeUs < b.startTimeUs;
        });

        track->blockSignals(true);
        for (int i = 0; i < entries.size(); ++i) {
            qint64 start = entries[i].startTimeUs;
            qint64 duration = 4000000LL;
            if (i < entries.size() - 1) {
                qint64 diff = entries[i + 1].startTimeUs - start;
                if (diff > 0) {
                    duration = diff;
                }
            }
            QString clipId = QString("clip_lyr_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
            addClipToTrack(trackId, clipId, 2, start, duration, "", entries[i].text);
        }
        track->blockSignals(false);
        emit track->clipsChanged();

        if (m_clipModels.contains(trackId)) {
            m_clipModels[trackId]->refresh();
        }

        recalculateTotalDuration();
        return true;
    } else if (isSrt) {
        QStringList lines = content.split('\n');
        int state = 0;
        qint64 startUs = 0;
        qint64 durationUs = 0;
        QString lyricText = "";

        QRegularExpression timecodeRegex(R"((\d{2}):(\d{2}):(\d{2})[,.](\d{3})\s*-->\s*(\d{2}):(\d{2}):(\d{2})[,.](\d{3}))");

        track->blockSignals(true);
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (state == 0) {
                if (trimmed.isEmpty()) continue;
                bool ok = false;
                trimmed.toInt(&ok);
                if (ok) {
                    state = 1;
                }
            } else if (state == 1) {
                QRegularExpressionMatch match = timecodeRegex.match(trimmed);
                if (match.hasMatch()) {
                    int sh = match.captured(1).toInt();
                    int sm = match.captured(2).toInt();
                    int ss = match.captured(3).toInt();
                    int sms = match.captured(4).toInt();

                    int eh = match.captured(5).toInt();
                    int em = match.captured(6).toInt();
                    int es = match.captured(7).toInt();
                    int ems = match.captured(8).toInt();

                    qint64 start = (sh * 3600LL + sm * 60LL + ss) * 1000000LL + sms * 1000LL;
                    qint64 end = (eh * 3600LL + em * 60LL + es) * 1000000LL + ems * 1000LL;

                    startUs = start;
                    durationUs = (end > start) ? (end - start) : 4000000LL;
                    lyricText.clear();
                    state = 2;
                } else {
                    state = 0;
                }
            } else if (state == 2) {
                if (trimmed.isEmpty()) {
                    if (!lyricText.isEmpty()) {
                        QString clipId = QString("clip_lyr_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
                        addClipToTrack(trackId, clipId, 2, startUs, durationUs, "", lyricText.trimmed());
                    }
                    state = 0;
                } else {
                    if (!lyricText.isEmpty()) lyricText += " ";
                    lyricText += trimmed;
                }
            }
        }
        if (state == 2 && !lyricText.isEmpty()) {
            QString clipId = QString("clip_lyr_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
            addClipToTrack(trackId, clipId, 2, startUs, durationUs, "", lyricText.trimmed());
        }
        track->blockSignals(false);
        emit track->clipsChanged();

        if (m_clipModels.contains(trackId)) {
            m_clipModels[trackId]->refresh();
        }

        recalculateTotalDuration();
        return true;
    }

    return false;
}

void TimelineManager::setShowVideoBackground(bool show) {
    if (m_showVideoBackground != show) {
        m_showVideoBackground = show;
        emit showVideoBackgroundChanged();
        setIsDirty(true);
    }
}

void TimelineManager::setLyricsOnlyMode(bool only) {
    // Legacy setter: maps bool to display mode (0 or 1)
    setLyricDisplayMode(only ? 1 : 0);
}

void TimelineManager::setLyricDisplayMode(int mode) {
    int clamped = qBound(0, mode, 3);
    if (m_lyricDisplayMode != clamped) {
        m_lyricDisplayMode = clamped;
        if (m_lyricEngine)
            m_lyricEngine->setDisplayMode(clamped);
        emit lyricDisplayModeChanged();
        setIsDirty(true);
    }
}

void TimelineManager::setExportAudioMode(int mode) {
    if (m_exportAudioMode != mode) {
        m_exportAudioMode = mode;
        emit exportAudioModeChanged();
        setIsDirty(true);
    }
}

QString TimelineManager::romanizeText(const QString& text) const {
    return Romanizer::romanize(text);
}

void TimelineManager::romanizeClip(QObject* clipObj) {
    Clip* clip = qobject_cast<Clip*>(clipObj);
    if (clip) {
        clip->romanize();
    }
}

double TimelineManager::alignAudioClip(const QString& targetClipId, const QString& referenceClipId) {
    Clip* targetClip = nullptr;
    Track* targetTrack = nullptr;
    for (Track* t : m_trackListModel->tracks()) {
        if (Clip* c = t->getClip(targetClipId)) {
            targetClip = c;
            targetTrack = t;
            break;
        }
    }
    if (!targetClip || targetClip->type() != Clip::Audio) return 0.0;

    Clip* refClip = nullptr;
    if (!referenceClipId.isEmpty()) {
        for (Track* t : m_trackListModel->tracks()) {
            if (Clip* c = t->getClip(referenceClipId)) {
                refClip = c;
                break;
            }
        }
    } else {
        // Find a fallback audio clip in another track
        for (Track* t : m_trackListModel->tracks()) {
            if (t == targetTrack) continue;
            for (Clip* c : t->clips()) {
                if (c->type() == Clip::Audio) {
                    refClip = c;
                    break;
                }
            }
            if (refClip) break;
        }
    }
    if (!refClip || refClip->type() != Clip::Audio) return 0.0;

    std::vector<float> targetSamples;
    int targetSampleRate = 48000;
    if (AudioEngine::instance()) {
        AudioReader* r = AudioEngine::instance()->getReader(targetClip->sourceFile());
        if (r) {
            targetSamples = r->samples();
            targetSampleRate = r->sampleRate();
        }
    }
    if (targetSamples.empty()) {
        AudioReader r;
        if (r.decodeFile(targetClip->sourceFile())) {
            targetSamples = r.samples();
            targetSampleRate = r.sampleRate();
        }
    }

    std::vector<float> refSamples;
    int refSampleRate = 48000;
    if (AudioEngine::instance()) {
        AudioReader* r = AudioEngine::instance()->getReader(refClip->sourceFile());
        if (r) {
            refSamples = r->samples();
            refSampleRate = r->sampleRate();
        }
    }
    if (refSamples.empty()) {
        AudioReader r;
        if (r.decodeFile(refClip->sourceFile())) {
            refSamples = r.samples();
            refSampleRate = r.sampleRate();
        }
    }

    if (targetSamples.empty() || refSamples.empty()) {
        return 0.0;
    }

    auto extractClipActiveEnvelope = [](const std::vector<float>& samples, int sampleRate, qint64 sourceStartUs, qint64 durationUs) -> std::vector<float> {
        double startSec = static_cast<double>(sourceStartUs) / 1000000.0;
        double durSec = static_cast<double>(durationUs) / 1000000.0;
        size_t startFrame = static_cast<size_t>(startSec * sampleRate);
        size_t durFrames = static_cast<size_t>(durSec * sampleRate);
        
        int channels = 2;
        size_t startSample = startFrame * channels;
        size_t durSamples = durFrames * channels;
        
        if (startSample >= samples.size()) {
            return std::vector<float>();
        }
        size_t endSample = (std::min)(startSample + durSamples, samples.size());
        
        int windowFrames = static_cast<int>(0.02 * sampleRate); // 20ms
        int step = windowFrames * channels;
        std::vector<float> env;
        env.reserve((endSample - startSample) / step + 1);
        
        for (size_t i = startSample; i < endSample; i += step) {
            float sum = 0.0f;
            size_t count = 0;
            size_t chunkEnd = (std::min)(i + step, endSample);
            for (size_t j = i; j < chunkEnd; ++j) {
                sum += std::abs(samples[j]);
                count++;
            }
            if (count > 0) {
                env.push_back(sum / count);
            } else {
                env.push_back(0.0f);
            }
        }
        return env;
    };

    std::vector<float> targetEnv = extractClipActiveEnvelope(targetSamples, targetSampleRate, targetClip->sourceStart(), targetClip->duration());
    std::vector<float> refEnv = extractClipActiveEnvelope(refSamples, refSampleRate, refClip->sourceStart(), refClip->duration());

    if (targetEnv.empty() || refEnv.empty()) {
        return 0.0;
    }

    qint64 targetStartUs = targetClip->startTime();
    qint64 refStartUs = refClip->startTime();
    qint64 initOffsetUs = targetStartUs - refStartUs;
    int initLagSteps = static_cast<int>(std::round(static_cast<double>(initOffsetUs) / 20000.0));

    double maxCorr = -1.0;
    int bestLagSteps = 0;
    bool foundPeak = false;

    // Search lag from -5s to +5s (in 20ms steps, so -250 to +250)
    int maxSearchSteps = 250; 
    for (int L = -maxSearchSteps; L <= maxSearchSteps; ++L) {
        int k = initLagSteps + L;
        double sum = 0.0;
        int count = 0;
        
        for (size_t i = 0; i < targetEnv.size(); ++i) {
            int refIdx = static_cast<int>(i) + k;
            if (refIdx >= 0 && refIdx < static_cast<int>(refEnv.size())) {
                sum += targetEnv[i] * refEnv[refIdx];
                count++;
            }
        }
        
        if (count > 10) {
            if (sum > maxCorr) {
                maxCorr = sum;
                bestLagSteps = L;
                foundPeak = true;
            }
        }
    }

    double offsetSeconds = 0.0;
    if (foundPeak) {
        qint64 offsetUs = static_cast<qint64>(bestLagSteps) * 20000LL;
        qint64 newStartTime = targetClip->startTime() + offsetUs;
        if (newStartTime < 0) {
            newStartTime = 0;
            offsetUs = -targetClip->startTime();
        }
        targetClip->setStartTime(newStartTime);
        offsetSeconds = static_cast<double>(offsetUs) / 1000000.0;
        emit timelineChanged();
    }
    return offsetSeconds;
}

bool TimelineManager::disableHwDecoding() const {
    QSettings settings("NC-KTV", "NC-KTV_V2");
    return settings.value("disable_hw_decoding", false).toBool();
}

void TimelineManager::setDisableHwDecoding(bool disable) {
    QSettings settings("NC-KTV", "NC-KTV_V2");
    if (settings.value("disable_hw_decoding", false).toBool() != disable) {
        settings.setValue("disable_hw_decoding", disable);
        emit disableHwDecodingChanged();
    }
}

void TimelineManager::restartApplication() {
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments());
    QCoreApplication::quit();
}

void TimelineManager::guessMetadataFromFilename(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString filename = fileInfo.completeBaseName(); // filename without extension

    // 1. Remove common noise suffixes
    static const QStringList suffixesToRemove = {
        " (karaoke)", " [karaoke]", " (instrumental)", " [instrumental]",
        " (vocal)", " [vocal]", " (vocals)", " [vocals]",
        " (official video)", " [official video]", " (official audio)", " [official audio]",
        " (lyrics video)", " (lyrics)", " [lyrics]",
        "_vocals", "_instruments", "_instrumental", "_spleeter", "_demucs"
    };

    for (const QString& suffix : suffixesToRemove) {
        int idx = filename.indexOf(suffix, 0, Qt::CaseInsensitive);
        while (idx != -1) {
            filename.remove(idx, suffix.length());
            idx = filename.indexOf(suffix, 0, Qt::CaseInsensitive);
        }
    }
    filename = filename.trimmed();

    // 2. Try to split by common delimiters
    QString guessedArtist = "";
    QString guessedTitle = "";

    int dashIdx = filename.indexOf(" - ");
    if (dashIdx == -1) {
        dashIdx = filename.indexOf(" -");
    }
    if (dashIdx == -1) {
        dashIdx = filename.indexOf("- ");
    }
    if (dashIdx == -1) {
        dashIdx = filename.indexOf(" _ ");
    }
    if (dashIdx == -1) {
        dashIdx = filename.indexOf("-");
    }

    if (dashIdx != -1) {
        guessedArtist = filename.left(dashIdx).trimmed();
        int delimLen = 1;
        if (filename.mid(dashIdx, 3) == " - " || filename.mid(dashIdx, 3) == " _ ") {
            delimLen = 3;
        } else if (filename.mid(dashIdx, 2) == " -" || filename.mid(dashIdx, 2) == "- ") {
            delimLen = 2;
        }
        guessedTitle = filename.mid(dashIdx + delimLen).trimmed();
    } else {
        guessedTitle = filename;
    }

    // Capitalize words nicely if they are all lowercase
    auto capitalize = [](QString str) -> QString {
        if (str.isEmpty()) return str;
        QStringList words = str.split(' ', Qt::SkipEmptyParts);
        for (int i = 0; i < words.size(); ++i) {
            if (!words[i].isEmpty()) {
                words[i][0] = words[i][0].toUpper();
            }
        }
        return words.join(' ');
    };

    if (!guessedArtist.isEmpty()) {
        guessedArtist = capitalize(guessedArtist);
        setArtistName(guessedArtist);
    }
    if (!guessedTitle.isEmpty()) {
        guessedTitle = capitalize(guessedTitle);
        setSongTitle(guessedTitle);
    }
}

void TimelineManager::detectSystemInfo() {
    // 1. OS Info
    m_osInfo = QSysInfo::prettyProductName() + " (" + QSysInfo::kernelVersion() + ")";

    // 2. CPU Info
#ifdef _WIN32
    QSettings cpuRegistry("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", QSettings::NativeFormat);
    m_cpuInfo = cpuRegistry.value("ProcessorNameString").toString().trimmed();
    if (m_cpuInfo.isEmpty()) {
        m_cpuInfo = QSysInfo::buildCpuArchitecture();
    }
#else
    m_cpuInfo = QSysInfo::buildCpuArchitecture();
#endif

    // 3. RAM Info
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        double totalGB = static_cast<double>(statex.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        double freeGB = static_cast<double>(statex.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
        m_ramInfo = QString("%1 GB Total (%2 GB Available)").arg(totalGB, 0, 'f', 1).arg(freeGB, 0, 'f', 1);
    } else {
        m_ramInfo = "Unknown RAM";
    }
#else
    m_ramInfo = "RAM Query Not Supported";
#endif

    // 4. GPU Info
#ifdef _WIN32
    QStringList gpus;
    IDXGIFactory* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
        IDXGIAdapter* pAdapter = nullptr;
        for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
                QString gpuName = QString::fromWCharArray(desc.Description);
                if (!gpuName.contains("Microsoft Basic Render Driver")) {
                    gpus.append(gpuName);
                }
            }
            pAdapter->Release();
        }
        pFactory->Release();
    }
    if (!gpus.isEmpty()) {
        m_gpuInfo = gpus.join(", ");
    } else {
        m_gpuInfo = "No Dedicated GPU Detected (Standard Basic Driver only)";
    }
#else
    m_gpuInfo = "GPU Query Not Supported";
#endif

    // 5. ONNX Provider Info
#if defined(NCKTV_HAS_ONNX) && NCKTV_HAS_ONNX
    try {
        std::vector<std::string> providers = Ort::GetAvailableProviders();
        QStringList list;
        for (const auto& p : providers) {
            QString name = QString::fromStdString(p);
            name.replace("ExecutionProvider", "");
            list.append(name);
        }
        m_onnxProviderInfo = list.join(", ");
    } catch (...) {
        m_onnxProviderInfo = "None";
    }
#else
    m_onnxProviderInfo = "Not Compiled";
#endif
}

void TimelineManager::downloadDefaultModel() {
    if (m_isDownloadingModel) return;

    QString targetDir = m_modelsDirPath;
    QDir dir(targetDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString targetFilePath = dir.filePath("Kim_Vocal_2.onnx");

    m_downloadFile = new QFile(targetFilePath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        m_modelDownloadStatusText = "Failed to create local file: " + m_downloadFile->errorString();
        emit modelDownloadStatusTextChanged();
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

    m_isDownloadingModel = true;
    m_modelDownloadProgress = 0.0;
    m_modelDownloadStatusText = "Connecting...";
    emit isDownloadingModelChanged();
    emit modelDownloadProgressChanged();
    emit modelDownloadStatusTextChanged();

    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
    }

    QUrl url("https://huggingface.co/seanghay/uvr_models/resolve/main/Kim_Vocal_2.onnx");
    startModelDownloadRequest(url);
}

void TimelineManager::cancelModelDownload() {
    if (!m_isDownloadingModel) return;

    m_modelDownloadStatusText = "Download cancelled.";
    emit modelDownloadStatusTextChanged();

    if (m_downloadReply) {
        m_downloadReply->abort();
    } else {
        if (m_downloadFile) {
            m_downloadFile->close();
            m_downloadFile->remove();
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        m_isDownloadingModel = false;
        emit isDownloadingModelChanged();
    }
}

void TimelineManager::startModelDownloadRequest(const QUrl& url) {
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, "NC-KTV/2.0");

    m_downloadReply = m_networkManager->get(request);

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile && m_downloadReply) {
            m_downloadFile->write(m_downloadReply->readAll());
        }
    });

    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            m_modelDownloadProgress = static_cast<double>(bytesReceived) / bytesTotal;
            emit modelDownloadProgressChanged();

            double mbReceived = static_cast<double>(bytesReceived) / (1024.0 * 1024.0);
            double mbTotal = static_cast<double>(bytesTotal) / (1024.0 * 1024.0);
            m_modelDownloadStatusText = QString("Downloading: %1 MB / %2 MB (%3%)")
                .arg(mbReceived, 0, 'f', 1)
                .arg(mbTotal, 0, 'f', 1)
                .arg(static_cast<int>(m_modelDownloadProgress * 100));
            emit modelDownloadStatusTextChanged();
        } else {
            double mbReceived = static_cast<double>(bytesReceived) / (1024.0 * 1024.0);
            m_modelDownloadStatusText = QString("Downloading: %1 MB")
                .arg(mbReceived, 0, 'f', 1);
            emit modelDownloadStatusTextChanged();
        }
    });

    connect(m_downloadReply, &QNetworkReply::finished, this, [this, url]() {
        if (!m_downloadReply) return;

        m_downloadReply->deleteLater();
        QNetworkReply::NetworkError replyError = m_downloadReply->error();

        // Check for manual redirect fallback
        QVariant redirectVal = m_downloadReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (replyError == QNetworkReply::NoError && redirectVal.isValid()) {
            QUrl redirectUrl = redirectVal.toUrl();
            QUrl nextUrl = url.resolved(redirectUrl);
            m_downloadReply = nullptr;

            m_modelDownloadStatusText = "Redirecting to CDN...";
            emit modelDownloadStatusTextChanged();

            startModelDownloadRequest(nextUrl);
            return;
        }

        if (replyError != QNetworkReply::NoError) {
            if (replyError == QNetworkReply::OperationCanceledError) {
                m_modelDownloadStatusText = "Download cancelled.";
            } else {
                m_modelDownloadStatusText = "Download failed: " + m_downloadReply->errorString();
            }
            emit modelDownloadStatusTextChanged();

            if (m_downloadFile) {
                m_downloadFile->close();
                m_downloadFile->remove(); // delete partial file
                delete m_downloadFile;
                m_downloadFile = nullptr;
            }
            m_isDownloadingModel = false;
            m_downloadReply = nullptr;
            emit isDownloadingModelChanged();
            return;
        }

        // Success!
        if (m_downloadFile) {
            m_downloadFile->close();
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }

        m_modelDownloadStatusText = "Download complete!";
        m_modelDownloadProgress = 1.0;
        emit modelDownloadStatusTextChanged();
        emit modelDownloadProgressChanged();

        m_isDownloadingModel = false;
        m_downloadReply = nullptr;
        emit isDownloadingModelChanged();

        scanModelsDir();
    });
}

} // namespace ncktv
