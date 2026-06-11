#pragma once

#include <QAbstractListModel>
#include <QList>
#include "../track.h"

namespace ncktv {

class TrackListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum TrackRoles {
        TrackObjectRole = Qt::UserRole + 1,
        TrackIdRole,
        TrackTypeRole,
        TrackNameRole,
        TrackMutedRole,
        TrackSoloRole,
        TrackLockedRole
    };
    Q_ENUM(TrackRoles)

    explicit TrackListModel(QObject* parent = nullptr);
    virtual ~TrackListModel() override = default;

    // QAbstractItemModel interface
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Model management
    void addTrack(Track* track);
    bool removeTrack(const QString& trackId);
    void clear();
    
    [[nodiscard]] Q_INVOKABLE QList<Track*> tracks() const { return m_tracks; }
    [[nodiscard]] Q_INVOKABLE Track* getTrackById(const QString& trackId) const;

signals:
    void trackAdded(Track* track);
    void trackRemoved(const QString& trackId);

private:
    QList<Track*> m_tracks;
};

} // namespace ncktv
