// Recording segment-index self-check — native increment 7a. No Qt, no GStreamer,
// no camera. Exercises the SegmentIndex against an in-memory SQLite store: add
// with validation (never index footage that was not written), overlap listing,
// total-bytes, and the oldest-first size/age retention cap (returning trimmed
// segments for file cleanup). Registered with CTest as `recording_selfcheck`.
// Scope: ../recording-playback-P4-03-P5-02-proposal.md.

#include "persist/SegmentIndex.h"
#include "persist/Schema.h"
#include "persist/Store.h"

#include <iostream>
#include <string>
#include <vector>

using namespace vms::persist;

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}

Segment seg(const std::string& cam, const std::string& start, const std::string& end,
            const std::string& path, std::int64_t bytes) {
    Segment s;
    s.cameraId = cam;
    s.startUtc = start;
    s.endUtc = end;
    s.path = path;
    s.codec = "h265";
    s.bytes = bytes;
    return s;
}

}  // namespace

int main() {
    std::cout << "vms_rectest — recording segment-index self-check\n";

    Store store;
    check(static_cast<bool>(store.open(":memory:")), "open in-memory store");
    check(static_cast<bool>(store.migrate(coreMigrations())), "migrate schema (v4 segments)");
    check(store.schemaVersion() >= 4, "schema is at least v4");

    SegmentIndex idx(store);

    // ---- add + validation ----
    std::int64_t idA = 0, idB = 0, idC = 0, idD = 0;
    check(static_cast<bool>(idx.add(
              seg("cam-1", "2026-07-29 09:00:00", "2026-07-29 09:10:00", "seg_a.mp4", 100), idA)) &&
              idA > 0,
          "add segment A");
    check(static_cast<bool>(idx.add(
              seg("cam-1", "2026-07-29 09:10:00", "2026-07-29 09:20:00", "seg_b.mp4", 100), idB)),
          "add segment B");
    check(static_cast<bool>(idx.add(
              seg("cam-1", "2026-07-29 09:20:00", "2026-07-29 09:30:00", "seg_c.mp4", 100), idC)),
          "add segment C");
    check(static_cast<bool>(idx.add(
              seg("cam-1", "2026-07-29 09:30:00", "2026-07-29 09:40:00", "seg_d.mp4", 100), idD)),
          "add segment D");

    std::int64_t bad = 0;
    check(idx.add(seg("cam-1", "2026-07-29 10:00:00", "2026-07-29 10:00:00", "z.mp4", 100), bad)
              .status == Status::Misuse,
          "reject zero-length segment (end == start)");
    check(idx.add(seg("cam-1", "2026-07-29 10:10:00", "2026-07-29 10:00:00", "z.mp4", 100), bad)
              .status == Status::Misuse,
          "reject inverted segment (end < start)");
    check(idx.add(seg("cam-1", "2026-07-29 10:00:00", "2026-07-29 10:10:00", "", 100), bad)
              .status == Status::Misuse,
          "reject segment with empty path");
    check(idx.add(seg("", "2026-07-29 10:00:00", "2026-07-29 10:10:00", "z.mp4", 100), bad)
              .status == Status::Misuse,
          "reject segment with empty camera_id");
    check(idx.add(seg("cam-1", "2026-02-30 10:00:00",
                      "2026-02-30 10:10:00", "z.mp4", 100), bad)
              .status == Status::Misuse,
          "reject non-canonical calendar timestamps");

    std::int64_t total = 0;
    check(static_cast<bool>(idx.totalBytes(total)) && total == 400,
          "total bytes == 400 (only the 4 valid segments indexed)");

    // ---- overlap listing ----
    {
        std::vector<Segment> got;
        check(static_cast<bool>(idx.list("cam-1", "2026-07-29 09:12:00",
                                         "2026-07-29 09:33:00", got)),
              "list a mid-range window");
        // A ends 09:10 (before 09:12) -> excluded; B, C, D overlap.
        check(got.size() == 3 && got[0].path == "seg_b.mp4" && got[2].path == "seg_d.mp4",
              "overlap list returns B,C,D in start order (A excluded)");
    }
    {
        std::vector<Segment> none;
        check(static_cast<bool>(idx.list("cam-1", "2026-07-29 12:00:00",
                                         "2026-07-29 13:00:00", none)) &&
                  none.empty(),
              "list a window with no footage returns empty (honest gap)");
    }

    // ---- size cap: trim oldest-first until under the cap ----
    {
        std::vector<Segment> deleted;
        RetentionCap cap;
        cap.maxTotalBytes = 250;   // 400 -> must drop A (300) then B (200 <= 250)
        check(static_cast<bool>(idx.enforceCap(cap, "2026-07-29 09:45:00", deleted)),
              "enforceCap (size 250)");
        check(deleted.size() == 2 && deleted[0].path == "seg_a.mp4" &&
                  deleted[1].path == "seg_b.mp4",
              "size cap trimmed oldest-first (A then B) and returned them for cleanup");
        std::int64_t after = 0;
        idx.totalBytes(after);
        check(after == 200, "remaining bytes == 200 (C,D) after size trim");
        std::vector<Segment> remain;
        idx.list("cam-1", "2026-07-29 00:00:00", "2026-07-29 23:59:59", remain);
        check(remain.size() == 2 && remain[0].path == "seg_c.mp4",
              "index reflects the deletion (C,D remain)");
    }

    // ---- age cap: drop segments older than N days ----
    {
        std::int64_t idOld = 0;
        idx.add(seg("cam-1", "2026-06-01 09:00:00", "2026-06-01 09:10:00", "old.mp4", 100), idOld);
        std::vector<Segment> deleted;
        RetentionCap cap;
        cap.maxAgeDays = 7;   // relative to 2026-07-29, June 1 is far older
        check(static_cast<bool>(idx.enforceCap(cap, "2026-07-29 09:45:00", deleted)),
              "enforceCap (age 7 days)");
        check(deleted.size() == 1 && deleted[0].path == "old.mp4",
              "age cap trimmed only the old segment");
        std::vector<Segment> remain;
        idx.list("cam-1", "2026-07-01 00:00:00", "2026-07-31 23:59:59", remain);
        check(remain.size() == 2, "recent segments (C,D) survive the age cap");
    }

    // ---- a zero cap is a no-op ----
    {
        std::vector<Segment> deleted;
        RetentionCap none;
        idx.enforceCap(none, "2026-07-29 09:45:00", deleted);
        check(deleted.empty(), "an unlimited (zero) cap deletes nothing");
    }

    // ---- 7b: honest availability spans (available / missing / overlapping) ----
    std::int64_t tmp = 0;
    idx.add(seg("cam-2", "2026-07-29 09:00:00", "2026-07-29 09:10:00", "c2a.mp4", 10), tmp);
    idx.add(seg("cam-2", "2026-07-29 09:10:00", "2026-07-29 09:20:00", "c2b.mp4", 10), tmp);
    {
        std::vector<AvailabilitySpan> av;
        check(static_cast<bool>(idx.availability(
                  "cam-2", "2026-07-29 09:00:00", "2026-07-29 09:20:00", av)),
              "7b: availability over continuous footage");
        check(av.size() == 1 && av[0].state == SpanState::Available,
              "7b: contiguous segments merge into one Available span");
    }
    idx.add(seg("cam-2", "2026-07-29 09:30:00", "2026-07-29 09:40:00", "c2g.mp4", 10), tmp);
    {
        std::vector<AvailabilitySpan> av;
        idx.availability("cam-2", "2026-07-29 09:00:00", "2026-07-29 09:40:00", av);
        check(av.size() == 3 && av[0].state == SpanState::Available &&
                  av[1].state == SpanState::Missing && av[2].state == SpanState::Available,
              "7b: a gap between segments is an explicit Missing span");
    }
    {
        std::vector<AvailabilitySpan> av;
        idx.availability("cam-2", "2026-07-29 08:50:00", "2026-07-29 09:05:00", av);
        check(av.size() == 2 && av[0].state == SpanState::Missing &&
                  av[0].startUtc == "2026-07-29 08:50:00" && av[1].state == SpanState::Available,
              "7b: requested time before footage is Missing, not shown as recorded");
    }
    {
        std::vector<AvailabilitySpan> av;
        idx.availability("cam-2", "2026-07-29 12:00:00", "2026-07-29 13:00:00", av);
        check(av.size() == 1 && av[0].state == SpanState::Missing,
              "7b: a range with no footage is one honest Missing span");
    }
    // Overlapping (duplicate) footage on its own camera.
    idx.add(seg("cam-3", "2026-07-29 09:00:00", "2026-07-29 09:15:00", "c3a.mp4", 10), tmp);
    idx.add(seg("cam-3", "2026-07-29 09:10:00", "2026-07-29 09:20:00", "c3b.mp4", 10), tmp);
    {
        std::vector<AvailabilitySpan> av;
        idx.availability("cam-3", "2026-07-29 09:00:00", "2026-07-29 09:20:00", av);
        check(av.size() == 3 && av[1].state == SpanState::Overlapping && av[1].sources == 2,
              "7b: overlapping segments are flagged Overlapping (2 sources), not hidden");
    }

    // ---- P3-18 / inc 38: cumulative completed-recording duration ----
    idx.add(seg("duration-a", "2026-07-29 09:00:00",
                "2026-07-29 09:10:00", "da.mp4", 10), tmp);
    idx.add(seg("duration-a", "2026-07-29 09:05:00",
                "2026-07-29 09:15:00", "db.mp4", 10), tmp);
    idx.add(seg("duration-a", "2026-07-29 09:15:00",
                "2026-07-29 09:20:00", "dc.mp4", 10), tmp);
    idx.add(seg("duration-b", "2026-07-29 10:00:00",
                "2026-07-29 10:02:00", "dd.mp4", 10), tmp);
    {
        RecordingDuration duration;
        check(static_cast<bool>(idx.recordedDuration(
                  {"duration-a", "duration-b", "duration-a"}, duration)),
              "38: query duration for a deduplicated camera set");
        check(duration.seconds == 1320 && duration.rawSeconds == 1620 &&
                  duration.overlapRemovedSeconds == 300,
              "38: union removes same-camera overlap and sums camera-hours");
        check(duration.segmentCount == 4 &&
                  duration.camerasWithFootage == 2 &&
                  duration.overlappingSegments == 1 &&
                  duration.invalidSegments == 0,
              "38: duration evidence counts cameras, segments, and overlap");
    }
    {
        RecordingDuration duration;
        check(static_cast<bool>(idx.recordedDuration(
                  {"camera-with-no-footage"}, duration)) &&
                  duration.seconds == 0 && duration.segmentCount == 0,
              "38: connected index with no completed footage returns honest zero");
    }
    check(static_cast<bool>(store.exec(
              "INSERT INTO segments(camera_id,start_utc,end_utc,path,bytes) "
              "VALUES('duration-a','not-a-time','2026-07-29 11:00:00',"
              "'legacy-bad.mp4',10);")),
          "38: inject one malformed legacy row for exclusion coverage");
    {
        RecordingDuration duration;
        idx.recordedDuration({"duration-a"}, duration);
        check(duration.seconds == 1200 && duration.segmentCount == 3 &&
                  duration.invalidSegments == 1,
              "38: malformed legacy rows are excluded and reported");
    }

    if (failures == 0) {
        std::cout << "PASS: segment validation, availability, retention, and "
                     "overlap-safe cumulative duration all verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
