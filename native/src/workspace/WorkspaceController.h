#pragma once

// P3-14 slice 1 — honest per-tile state, rendered.
//
// The governor already computes an honest per-tile state (Live / Degraded /
// PausedOffscreen / PausedCapacity) and vms_grid prints it to the console
// (`tile 5 [thumb/degraded]`). This controller is the bridge that turns those
// same decisions into a data model a Qt Quick view binds to, so acceptance
// criterion #5 ("every non-decoding tile shows a clear non-live indicator; no
// stale frame presented as live") is met *on screen* and not only in a log.
//
// It drives the identical governor semantics vms_grid uses: a square grid of
// tiles all requesting Main, tile 0 focused/High, the rest Medium; a focus
// sweep advances round-robin, re-plans via GovernorSession, and the model
// refreshes. There is no media pipeline in this slice — routing the live
// d3d11-composited video underneath the state chrome is the next slice. The
// point proven here is the Qt Quick + governor integration and the honest
// visual state, both dev-box verifiable without a camera.

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <vector>

#include "governor/Governor.h"

class WorkspaceController : public QObject {
    Q_OBJECT
    // The per-tile model the QML grid repeats over. Each entry is a QVariantMap
    // { id, tier, state, degraded, focused, badge }. Rebuilt on every re-plan.
    Q_PROPERTY(QVariantList tiles READ tiles NOTIFY changed)
    Q_PROPERTY(int columns READ columns NOTIFY changed)
    Q_PROPERTY(int rows READ rows NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    // Human-readable aggregate capacity meter, e.g.
    // "decode 24.9 / 64 main-eq  ·  mem 2600 / 6000 MB  ·  decoding 40 / 64".
    Q_PROPERTY(QString capacity READ capacity NOTIFY changed)
    Q_PROPERTY(QString profileLabel READ profileLabel CONSTANT)
    Q_PROPERTY(bool overflow READ overflow NOTIFY changed)

public:
    WorkspaceController(vms::CapacityProfile profile, int count,
                        QObject* parent = nullptr);

    QVariantList tiles() const { return tiles_; }
    int columns() const { return columns_; }
    int rows() const { return rows_; }
    int focusIndex() const { return focusIndex_; }
    QString capacity() const { return capacity_; }
    QString profileLabel() const { return profileLabel_; }
    bool overflow() const { return overflow_; }

    // Advance focus one cell, re-plan, refresh the model. Returns a one-line
    // human summary of the transitions (used by --selftest and logged live).
    Q_INVOKABLE QString sweep();

    // Begin an automatic focus sweep every intervalMs (<= 0 leaves it manual).
    void startAutoSweep(int intervalMs);

    // A plain-text dump of the current plan, one line per tile, mirroring the
    // vms_grid console form. Used by --selftest.
    QString dumpPlan() const;

    // The current per-tile tiers, indexed by tile id (absent tiles => Paused).
    // The video grid (slice 2b) builds its branches from exactly this plan so
    // the picture matches the chrome.
    std::vector<vms::Tier> currentTiers() const;

signals:
    void changed();

private:
    void rebuildModel();

    vms::GovernorSession session_;
    std::vector<vms::TileRequest> requests_;
    vms::GovernorResult plan_;
    QVariantList tiles_;
    int columns_ = 1;
    int rows_ = 1;
    int focusIndex_ = 0;
    QString capacity_;
    QString profileLabel_;
    bool overflow_ = false;
    QTimer timer_;
};
