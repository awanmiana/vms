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
    // A one-line live optimizer read-out (inc 8): what the health sampler adjusted
    // and why, e.g. "optimizer: mem 480/1000 MB (ram) · cpu 34%". Empty until the
    // sampler runs. Honest, read-only advice — the optimizer never mutates the OS.
    Q_PROPERTY(QString optimizer READ optimizer NOTIFY changed)
    Q_PROPERTY(bool overflow READ overflow NOTIFY changed)
    // Whether the automatic focus sweep is running (a tile click stops it; the
    // toolbar toggles it). Lets the UI show an honest Auto ▶ / ⏸ control.
    Q_PROPERTY(bool autoSweeping READ autoSweeping NOTIFY sweepingChanged)

public:
    WorkspaceController(vms::CapacityProfile profile, int count,
                        QObject* parent = nullptr);

    QVariantList tiles() const { return tiles_; }
    int columns() const { return columns_; }
    int rows() const { return rows_; }
    int focusIndex() const { return focusIndex_; }
    QString capacity() const { return capacity_; }
    QString profileLabel() const { return profileLabel_; }
    QString optimizer() const { return optimizer_; }
    bool overflow() const { return overflow_; }

    // inc 8: the machine's STATIC capacity ceiling (from the hardware probe /
    // calibration), which the optimizer adjusts down from each sample.
    const vms::CapacityProfile& baseProfile() const { return baseProfile_; }

    // inc 8: apply an optimizer-adjusted capacity profile to the live session. If
    // it differs from the profile currently in force, the session re-plans under
    // the new budget (degrade under pressure / recover when it clears) and the UI
    // refreshes; `reportText` is shown in the optimizer read-out. A no-op (no
    // re-plan, no flicker) when the profile is unchanged.
    void applyOptimizedProfile(const vms::CapacityProfile& profile,
                               const QString& reportText);
    bool autoSweeping() const;

    // Advance focus one cell, re-plan, refresh the model. Returns a one-line
    // human summary of the transitions (used by --selftest and logged live).
    Q_INVOKABLE QString sweep();

    // Operator picks the working-set tile: focus `id` (High priority), re-plan,
    // refresh. Stops the automatic sweep — the operator has taken control. Called
    // from QML on a tile click; this is the input the governor exists to serve.
    Q_INVOKABLE void focusTile(int id);

    // Operator sets a camera's device-activity priority (level: 3=High, 2=Medium,
    // 1=Low). Independent of focus and persistent across sweeps: under capacity
    // pressure the governor degrades lower-priority cameras first, so a High mark
    // keeps that camera at a better tier even while the operator looks elsewhere.
    Q_INVOKABLE void setPriority(int id, int level);

    // Operator sets a camera's desired media tier — the quality ceiling (the
    // first control axis). level: 3=Main, 2=Sub, 1=Thumb, 0=Off. The governor
    // never exceeds it, so capping a camera (or turning it Off) frees decode and
    // memory budget for the others. Off means the camera does not decode at all.
    Q_INVOKABLE void setDesiredTier(int id, int level);

    // Change the wall layout to `count` tiles (a square-ish grid). Rebuilds the
    // working set and re-plans from scratch. Fewer tiles means the governor can
    // upgrade more of them to Main; more tiles means it degrades to fit budget.
    Q_INVOKABLE void setTileCount(int count);

    // --- Spatial canvas (inc 24, P3-15/P3-01) -------------------------------
    // Operator drags a tile to a new world position on the spatial canvas
    // (P3-01's drag verb). Updates the model's px/py in place — positions are
    // presentation, not a governor input, so no re-plan happens here.
    Q_INVOKABLE void setTilePos(int id, double x, double y);

    // The viewport media policy (P3-15): given the current canvas viewport
    // (view size, zoom, pan offset) classify every tile by zone × zoom level
    // (the prototype-exact SpatialPolicy) and drive the governor from it in ONE
    // batched re-plan — culled tiles become visible=false (truly not decoding),
    // on-screen tiles get their zone tier as the desired ceiling. Downgrades
    // apply immediately; promotions only when `settled` (the 300 ms dwell,
    // owned by the QML settle timer). A no-change pass does not re-plan.
    Q_INVOKABLE void updateSpatialViewport(double viewW, double viewH,
                                           double zoom, double offX,
                                           double offY, bool settled);

    // "site" / "wing" / "room" for the header read-out (single-sourced from
    // SpatialPolicy rather than re-deriving thresholds in QML).
    Q_INVOKABLE QString zoomLevelName(double zoom) const;

    // Persistence accessors for the spatial positions (schema v9).
    double tilePosX(int id) const;
    double tilePosY(int id) const;

    // Begin an automatic focus sweep every intervalMs (<= 0 leaves it manual).
    void startAutoSweep(int intervalMs);

    // Toolbar control: turn the automatic sweep back on (at the interval passed to
    // startAutoSweep) or off. Turning it off leaves the operator's focus in place.
    Q_INVOKABLE void setAutoSweep(bool on);

    // A plain-text dump of the current plan, one line per tile, mirroring the
    // vms_grid console form. Used by --selftest.
    QString dumpPlan() const;

    // The current per-tile tiers, indexed by tile id (absent tiles => Paused).
    // The video grid (slice 2b) builds its branches from exactly this plan so
    // the picture matches the chrome.
    std::vector<vms::Tier> currentTiers() const;

    // Persistence accessors (P0-04 inc 5c): the operator's per-tile overrides, so
    // the layout can be saved and restored across a restart. Tier/priority are
    // returned as their enum int values.
    int tileCount() const { return static_cast<int>(requests_.size()); }
    int desiredTierOf(int id) const;
    int priorityOf(int id) const;

signals:
    void changed();
    // Emitted whenever the plan is (re)built — a sweep, a click, or startup. In
    // --video mode the app connects this to reconfigure the live grid, so the
    // picture follows the chrome no matter what triggered the re-plan.
    void planChanged();
    void sweepingChanged();
    // Emitted when the tile COUNT changes (not just tiers). In --video mode the
    // app rebuilds the whole grid pipeline for this, rather than re-tiering the
    // existing branches (which planChanged does).
    void layoutChanged();

private:
    void rebuildModel(bool isLayoutChange = false);

    vms::GovernorSession session_;
    vms::CapacityProfile baseProfile_;   // static ceiling; optimizer adjusts from this
    std::vector<vms::TileRequest> requests_;
    // Spatial-canvas world position per tile id (inc 24). Defaults to the
    // prototype's grid placement; operator drags override; persisted (v9).
    std::vector<double> posX_, posY_;
    vms::GovernorResult plan_;
    QVariantList tiles_;
    int columns_ = 1;
    int rows_ = 1;
    int focusIndex_ = 0;
    QString capacity_;
    QString profileLabel_;
    QString optimizer_;
    bool overflow_ = false;
    int sweepIntervalMs_ = 0;   // remembered so the toolbar can resume the sweep
    QTimer timer_;
};
