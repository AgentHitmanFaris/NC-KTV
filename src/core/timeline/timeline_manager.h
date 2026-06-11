#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariantList>
#include <QUrl>
#include "track.h"
#include "lyric_engine.h"
#include "models/track_list_model.h"
#include "models/clip_list_model.h"

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace ncktv {

class TimelineManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList mediaList READ mediaList WRITE setMediaList NOTIFY mediaListChanged)
    Q_PROPERTY(bool isDirty READ isDirty WRITE setIsDirty NOTIFY isDirtyChanged)
    
    // Subtitle styles and timeline markers properties
    Q_PROPERTY(QVariantList markers READ markers WRITE setMarkers NOTIFY markersChanged)
    Q_PROPERTY(QString subtitleFontFamily READ subtitleFontFamily WRITE setSubtitleFontFamily NOTIFY subtitleFontFamilyChanged)
    Q_PROPERTY(int subtitleFontSize READ subtitleFontSize WRITE setSubtitleFontSize NOTIFY subtitleFontSizeChanged)
    Q_PROPERTY(QString subtitleFillColor READ subtitleFillColor WRITE setSubtitleFillColor NOTIFY subtitleFillColorChanged)
    Q_PROPERTY(QString subtitleActiveColor READ subtitleActiveColor WRITE setSubtitleActiveColor NOTIFY subtitleActiveColorChanged)
    Q_PROPERTY(QString subtitleOutlineColor READ subtitleOutlineColor WRITE setSubtitleOutlineColor NOTIFY subtitleOutlineColorChanged)
    Q_PROPERTY(int subtitleOutlineWidth READ subtitleOutlineWidth WRITE setSubtitleOutlineWidth NOTIFY subtitleOutlineWidthChanged)


    Q_PROPERTY(QString songTitle READ songTitle WRITE setSongTitle NOTIFY songTitleChanged)
    Q_PROPERTY(QString artistName READ artistName WRITE setArtistName NOTIFY artistNameChanged)
    Q_PROPERTY(int introSplashDuration READ introSplashDuration WRITE setIntroSplashDuration NOTIFY introSplashDurationChanged)
    Q_PROPERTY(QString endingVideoPath READ endingVideoPath WRITE setEndingVideoPath NOTIFY endingVideoPathChanged)

    Q_PROPERTY(qint64 currentPlayheadTime READ currentPlayheadTime WRITE setCurrentPlayheadTime NOTIFY currentPlayheadTimeChanged)
    Q_PROPERTY(qint64 totalDuration READ totalDuration WRITE setTotalDuration NOTIFY totalDurationChanged)
    Q_PROPERTY(double fps READ fps WRITE setFps NOTIFY fpsChanged)
    Q_PROPERTY(TrackListModel* trackListModel READ trackListModel CONSTANT)
    Q_PROPERTY(bool isSeparating READ isSeparating NOTIFY isSeparatingChanged)
    Q_PROPERTY(double separationProgress READ separationProgress NOTIFY separationProgressChanged)
    Q_PROPERTY(QString separationStatusText READ separationStatusText NOTIFY separationStatusTextChanged)
    Q_PROPERTY(bool isRendering READ isRendering NOTIFY isRenderingChanged)
    Q_PROPERTY(double renderProgress READ renderProgress NOTIFY renderProgressChanged)
    Q_PROPERTY(QString renderStatusText READ renderStatusText NOTIFY renderStatusTextChanged)
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY modelPathChanged)
    Q_PROPERTY(QStringList discoveredModels READ discoveredModels NOTIFY discoveredModelsChanged)
    Q_PROPERTY(QStringList discoveredModelPaths READ discoveredModelPaths NOTIFY discoveredModelsChanged)
    Q_PROPERTY(QString modelsDirPath READ modelsDirPath WRITE setModelsDirPath NOTIFY modelsDirPathChanged)
    Q_PROPERTY(bool isDownloadingModel READ isDownloadingModel NOTIFY isDownloadingModelChanged)
    Q_PROPERTY(double modelDownloadProgress READ modelDownloadProgress NOTIFY modelDownloadProgressChanged)
    Q_PROPERTY(QString modelDownloadStatusText READ modelDownloadStatusText NOTIFY modelDownloadStatusTextChanged)
    Q_PROPERTY(bool showVideoBackground READ showVideoBackground WRITE setShowVideoBackground NOTIFY showVideoBackgroundChanged)
    Q_PROPERTY(bool lyricsOnlyMode READ lyricsOnlyMode WRITE setLyricsOnlyMode NOTIFY lyricDisplayModeChanged)
    Q_PROPERTY(int lyricDisplayMode READ lyricDisplayMode WRITE setLyricDisplayMode NOTIFY lyricDisplayModeChanged)
    Q_PROPERTY(int exportAudioMode READ exportAudioMode WRITE setExportAudioMode NOTIFY exportAudioModeChanged)
    Q_PROPERTY(bool showSourceMonitor READ showSourceMonitor WRITE setShowSourceMonitor NOTIFY showSourceMonitorChanged)
    Q_PROPERTY(LyricEngine* lyricEngine READ lyricEngine CONSTANT)
    Q_PROPERTY(bool disableHwDecoding READ disableHwDecoding WRITE setDisableHwDecoding NOTIFY disableHwDecodingChanged)
    Q_PROPERTY(QString cpuInfo READ cpuInfo CONSTANT)
    Q_PROPERTY(QString gpuInfo READ gpuInfo CONSTANT)
    Q_PROPERTY(QString ramInfo READ ramInfo CONSTANT)
    Q_PROPERTY(QString osInfo READ osInfo CONSTANT)
    Q_PROPERTY(QString onnxProviderInfo READ onnxProviderInfo CONSTANT)

public:
    explicit TimelineManager(QObject* parent = nullptr);
    virtual ~TimelineManager() override;

    Q_INVOKABLE [[nodiscard]] bool disableHwDecoding() const;
    Q_INVOKABLE void setDisableHwDecoding(bool disable);
    Q_INVOKABLE void restartApplication();

    [[nodiscard]] QString modelPath() const { return m_modelPath; }
    void setModelPath(const QString& path);

    [[nodiscard]] QString modelsDirPath() const { return m_modelsDirPath; }
    void setModelsDirPath(const QString& path);

    [[nodiscard]] QStringList discoveredModels() const { return m_discoveredModels; }
    [[nodiscard]] QStringList discoveredModelPaths() const { return m_discoveredModelPaths; }
    [[nodiscard]] QString cpuInfo() const { return m_cpuInfo; }
    [[nodiscard]] QString gpuInfo() const { return m_gpuInfo; }
    [[nodiscard]] QString ramInfo() const { return m_ramInfo; }
    [[nodiscard]] QString osInfo() const { return m_osInfo; }
    [[nodiscard]] QString onnxProviderInfo() const { return m_onnxProviderInfo; }

    Q_INVOKABLE void scanModelsDir();

    [[nodiscard]] bool isSeparating() const { return m_isSeparating; }
    [[nodiscard]] double separationProgress() const { return m_separationProgress; }
    [[nodiscard]] QString separationStatusText() const { return m_separationStatusText; }

    [[nodiscard]] bool isDownloadingModel() const { return m_isDownloadingModel; }
    [[nodiscard]] double modelDownloadProgress() const { return m_modelDownloadProgress; }
    [[nodiscard]] QString modelDownloadStatusText() const { return m_modelDownloadStatusText; }

    Q_INVOKABLE void downloadDefaultModel();
    Q_INVOKABLE void cancelModelDownload();

    [[nodiscard]] bool isRendering() const { return m_isRendering; }
    [[nodiscard]] double renderProgress() const { return m_renderProgress; }
    [[nodiscard]] QString renderStatusText() const { return m_renderStatusText; }

    [[nodiscard]] QVariantList mediaList() const { return m_mediaList; }
    void setMediaList(const QVariantList& list);

    [[nodiscard]] bool isDirty() const { return m_isDirty; }
    void setIsDirty(bool dirty);
    Q_INVOKABLE void setDirty(bool dirty = true) { setIsDirty(dirty); }

    // Subtitle style getters & setters
    [[nodiscard]] QString subtitleFontFamily() const { return m_subtitleFontFamily; }
    void setSubtitleFontFamily(const QString& family);

    [[nodiscard]] int subtitleFontSize() const { return m_subtitleFontSize; }
    void setSubtitleFontSize(int size);

    [[nodiscard]] QString subtitleFillColor() const { return m_subtitleFillColor; }
    void setSubtitleFillColor(const QString& color);

    [[nodiscard]] QString subtitleActiveColor() const { return m_subtitleActiveColor; }
    void setSubtitleActiveColor(const QString& color);

    [[nodiscard]] QString subtitleOutlineColor() const { return m_subtitleOutlineColor; }
    void setSubtitleOutlineColor(const QString& color);

    [[nodiscard]] int subtitleOutlineWidth() const { return m_subtitleOutlineWidth; }
    void setSubtitleOutlineWidth(int width);

    // Markers getter, setter, and operations
    [[nodiscard]] QVariantList markers() const { return m_markers; }
    void setMarkers(const QVariantList& list);
    Q_INVOKABLE void addMarker(qint64 timeUs, const QString& name = "Marker", const QString& color = "green");
    Q_INVOKABLE void removeMarker(const QString& markerId);
    Q_INVOKABLE void updateMarker(const QString& markerId, const QString& name, const QString& color);

    [[nodiscard]] bool showVideoBackground() const { return m_showVideoBackground; }
    void setShowVideoBackground(bool show);

    // Backward-compatible computed getter (true when displayMode == CenterScrollQueue)
    [[nodiscard]] bool lyricsOnlyMode() const { return m_lyricDisplayMode == 1; }
    void setLyricsOnlyMode(bool only); // Legacy setter for QML/render compatibility

    [[nodiscard]] int lyricDisplayMode() const { return m_lyricDisplayMode; }
    void setLyricDisplayMode(int mode);

    [[nodiscard]] LyricEngine* lyricEngine() const { return m_lyricEngine; }

    [[nodiscard]] int exportAudioMode() const { return m_exportAudioMode; }
    void setExportAudioMode(int mode);

    [[nodiscard]] bool showSourceMonitor() const { return m_showSourceMonitor; }
    void setShowSourceMonitor(bool show);

    [[nodiscard]] QString songTitle() const { return m_songTitle; }
    void setSongTitle(const QString& title);

    [[nodiscard]] QString artistName() const { return m_artistName; }
    void setArtistName(const QString& name);

    [[nodiscard]] int introSplashDuration() const { return m_introSplashDuration; }
    void setIntroSplashDuration(int durationMs);

    [[nodiscard]] QString endingVideoPath() const { return m_endingVideoPath; }
    void setEndingVideoPath(const QString& path);

    Q_INVOKABLE [[nodiscard]] QString resolveEndingVideoPath() const;


    [[nodiscard]] qint64 currentPlayheadTime() const { return m_currentPlayheadTime; }
    void setCurrentPlayheadTime(qint64 timeMicroseconds);

    [[nodiscard]] qint64 totalDuration() const { return m_totalDuration; }
    void setTotalDuration(qint64 durationMicroseconds);

    [[nodiscard]] double fps() const { return m_fps; }
    void setFps(double fpsValue);

    [[nodiscard]] TrackListModel* trackListModel() const { return m_trackListModel; }

    // Track Management
    Q_INVOKABLE QString addTrack(int type, const QString& name, const QString& trackId = QString());
    Q_INVOKABLE bool removeTrack(const QString& trackId);
    Q_INVOKABLE QObject* getClipModelForTrack(const QString& trackId);

    // Timeline Operations
    Q_INVOKABLE bool addClipToTrack(const QString& trackId, const QString& clipId, int type, qint64 startTime, qint64 duration, const QString& sourceFile = "", const QString& lyricText = "");
    Q_INVOKABLE bool addClipToTrackWithSourceStart(const QString& trackId, const QString& clipId, int type, qint64 startTime, qint64 duration, qint64 sourceStart, const QString& sourceFile = "", const QString& lyricText = "");
    Q_INVOKABLE bool importLyricsFromFile(const QString& trackId, const QString& filePath);
    Q_INVOKABLE bool importLyricsFromString(const QString& trackId, const QString& rawLrcContent);
    Q_INVOKABLE bool splitClip(const QString& trackId, const QString& clipId, qint64 splitTimeMicroseconds);
    Q_INVOKABLE void separateStems(const QString& clipId);
    Q_INVOKABLE void separateStemsForFile(const QString& filePath);
    Q_INVOKABLE void startExport(const QString& outputPath, int width, int height, int fps, int videoBitrate, int audioBitrate, const QString& audioCodec = "AAC");
    Q_INVOKABLE void cancelExport();
    
    // Romanization
    Q_INVOKABLE [[nodiscard]] QString romanizeText(const QString& text) const;
    Q_INVOKABLE void romanizeClip(QObject* clipObj);
    
    // Vocal Alignment Helper
    Q_INVOKABLE double alignAudioClip(const QString& targetClipId, const QString& referenceClipId = QString());
    
    // Snapping Engine
    Q_INVOKABLE qint64 checkSnapping(const QString& excludeClipId, qint64 targetTimeMicroseconds, qint64 thresholdMicroseconds) const;
    
    // Collision Engine
    Q_INVOKABLE qint64 checkCollisions(const QString& trackId, const QString& clipId, qint64 targetTimeMicroseconds) const;

    // Timecode Utilities
    Q_INVOKABLE [[nodiscard]] QString formatTimecode(qint64 microseconds) const;
    Q_INVOKABLE [[nodiscard]] qint64 parseTimecode(const QString& timecode) const;
    Q_INVOKABLE [[nodiscard]] qint64 timeToFrames(qint64 microseconds) const;
    Q_INVOKABLE [[nodiscard]] qint64 framesToTime(qint64 frames) const;

    // Project Serialization (.nctv JSON files)
    Q_INVOKABLE bool saveProject(const QString& filePath);
    Q_INVOKABLE bool loadProject(const QString& filePath);
    Q_INVOKABLE void clearProject();

signals:
    void currentPlayheadTimeChanged();
    void totalDurationChanged();
    void fpsChanged();
    void projectLoaded();
    void projectCleared();
    void isSeparatingChanged();
    void separationProgressChanged();
    void separationStatusTextChanged();
    void isRenderingChanged();
    void renderProgressChanged();
    void renderStatusTextChanged();
    void exportCompleted(const QString& outputPath);
    void exportFailed(const QString& errorMessage);
    void modelPathChanged();
    void modelsDirPathChanged();
    void discoveredModelsChanged();
    void isDownloadingModelChanged();
    void modelDownloadProgressChanged();
    void modelDownloadStatusTextChanged();
    void mediaListChanged();
    void isDirtyChanged();
    void mediaSeparationCompleted(const QString& vocalsPath, const QString& instPath);
    void mediaSeparationFailed(const QString& errorMessage);
    void timelineChanged();
    
    // Subtitle style and markers signals
    void subtitleFontFamilyChanged();
    void subtitleFontSizeChanged();
    void subtitleFillColorChanged();
    void subtitleActiveColorChanged();
    void subtitleOutlineColorChanged();
    void subtitleOutlineWidthChanged();
    void markersChanged();
    void showVideoBackgroundChanged();
    void lyricDisplayModeChanged();
    void exportAudioModeChanged();
    void showSourceMonitorChanged();
    void songTitleChanged();
    void artistNameChanged();
    void introSplashDurationChanged();
    void endingVideoPathChanged();
    void disableHwDecodingChanged();


private slots:
    void onSeparationProgress(double fraction);
    void onSeparationCompleted(const QString& clipId, const QString& vocalsPath, const QString& instPath);
    void onSeparationFailed(const QString& clipId, const QString& errorMessage);
    void onRenderProgress(double fraction);
    void onRenderStatusText(const QString& text);
    void onRenderCompleted(const QString& outputPath, const QString& videoCodec, const QString& audioCodec);
    void onRenderFailed(const QString& errorMessage);

private:
    void recalculateTotalDuration();
    void guessMetadataFromFilename(const QString& filePath);

    qint64 m_currentPlayheadTime = 0; // In Microseconds
    qint64 m_totalDuration = 0;       // In Microseconds
    double m_fps = 30.0;              // Default NLE FPS

    TrackListModel* m_trackListModel = nullptr;
    QMap<QString, ClipListModel*> m_clipModels; // Managed Clip models per trackId

    bool m_isSeparating = false;
    double m_separationProgress = 0.0;
    QString m_separationStatusText;
    class StemSeparationWorker* m_separationWorker = nullptr;

    bool m_isRendering = false;
    double m_renderProgress = 0.0;
    QString m_renderStatusText;
    class RenderWorker* m_renderWorker = nullptr;

    QString m_modelPath;
    QString m_modelsDirPath;
    QStringList m_discoveredModels;
    QStringList m_discoveredModelPaths;
    QVariantList m_mediaList;
    bool m_isDirty = false;

    bool m_isDownloadingModel = false;
    double m_modelDownloadProgress = 0.0;
    QString m_modelDownloadStatusText;
    QNetworkAccessManager* m_networkManager = nullptr;
    QNetworkReply* m_downloadReply = nullptr;
    QFile* m_downloadFile = nullptr;

    void startModelDownloadRequest(const QUrl& url);

    // Subtitle style and markers variables
    QVariantList m_markers;
    QString m_subtitleFontFamily = "Outfit";
    int m_subtitleFontSize = 24;
    QString m_subtitleFillColor = "#4A4A5A";
    QString m_subtitleActiveColor = "#00E676";
    QString m_subtitleOutlineColor = "#08080A";
    int m_subtitleOutlineWidth = 2;

    bool m_showVideoBackground = true;
    int m_lyricDisplayMode = 0; // 0=BottomTwoLine, 1=CenterScroll, 2=WordBounce, 3=Cinematic
    int m_exportAudioMode = 0; // 0 = Full, 1 = Instrumental Only
    bool m_showSourceMonitor = true;
    LyricEngine* m_lyricEngine = nullptr;

    QString m_songTitle = "Untitled Song";
    QString m_artistName = "Unknown Artist";
    int m_introSplashDuration = 3000;
    QString m_endingVideoPath = "splash_screen/end.mp4";

    QString m_cpuInfo;
    QString m_gpuInfo;
    QString m_ramInfo;
    QString m_osInfo;
    QString m_onnxProviderInfo;
    void detectSystemInfo();
};


} // namespace ncktv
