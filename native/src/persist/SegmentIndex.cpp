#include "persist/SegmentIndex.h"

#include <algorithm>
#include <set>

namespace vms::persist {

const char* SpanStateName(SpanState s) {
    switch (s) {
        case SpanState::Missing:     return "missing";
        case SpanState::Available:   return "available";
        case SpanState::Overlapping: return "overlapping";
        case SpanState::Unknown:     return "unknown";
    }
    return "?";
}

std::vector<AvailabilitySpan> ComputeAvailability(const std::vector<Segment>& segs,
                                                  const std::string& reqStart,
                                                  const std::string& reqEnd) {
    std::vector<AvailabilitySpan> spans;
    if (reqStart.empty() || reqEnd.empty() || reqEnd <= reqStart) return spans;

    // Boundary points: the request edges plus every segment endpoint clipped into
    // the request. Between two adjacent boundaries the coverage count is constant.
    std::set<std::string> points{reqStart, reqEnd};
    for (const Segment& s : segs) {
        const std::string cs = std::max(s.startUtc, reqStart);
        const std::string ce = std::min(s.endUtc, reqEnd);
        if (cs < ce) { points.insert(cs); points.insert(ce); }
    }

    std::vector<std::string> pts(points.begin(), points.end());
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const std::string& a = pts[i];
        const std::string& b = pts[i + 1];
        int coverage = 0;
        for (const Segment& s : segs)
            if (s.startUtc <= a && s.endUtc >= b) ++coverage;

        SpanState state = coverage == 0 ? SpanState::Missing
                        : coverage == 1 ? SpanState::Available
                                        : SpanState::Overlapping;

        // Merge into the previous span when the state matches (keep the higher
        // source count for an overlapping run).
        if (!spans.empty() && spans.back().state == state) {
            spans.back().endUtc = b;
            spans.back().sources = std::max(spans.back().sources, coverage);
        } else {
            spans.push_back({a, b, state, coverage});
        }
    }
    return spans;
}

namespace {

// Column order shared by every SELECT below.
const char* kCols = "id, camera_id, start_utc, end_utc, path, codec, bytes";

Segment rowToSegment(const Row& r) {
    Segment s;
    s.id = std::get<std::int64_t>(r[0]);
    s.cameraId = std::get<std::string>(r[1]);
    s.startUtc = std::get<std::string>(r[2]);
    s.endUtc = std::get<std::string>(r[3]);
    s.path = std::get<std::string>(r[4]);
    if (std::holds_alternative<std::string>(r[5])) s.codec = std::get<std::string>(r[5]);
    s.bytes = std::get<std::int64_t>(r[6]);
    return s;
}

}  // namespace

Error SegmentIndex::add(const Segment& seg, std::int64_t& outId) {
    outId = 0;
    if (seg.cameraId.empty() || seg.path.empty())
        return {Status::Misuse, "segment requires camera_id and path"};
    if (seg.startUtc.empty() || seg.endUtc.empty() || seg.endUtc <= seg.startUtc)
        return {Status::Misuse, "segment end_utc must be after start_utc"};
    if (seg.bytes < 0)
        return {Status::Misuse, "segment bytes must be >= 0"};

    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    const Value codec = seg.codec.empty() ? Value{nullptr} : Value{seg.codec};
    if (Error e = store_.exec(
            "INSERT INTO segments(camera_id, start_utc, end_utc, path, codec, bytes) "
            "VALUES(?, ?, ?, ?, ?, ?);",
            {seg.cameraId, seg.startUtc, seg.endUtc, seg.path, codec, seg.bytes});
        !e)
        return e;
    Result r;
    if (Error e = store_.query("SELECT last_insert_rowid();", {}, r); !e) return e;
    outId = std::get<std::int64_t>(r.rows[0][0]);
    return tx.commit();
}

Error SegmentIndex::list(const std::string& cameraId, const std::string& startUtc,
                         const std::string& endUtc, std::vector<Segment>& out) {
    out.clear();
    Result r;
    // Overlap: a segment intersects [startUtc, endUtc) iff it starts before the
    // range ends and ends after the range starts.
    if (Error e = store_.query(
            std::string("SELECT ") + kCols +
                " FROM segments WHERE camera_id=? AND start_utc<? AND end_utc>? "
                "ORDER BY start_utc;",
            {cameraId, endUtc, startUtc}, r);
        !e)
        return e;
    out.reserve(r.rows.size());
    for (const Row& row : r.rows) out.push_back(rowToSegment(row));
    return Error::success();
}

Error SegmentIndex::totalBytes(std::int64_t& out, const std::string& cameraId) {
    out = 0;
    Result r;
    Error e = cameraId.empty()
        ? store_.query("SELECT COALESCE(SUM(bytes),0) FROM segments;", {}, r)
        : store_.query("SELECT COALESCE(SUM(bytes),0) FROM segments WHERE camera_id=?;",
                       {cameraId}, r);
    if (!e) return e;
    out = std::get<std::int64_t>(r.rows[0][0]);
    return Error::success();
}

Error SegmentIndex::availability(const std::string& cameraId, const std::string& startUtc,
                                 const std::string& endUtc,
                                 std::vector<AvailabilitySpan>& out) {
    out.clear();
    std::vector<Segment> segs;
    if (Error e = list(cameraId, startUtc, endUtc, segs); !e) return e;
    out = ComputeAvailability(segs, startUtc, endUtc);
    return Error::success();
}

Error SegmentIndex::enforceCap(const RetentionCap& cap, const std::string& nowUtc,
                               std::vector<Segment>& deleted) {
    deleted.clear();

    // Age pass: drop everything that ended before (nowUtc - maxAgeDays).
    if (cap.maxAgeDays > 0) {
        const std::string modifier = "-" + std::to_string(cap.maxAgeDays) + " days";
        Result r;
        if (Error e = store_.query(
                std::string("SELECT ") + kCols +
                    " FROM segments WHERE end_utc < datetime(?, ?) ORDER BY start_utc;",
                {nowUtc, modifier}, r);
            !e)
            return e;
        if (!r.rows.empty()) {
            Store::Tx tx(store_);
            if (Error e = tx.begin(); !e) return e;
            if (Error e = store_.exec(
                    "DELETE FROM segments WHERE end_utc < datetime(?, ?);",
                    {nowUtc, modifier});
                !e)
                return e;
            if (Error e = tx.commit(); !e) return e;
            for (const Row& row : r.rows) deleted.push_back(rowToSegment(row));
        }
    }

    // Size pass: delete oldest-first until total bytes fit under the cap.
    if (cap.maxTotalBytes > 0) {
        std::int64_t total = 0;
        if (Error e = totalBytes(total); !e) return e;
        while (total > cap.maxTotalBytes) {
            Result r;
            if (Error e = store_.query(
                    std::string("SELECT ") + kCols +
                        " FROM segments ORDER BY start_utc, id LIMIT 1;",
                    {}, r);
                !e)
                return e;
            if (r.rows.empty()) break;
            const Segment oldest = rowToSegment(r.rows[0]);
            Store::Tx tx(store_);
            if (Error e = tx.begin(); !e) return e;
            if (Error e = store_.exec("DELETE FROM segments WHERE id=?;", {oldest.id}); !e)
                return e;
            if (Error e = tx.commit(); !e) return e;
            deleted.push_back(oldest);
            total -= oldest.bytes;
        }
    }
    return Error::success();
}

} // namespace vms::persist
