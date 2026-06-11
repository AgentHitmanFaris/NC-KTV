#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <nlohmann/json.hpp>

namespace ncktv {

class Clip : public QObject {
    Q_OBJECT
    
    Q_PROPERTY(QString clipId READ clipId CONSTANT)
    Q_PROPERTY(QString trackId READ trackId WRITE setTrackId NOTIFY trackIdChanged)
    Q_PROPERTY(int clipType READ clipType WRITE setClipType NOTIFY clipTypeChanged)
    Q_PROPERTY(qint64 startTime READ startTime WRITE setStartTime NOTIFY startTimeChanged)
    Q_PROPERTY(qint64 duration READ duration WRITE setDuration NOTIFY durationChanged)
    Q_PROPERTY(QString sourceFile READ sourceFile WRITE setSourceFile NOTIFY sourceFileChanged)
    Q_PROPERTY(qint64 sourceStart READ sourceStart WRITE setSourceStart NOTIFY sourceStartChanged)
    Q_PROPERTY(qint64 sourceDuration READ sourceDuration WRITE setSourceDuration NOTIFY sourceDurationChanged)
    Q_PROPERTY(QString lyricText READ lyricText WRITE setLyricText NOTIFY lyricTextChanged)
    Q_PROPERTY(qint64 endTime READ endTime NOTIFY startTimeChanged)
    Q_PROPERTY(QVariantList syllables READ syllables NOTIFY syllablesChanged)

public:
    enum Type {
        Audio = 0,
        Video = 1,
        Lyrics = 2
    };
    Q_ENUM(Type)

    explicit Clip(QObject* parent = nullptr);
    Clip(const QString& id, Type type, qint64 start, qint64 dur, QObject* parent = nullptr);
    virtual ~Clip() override = default;

    // Getters
    [[nodiscard]] QString clipId() const { return m_clipId; }
    [[nodiscard]] QString trackId() const { return m_trackId; }
    [[nodiscard]] int clipType() const { return static_cast<int>(m_clipType); }
    [[nodiscard]] Type type() const { return m_clipType; }
    [[nodiscard]] qint64 startTime() const { return m_startTime; }
    [[nodiscard]] qint64 duration() const { return m_duration; }
    [[nodiscard]] QString sourceFile() const { return m_sourceFile; }
    [[nodiscard]] qint64 sourceStart() const { return m_sourceStart; }
    [[nodiscard]] qint64 sourceDuration() const { return m_sourceDuration; }
    [[nodiscard]] QString lyricText() const { return m_lyricText; }
    [[nodiscard]] qint64 endTime() const { return m_startTime + m_duration; }
    [[nodiscard]] QVariantList syllables() const { return m_syllables; }

    // Setters (with signal emissions for reactive UI bindings)
    void setTrackId(const QString& id);
    void setClipType(int type);
    void setStartTime(qint64 start);
    void setDuration(qint64 dur);
    void setSourceFile(const QString& path);
    void setSourceStart(qint64 start);
    void setSourceDuration(qint64 dur);
    void setLyricText(const QString& text);
    void setSyllables(const QVariantList& list);

    // Timeline utility methods
    Q_INVOKABLE void moveTo(qint64 newStart);
    Q_INVOKABLE void resizeClip(qint64 newDuration, bool fromStart = false);
    
    // Parse syllable-level timings in LRC format: "Word1 <00:01.50> Word2 <00:02.00>"
    Q_INVOKABLE void parseLrcSyllables(const QString& rawLrc);
    
    // Update a single syllable's relative start time and duration
    Q_INVOKABLE void updateSyllable(int index, qint64 relativeStart, qint64 duration);
    
    // Update a single syllable's Bezier timing curve coordinates
    Q_INVOKABLE void updateSyllableCurve(int index, double x1, double y1, double x2, double y2);
    
    // Romanizes the lyric text and syllables in-place
    Q_INVOKABLE void romanize();
    
    // Generates word timings evenly spread across the clip's duration
    Q_INVOKABLE void autoGenerateSyllables();

    // Serialization
    [[nodiscard]] nlohmann::json toJson() const;
    static Clip* fromJson(const nlohmann::json& j, QObject* parent = nullptr);

signals:
    void trackIdChanged();
    void clipTypeChanged();
    void startTimeChanged();
    void durationChanged();
    void sourceFileChanged();
    void sourceStartChanged();
    void sourceDurationChanged();
    void lyricTextChanged();
    void syllablesChanged();

private:
    QString m_clipId;
    QString m_trackId;
    Type m_clipType = Audio;
    qint64 m_startTime = 0;      // In Microseconds
    qint64 m_duration = 0;       // In Microseconds
    QString m_sourceFile;
    qint64 m_sourceStart = 0;    // In Microseconds
    qint64 m_sourceDuration = 0; // In Microseconds
    QString m_lyricText;
    QVariantList m_syllables;    // Contains QVariantMaps with keys: text, relativeStart, duration
};

} // namespace ncktv
