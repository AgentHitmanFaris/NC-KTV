#include "clip_list_model.h"
#include "../track.h"

namespace ncktv {

ClipListModel::ClipListModel(QObject* parent)
    : QAbstractListModel(parent) {
}

ClipListModel::ClipListModel(Track* track, QObject* parent)
    : QAbstractListModel(parent),
      m_track(track) {
    setupConnections();
}

void ClipListModel::setTrack(Track* track) {
    if (m_track != track) {
        beginResetModel();
        teardownConnections();
        m_track = track;
        setupConnections();
        endResetModel();
        emit trackChanged();
    }
}

int ClipListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_track) {
        return 0;
    }
    return m_track->clips().size();
}

QVariant ClipListModel::data(const QModelIndex& index, int role) const {
    if (!m_track || !index.isValid() || index.row() < 0 || index.row() >= m_track->clips().size()) {
        return {};
    }

    Clip* clip = m_track->clips()[index.row()];
    switch (role) {
        case ClipObjectRole:
            return QVariant::fromValue(clip);
        case ClipIdRole:
            return clip->clipId();
        case ClipTypeRole:
            return clip->clipType();
        case ClipStartTimeRole:
            return clip->startTime();
        case ClipDurationRole:
            return clip->duration();
        case ClipEndTimeRole:
            return clip->endTime();
        case ClipSourceFileRole:
            return clip->sourceFile();
        case ClipLyricTextRole:
            return clip->lyricText();
        default:
            return {};
    }
}

QHash<int, QByteArray> ClipListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[ClipObjectRole] = "clipObject";
    roles[ClipIdRole] = "clipId";
    roles[ClipTypeRole] = "clipType";
    roles[ClipStartTimeRole] = "clipStartTime";
    roles[ClipDurationRole] = "clipDuration";
    roles[ClipEndTimeRole] = "clipEndTime";
    roles[ClipSourceFileRole] = "clipSourceFile";
    roles[ClipLyricTextRole] = "clipLyricText";
    return roles;
}

void ClipListModel::handleClipAdded(Clip* clip) {
    if (!m_track || !clip) return;
    
    // Determine the index of the newly added clip (since m_clips is kept sorted)
    int idx = m_track->clips().indexOf(clip);
    if (idx != -1) {
        beginInsertRows(QModelIndex(), idx, idx);
        // Connections are already wired in Track but let's observe changes for the view
        connect(clip, &Clip::startTimeChanged, this, [this, clip]() {
            handleClipsChanged();
        });
        connect(clip, &Clip::durationChanged, this, [this, clip]() {
            int row = m_track->clips().indexOf(clip);
            if (row != -1) {
                auto qIdx = index(row);
                emit dataChanged(qIdx, qIdx, {ClipDurationRole, ClipEndTimeRole});
            }
        });
        connect(clip, &Clip::lyricTextChanged, this, [this, clip]() {
            int row = m_track->clips().indexOf(clip);
            if (row != -1) {
                auto qIdx = index(row);
                emit dataChanged(qIdx, qIdx, {ClipLyricTextRole});
            }
        });
        connect(clip, &Clip::sourceFileChanged, this, [this, clip]() {
            int row = m_track->clips().indexOf(clip);
            if (row != -1) {
                auto qIdx = index(row);
                emit dataChanged(qIdx, qIdx, {ClipSourceFileRole});
            }
        });
        endInsertRows();
    }
}

void ClipListModel::handleClipRemoved(const QString& clipId) {
    // Re-synchronize using layout changes or find specific rows.
    // ClipsChanged is emitted immediately after. Let's do a full reset or local row detection
    handleClipsChanged();
}

void ClipListModel::handleClipsChanged() {
    beginResetModel();
    teardownConnections();
    setupConnections();
    endResetModel();
}

void ClipListModel::setupConnections() {
    if (!m_track) return;

    connect(m_track, &Track::clipAdded, this, &ClipListModel::handleClipAdded);
    connect(m_track, &Track::clipRemoved, this, &ClipListModel::handleClipRemoved);
    connect(m_track, &Track::clipsChanged, this, &ClipListModel::handleClipsChanged);

    // Bind current clips
    for (Clip* clip : m_track->clips()) {
        connect(clip, &Clip::startTimeChanged, this, [this, clip]() {
            handleClipsChanged();
        });
        connect(clip, &Clip::durationChanged, this, [this, clip]() {
            int row = m_track->clips().indexOf(clip);
            if (row != -1) {
                auto qIdx = index(row);
                emit dataChanged(qIdx, qIdx, {ClipDurationRole, ClipEndTimeRole});
            }
        });
        connect(clip, &Clip::lyricTextChanged, this, [this, clip]() {
            int row = m_track->clips().indexOf(clip);
            if (row != -1) {
                auto qIdx = index(row);
                emit dataChanged(qIdx, qIdx, {ClipLyricTextRole});
            }
        });
        connect(clip, &Clip::sourceFileChanged, this, [this, clip]() {
            int row = m_track->clips().indexOf(clip);
            if (row != -1) {
                auto qIdx = index(row);
                emit dataChanged(qIdx, qIdx, {ClipSourceFileRole});
            }
        });
    }
}

void ClipListModel::teardownConnections() {
    if (!m_track) return;

    m_track->disconnect(this);
    for (Clip* clip : m_track->clips()) {
        clip->disconnect(this);
    }
}

void ClipListModel::refresh() {
    beginResetModel();
    teardownConnections();
    setupConnections();
    endResetModel();
}

Clip* ClipListModel::getClip(int row) const {
    if (!m_track || row < 0 || row >= m_track->clips().size()) {
        return nullptr;
    }
    return m_track->clips()[row];
}

} // namespace ncktv
