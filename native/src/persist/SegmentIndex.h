#pragma once

// Recording segment index — native increment 7a. Scope:
// ../recording-playback-P4-03-P5-02-proposal.md.
//
// The data authority for OPTIONAL local recording (P0-01E): one row per completed
// recorded segment (the video lives in a file; this indexes what footage exists
// and where). It never claims footage that was not written, and a size/age cap
// trims oldest-first (a crude stand-in for the real retention policy, P4-06). It
// returns the trimmed segments so the caller can unlink the files. Pure and
// SQLite-backed behind the C0-03 Store contract, so it is unit-testable in
// isolation (vms_rectest) exactly like WorkspaceRepo / CredentialRepo.
//
// All times are UTC in SQLite's 'YYYY-MM-DD HH:MM:SS' format, so lexicographic
// order is chronological and datetime() age math works.

#include <cstdint>
#include <string>
#include <vector>

#include "persist/Store.h"

namespace vms::persist {

struct Segment {
    std::int64_t id = 0;
    std::string cameraId;
    std::string startUtc;   // 'YYYY-MM-DD HH:MM:SS' UTC
    std::string endUtc;
    std::string path;
    std::string codec;      // may be empty
    std::int64_t bytes = 0;
};

// A crude retention cap standing in for P4-06. 0 = unlimited on that dimension.
struct RetentionCap {
    std::int64_t maxTotalBytes = 0;
    int maxAgeDays = 0;
};

// Honest footage-availability state for a span of a requested range (P5-02): the
// requested time is NEVER presented as recorded — a span is only Available where
// a segment actually backs it. Unknown is reserved for a source that could not be
// queried (a remote recorder); the local index yields only the first three.
enum class SpanState { Missing, Available, Overlapping, Unknown };
const char* SpanStateName(SpanState s);

// One contiguous span of the requested range with a single honest state.
struct AvailabilitySpan {
    std::string startUtc;
    std::string endUtc;
    SpanState state = SpanState::Missing;
    int sources = 0;   // segments covering this span (0=gap, 1=available, >=2=overlap)
};

// Site/premises callers need a cumulative recording-time value rather than a
// playback range. Duration is the union of completed segment intervals for each
// requested camera, summed across cameras (camera-hours). Same-camera overlap is
// removed so duplicate fragments cannot inflate the result. Malformed legacy
// rows are excluded and reported rather than converted into invented time.
struct RecordingDuration {
    std::int64_t seconds = 0;
    std::int64_t rawSeconds = 0;
    std::int64_t overlapRemovedSeconds = 0;
    int segmentCount = 0;              // valid completed segments
    int camerasWithFootage = 0;
    int overlappingSegments = 0;
    int invalidSegments = 0;
};

// Pure: given the segments overlapping [reqStart, reqEnd) (as SegmentIndex::list
// returns them), compute the ordered availability spans that tile the requested
// range end-to-end — every instant of the request is labelled Available, Missing,
// or Overlapping, so gaps and duplicate footage are shown, never hidden. No DB
// access, so it is trivially unit-testable.
std::vector<AvailabilitySpan> ComputeAvailability(const std::vector<Segment>& segs,
                                                  const std::string& reqStart,
                                                  const std::string& reqEnd);

class SegmentIndex {
public:
    explicit SegmentIndex(Store& store) : store_(store) {}

    // Record one COMPLETED segment atomically. Rejects (Status::Misuse, no row) a
    // segment with no camera_id/path or with end_utc <= start_utc or negative
    // bytes — so the index never advertises footage that was not written.
    Error add(const Segment& seg, std::int64_t& outId);

    // Segments for a camera that OVERLAP [startUtc, endUtc), ordered by start.
    Error list(const std::string& cameraId, const std::string& startUtc,
               const std::string& endUtc, std::vector<Segment>& out);

    // Total indexed bytes (all cameras, or one camera when cameraId is set).
    Error totalBytes(std::int64_t& out, const std::string& cameraId = "");

    // Query the index and compute honest availability spans over [startUtc,
    // endUtc) for one camera (list() + ComputeAvailability()).
    Error availability(const std::string& cameraId, const std::string& startUtc,
                       const std::string& endUtc, std::vector<AvailabilitySpan>& out);

    // Aggregate all completed footage for the requested camera ids. Duplicate
    // ids are ignored. An empty set or cameras with no rows returns a successful
    // zero summary: the source was queried and no completed footage exists.
    Error recordedDuration(const std::vector<std::string>& cameraIds,
                           RecordingDuration& out);

    // Enforce the cap: first delete segments older than maxAgeDays (relative to
    // nowUtc), then delete oldest-first until total bytes <= maxTotalBytes. The
    // deleted rows are returned in `deleted` (oldest-first) so the caller can
    // unlink the files. Each deletion is transactional; a zero cap is a no-op.
    Error enforceCap(const RetentionCap& cap, const std::string& nowUtc,
                     std::vector<Segment>& deleted);

private:
    Store& store_;
};

} // namespace vms::persist
