#pragma once

// P3-05 — Instant playback from live view (native increment 23). Scope:
// ../instant-playback-P3-05-proposal.md.
//
// The "instant replay" verb: from the Live wall, jump back N seconds on the
// selected camera and watch, then return to live. It does NOT re-implement the
// timeline/transport/decode — it OWNS a PlaybackController (the honest
// availability + playhead + transport model, inc 7c-1) pinned to a look-back
// window, and the app feeds that controller's intent to a PlaybackPipeline
// exactly as the Playback tab does. Composition (not extension) keeps the Live
// overlay's state independent of the Playback tab's timeline (P3-14).
//
// Honest per P0-03: a camera with no local recording opens the overlay in an
// UNAVAILABLE state (never presenting requested time as recorded), and a
// look-back longer than the available footage clamps to the earliest recorded
// moment and reports the ACTUAL look-back. Read-only over the local recording
// index (P0-01E). Pure Qt + the pure SegmentIndex, so it verifies headlessly
// (--instant-selftest) against real recorded segments.

#include <QObject>
#include <QString>

#include "PlaybackController.h"
#include "persist/SegmentIndex.h"

class InstantReplayController : public QObject {
    Q_OBJECT
    // Whether the replay overlay is open (an operator invoked replay()).
    Q_PROPERTY(bool active READ active NOTIFY changed)
    // Whether the camera has local footage to replay. False => the overlay is
    // open but honestly shows "no local recording" instead of fake video.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString cameraId READ cameraId NOTIFY changed)
    // The ACTUAL look-back applied, in seconds (clamped to available footage).
    Q_PROPERTY(int lookbackSec READ lookbackSec NOTIFY changed)
    // A one-line honest status the overlay header shows.
    Q_PROPERTY(QString status READ status NOTIFY changed)
    // The reused timeline/transport/availability model the overlay binds to —
    // the same surface the Playback tab uses (spans, playhead, play/speed).
    Q_PROPERTY(PlaybackController* pb READ pb CONSTANT)

public:
    InstantReplayController(vms::persist::SegmentIndex* index, QString cameraId,
                            QObject* parent = nullptr);

    bool active() const { return active_; }
    bool available() const { return available_; }
    QString cameraId() const { return cameraId_; }
    int lookbackSec() const { return lookbackSec_; }
    QString status() const { return status_; }
    PlaybackController* pb() const { return pb_; }

    // Jump back `seconds` from the live edge (the latest recorded moment for this
    // camera) and auto-play. If the camera has no local recording, activates in
    // an unavailable state; if less than `seconds` of footage exists, clamps the
    // window to the earliest recorded moment and reports the actual look-back.
    Q_INVOKABLE void replay(int seconds);

    // Close the replay overlay and return to the live wall (pauses the decode).
    Q_INVOKABLE void returnToLive();

    // The camera's earliest recorded start and live edge (latest end), in epoch
    // seconds. Returns false when the camera has no footage. Exposed for the
    // self-check.
    bool footageExtent(long long& earliestSec, long long& liveEdgeSec) const;

signals:
    void changed();

private:
    vms::persist::SegmentIndex* index_ = nullptr;
    QString cameraId_;
    PlaybackController* pb_ = nullptr;   // owned (this is its QObject parent)
    bool active_ = false;
    bool available_ = false;
    int lookbackSec_ = 0;
    QString status_;
};
