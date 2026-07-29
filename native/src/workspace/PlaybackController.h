#pragma once

// P3-14 / P5-02 — Playback controller (native increment 7c). Scope:
// ../recording-playback-P4-03-P5-02-proposal.md.
//
// Turns the recording SegmentIndex (7a/7b) into the honest data model the
// Playback timeline binds to. For a selected camera and a requested [start,end]
// window it exposes:
//   - availability spans (Available / Missing gap / Overlapping) as fractions of
//     the window, so the timeline draws real footage and honest gaps and NEVER
//     paints requested time as recorded (P5-02);
//   - a playhead (absolute time + fraction) the operator drags to scrub;
//   - which recorded file (and offset) backs the current playhead, or none when
//     the playhead sits in a gap.
// Transport state (playing / speed) is held here; driving an actual media
// pipeline from it (playbin seek/speed/frame-step over the recorded file) is the
// windowed slice 7c-2. This controller is pure Qt + the pure SegmentIndex, so it
// is verified headlessly (--playback-selftest) against real recorded segments.

#include <QObject>
#include <QString>
#include <QVariantList>

#include <vector>

#include "persist/SegmentIndex.h"

class PlaybackController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString cameraId READ cameraId NOTIFY changed)
    Q_PROPERTY(QString rangeStart READ rangeStart NOTIFY changed)
    Q_PROPERTY(QString rangeEnd READ rangeEnd NOTIFY changed)
    // Each entry: { state, stateText, startFrac, endFrac, startUtc, endUtc, sources }.
    Q_PROPERTY(QVariantList spans READ spans NOTIFY changed)
    Q_PROPERTY(double playheadFrac READ playheadFrac NOTIFY playheadChanged)
    Q_PROPERTY(QString playheadUtc READ playheadUtc NOTIFY playheadChanged)
    // True when the playhead sits over recorded footage (an Available/Overlapping
    // span). The UI shows "no footage at this time" honestly when false.
    Q_PROPERTY(bool onFootage READ onFootage NOTIFY playheadChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY transportChanged)
    Q_PROPERTY(double speed READ speed NOTIFY transportChanged)

public:
    PlaybackController(vms::persist::SegmentIndex* index, QString cameraId,
                       QObject* parent = nullptr);

    QString cameraId() const { return cameraId_; }
    QString rangeStart() const { return rangeStart_; }
    QString rangeEnd() const { return rangeEnd_; }
    QVariantList spans() const { return spans_; }
    double playheadFrac() const;
    QString playheadUtc() const;
    bool onFootage() const;
    bool playing() const { return playing_; }
    double speed() const { return speed_; }

    // Set the requested playback window (UTC 'YYYY-MM-DD HH:MM:SS') and reload the
    // availability spans from the index. Clamps the playhead into the new window.
    Q_INVOKABLE void setRange(const QString& startUtc, const QString& endUtc);
    // Re-query the index (footage may have grown while recording).
    Q_INVOKABLE void refresh();
    // Move the playhead to a fraction [0,1] of the window (draggable scrub).
    Q_INVOKABLE void seekFrac(double frac);

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void setSpeed(double speed);
    // Frame-step by `frames` at `fps` (approximate: moves the playhead in time).
    Q_INVOKABLE void stepFrames(int frames, int fps = 25);

    // The recorded file backing the current playhead, and the seek offset (seconds)
    // into it. Returns { path, offsetSec } or an empty path when over a gap. This
    // is what slice 7c-2 hands to a playbin.
    Q_INVOKABLE QVariantMap segmentAtPlayhead() const;

    // Plain-text dump of the current spans (used by --playback-selftest).
    QString dumpSpans() const;

signals:
    void changed();
    void playheadChanged();
    void transportChanged();

private:
    void reload();
    long long startSec() const;
    long long endSec() const;

    vms::persist::SegmentIndex* index_ = nullptr;
    QString cameraId_;
    QString rangeStart_;
    QString rangeEnd_;
    QVariantList spans_;
    std::vector<vms::persist::Segment> segs_;   // raw segments in the window
    long long playheadSec_ = 0;                  // absolute epoch seconds
    bool playing_ = false;
    double speed_ = 1.0;
};

// Parse a UTC 'YYYY-MM-DD HH:MM:SS' timestamp to epoch seconds, or -1 on a
// malformed string. Exposed for the self-check.
long long ParseUtcSeconds(const QString& utc);
