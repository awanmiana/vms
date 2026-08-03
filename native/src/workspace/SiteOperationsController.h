#pragma once

// P3-18 / native increment 35: read-only front-layer aggregation keyed by the
// active premises. It composes existing trusted models; it never mutates them
// and never invents data for operating hours, uptime, duration, or analytics.

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class PremisesController;
class DeviceController;
class WorkspaceController;
namespace vms::persist { class SegmentIndex; }

class SiteOperationsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY changed)

public:
    SiteOperationsController(PremisesController* premises,
                             DeviceController* devices,
                             WorkspaceController* live,
                             vms::persist::SegmentIndex* recordings = nullptr,
                             QObject* parent = nullptr);

    QVariantMap snapshot() const { return snapshot_; }
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    PremisesController* premises_ = nullptr;
    DeviceController* devices_ = nullptr;
    WorkspaceController* live_ = nullptr;
    vms::persist::SegmentIndex* recordings_ = nullptr;
    QVariantMap snapshot_;
    QVariantMap recordingDurationCache_;
    QString recordingScopeKey_;
    QElapsedTimer recordingCacheAge_;
    QTimer clock_;
};
