#pragma once

#include <QAbstractListModel>
#include "../clip.h"

#include "../track.h"

namespace ncktv {

class ClipListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(Track* track READ track WRITE setTrack NOTIFY trackChanged)

public:
    enum ClipRoles {
        ClipObjectRole = Qt::UserRole + 1,
        ClipIdRole,
        ClipTypeRole,
        ClipStartTimeRole,
        ClipDurationRole,
        ClipEndTimeRole,
        ClipSourceFileRole,
        ClipLyricTextRole
    };
    Q_ENUM(ClipRoles)

    explicit ClipListModel(QObject* parent = nullptr);
    explicit ClipListModel(Track* track, QObject* parent = nullptr);
    virtual ~ClipListModel() override = default;

    [[nodiscard]] Track* track() const { return m_track; }
    void setTrack(Track* track);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE ncktv::Clip* getClip(int row) const;

    // QAbstractItemModel interface
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

signals:
    void trackChanged();

private slots:
    void handleClipAdded(Clip* clip);
    void handleClipRemoved(const QString& clipId);
    void handleClipsChanged();

private:
    void setupConnections();
    void teardownConnections();

    Track* m_track = nullptr;
};

} // namespace ncktv
