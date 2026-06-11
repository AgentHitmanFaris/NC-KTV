#include "track_list_model.h"

namespace ncktv {

TrackListModel::TrackListModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int TrackListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_tracks.size();
}

QVariant TrackListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size()) {
        return {};
    }

    Track* track = m_tracks[index.row()];
    switch (role) {
        case TrackObjectRole:
            return QVariant::fromValue(track);
        case TrackIdRole:
            return track->trackId();
        case TrackTypeRole:
            return track->trackType();
        case TrackNameRole:
            return track->name();
        case TrackMutedRole:
            return track->isMuted();
        case TrackSoloRole:
            return track->isSolo();
        case TrackLockedRole:
            return track->isLocked();
        default:
            return {};
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[TrackObjectRole] = "trackObject";
    roles[TrackIdRole] = "trackId";
    roles[TrackTypeRole] = "trackType";
    roles[TrackNameRole] = "trackName";
    roles[TrackMutedRole] = "trackMuted";
    roles[TrackSoloRole] = "trackSolo";
    roles[TrackLockedRole] = "trackLocked";
    return roles;
}

void TrackListModel::addTrack(Track* track) {
    if (!track) return;

    // Track is now owned by the model for lifecycle management
    track->setParent(this);

    beginInsertRows(QModelIndex(), m_tracks.size(), m_tracks.size());
    m_tracks.append(track);
    endInsertRows();

    // Connect sub-signals to notify the model of specific property modifications
    connect(track, &Track::nameChanged, this, [this, track]() {
        int idx = m_tracks.indexOf(track);
        if (idx != -1) {
            auto qIdx = index(idx);
            emit dataChanged(qIdx, qIdx, {TrackNameRole});
        }
    });

    connect(track, &Track::isMutedChanged, this, [this, track]() {
        int idx = m_tracks.indexOf(track);
        if (idx != -1) {
            auto qIdx = index(idx);
            emit dataChanged(qIdx, qIdx, {TrackMutedRole});
        }
    });

    connect(track, &Track::isSoloChanged, this, [this, track]() {
        int idx = m_tracks.indexOf(track);
        if (idx != -1) {
            auto qIdx = index(idx);
            emit dataChanged(qIdx, qIdx, {TrackSoloRole});
        }
    });

    connect(track, &Track::isLockedChanged, this, [this, track]() {
        int idx = m_tracks.indexOf(track);
        if (idx != -1) {
            auto qIdx = index(idx);
            emit dataChanged(qIdx, qIdx, {TrackLockedRole});
        }
    });

    emit trackAdded(track);
}

bool TrackListModel::removeTrack(const QString& trackId) {
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i]->trackId() == trackId) {
            beginRemoveRows(QModelIndex(), i, i);
            Track* track = m_tracks[i];
            m_tracks.removeAt(i);
            endRemoveRows();

            track->disconnect(this);
            emit trackRemoved(trackId);
            track->deleteLater();
            return true;
        }
    }
    return false;
}

void TrackListModel::clear() {
    if (m_tracks.isEmpty()) return;

    beginResetModel();
    for (Track* track : m_tracks) {
        track->disconnect(this);
        track->deleteLater();
    }
    m_tracks.clear();
    endResetModel();
}

Track* TrackListModel::getTrackById(const QString& trackId) const {
    for (Track* track : m_tracks) {
        if (track->trackId() == trackId) {
            return track;
        }
    }
    if (trackId == QLatin1String("Track_Lyric")) {
        for (Track* track : m_tracks) {
            if (track->trackType() == Track::Lyrics) {
                return track;
            }
        }
    }
    return nullptr;
}

} // namespace ncktv
