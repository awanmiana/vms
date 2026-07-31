#include "InstantReplayController.h"

#include <climits>
#include <ctime>
#include <vector>

namespace {

// Epoch seconds -> UTC 'YYYY-MM-DD HH:MM:SS' (the format SegmentIndex/
// PlaybackController use). The inverse of ParseUtcSeconds (PlaybackController.h).
QString FormatUtcSeconds(long long epochSec) {
    std::time_t t = static_cast<std::time_t>(epochSec);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    return QString::fromLatin1(buf);
}

}  // namespace

InstantReplayController::InstantReplayController(vms::persist::SegmentIndex* index,
                                                 QString cameraId, QObject* parent)
    : QObject(parent), index_(index), cameraId_(std::move(cameraId)) {
    // The instant overlay owns its own timeline/transport model, independent of
    // the Playback tab's (P3-14: the two instances never share state).
    pb_ = new PlaybackController(index_, cameraId_, this);
}

bool InstantReplayController::footageExtent(long long& earliestSec,
                                            long long& liveEdgeSec) const {
    if (!index_) return false;
    std::vector<vms::persist::Segment> segs;
    // A window wide enough to cover any plausible recording. list() returns every
    // segment overlapping it, ordered by start; we take the min start / max end.
    index_->list(cameraId_.toStdString(), "1970-01-01 00:00:00",
                 "2100-01-01 00:00:00", segs);
    if (segs.empty()) return false;
    long long minS = LLONG_MAX, maxE = LLONG_MIN;
    for (const vms::persist::Segment& s : segs) {
        const long long ss = ParseUtcSeconds(QString::fromStdString(s.startUtc));
        const long long se = ParseUtcSeconds(QString::fromStdString(s.endUtc));
        if (ss >= 0 && ss < minS) minS = ss;
        if (se >= 0 && se > maxE) maxE = se;
    }
    if (maxE <= minS) return false;
    earliestSec = minS;
    liveEdgeSec = maxE;
    return true;
}

void InstantReplayController::replay(int seconds) {
    if (seconds <= 0) seconds = 30;
    active_ = true;

    long long earliest = 0, edge = 0;
    if (!footageExtent(earliest, edge)) {
        // Honest: no local recording. Open the overlay, but show nothing as
        // recorded — clear the window so no availability spans exist.
        available_ = false;
        lookbackSec_ = seconds;
        status_ = QStringLiteral("No local recording for this camera");
        pb_->setRange(QString(), QString());
        pb_->pause();
        emit changed();
        return;
    }

    // Clamp the look-back to what actually exists and report the real duration.
    long long start = edge - seconds;
    if (start < earliest) start = earliest;
    const int actual = static_cast<int>(edge - start);

    available_ = true;
    lookbackSec_ = actual;
    status_ = QStringLiteral("Instant replay · last %1s").arg(actual);
    pb_->setRange(FormatUtcSeconds(start), FormatUtcSeconds(edge));
    pb_->seekFrac(0.0);   // open at the start of the look-back window
    pb_->setSpeed(1.0);
    pb_->play();
    emit changed();
}

void InstantReplayController::returnToLive() {
    if (!active_) return;
    active_ = false;
    status_.clear();
    pb_->pause();
    emit changed();
}
