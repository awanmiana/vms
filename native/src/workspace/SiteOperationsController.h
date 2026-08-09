#pragma once

// P3-18 / native increment 35: read-only front-layer aggregation keyed by the
// active premises. It composes existing trusted models, checkpoints monotonic
// decoded-stream observations, and summarizes only site-attributable alarms;
// it never invents process-downtime duration, VCA results, or event history.

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class PremisesController;
class DeviceController;
class WorkspaceController;
class AlarmController;
namespace vms::persist { class SegmentIndex; }
namespace vms::persist { class StreamDurationRepo; }

class SiteOperationsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY changed)

public:
    SiteOperationsController(PremisesController* premises,
                             DeviceController* devices,
                             WorkspaceController* live,
                             vms::persist::SegmentIndex* recordings = nullptr,
                             vms::persist::StreamDurationRepo* streams = nullptr,
                             AlarmController* alarms = nullptr,
                             QObject* parent = nullptr);

    QVariantMap snapshot() const { return snapshot_; }
    Q_INVOKABLE void refresh();
    void flushStreaming();

signals:
    void changed();

private:
    PremisesController* premises_ = nullptr;
    DeviceController* devices_ = nullptr;
    WorkspaceController* live_ = nullptr;
    vms::persist::SegmentIndex* recordings_ = nullptr;
    vms::persist::StreamDurationRepo* streams_ = nullptr;
    AlarmController* alarms_ = nullptr;
    QVariantMap snapshot_;
    QVariantMap recordingDurationCache_;
    QString recordingScopeKey_;
    QElapsedTimer recordingCacheAge_;
    QElapsedTimer streamingObservationAge_;
    QString observedStreamingSiteId_;
    int observedPlayingBranches_ = 0;
    QString streamingCheckpointError_;
    QTimer clock_;

    void advanceStreamingObservation(const QString& nextSiteId,
                                     int nextPlayingBranches);
};
