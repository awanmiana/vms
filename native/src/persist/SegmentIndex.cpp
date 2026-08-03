#include "persist/SegmentIndex.h"

#include <algorithm>
#include <cctype>
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

bool parseCanonicalUtc(const std::string& value, std::int64_t& seconds) {
    seconds = 0;
    if (value.size() != 19 || value[4] != '-' || value[7] != '-' ||
        value[10] != ' ' || value[13] != ':' || value[16] != ':')
        return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) return false;
    }
    auto number = [&](std::size_t offset, std::size_t count) {
        int out = 0;
        for (std::size_t i = 0; i < count; ++i)
            out = out * 10 + (value[offset + i] - '0');
        return out;
    };
    int year = number(0, 4);
    const int month = number(5, 2);
    const int day = number(8, 2);
    const int hour = number(11, 2);
    const int minute = number(14, 2);
    const int second = number(17, 2);
    if (year < 1 || month < 1 || month > 12 || hour > 23 || minute > 59 ||
        second > 59)
        return false;
    static constexpr int monthDays[] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const int maxDay = monthDays[month - 1] + (month == 2 && leap ? 1 : 0);
    if (day < 1 || day > maxDay) return false;

    // Howard Hinnant's civil-date conversion: days relative to 1970-01-01.
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned shiftedMonth =
        static_cast<unsigned>(month + (month > 2 ? -3 : 9));
    const unsigned doy = (153 * shiftedMonth + 2) / 5 +
                         static_cast<unsigned>(day - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const std::int64_t days =
        static_cast<std::int64_t>(era) * 146097 + doe - 719468;
    seconds = days * 86400 + hour * 3600 + minute * 60 + second;
    return true;
}

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
    std::int64_t startSeconds = 0;
    std::int64_t endSeconds = 0;
    if (!parseCanonicalUtc(seg.startUtc, startSeconds) ||
        !parseCanonicalUtc(seg.endUtc, endSeconds))
        return {Status::Misuse,
                "segment timestamps must be canonical YYYY-MM-DD HH:MM:SS UTC"};
    if (endSeconds <= startSeconds)
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

Error SegmentIndex::recordedDuration(const std::vector<std::string>& cameraIds,
                                     RecordingDuration& out) {
    out = {};
    std::set<std::string> unique;
    for (const std::string& id : cameraIds)
        if (!id.empty()) unique.insert(id);
    if (unique.empty()) return Error::success();

    // Chunk the IN query below SQLite's common bind-parameter limits. A camera
    // appears in exactly one chunk, so interval union remains camera-local.
    constexpr std::size_t kChunkSize = 256;
    std::vector<std::string> ids(unique.begin(), unique.end());
    for (std::size_t offset = 0; offset < ids.size(); offset += kChunkSize) {
        const std::size_t count = std::min(kChunkSize, ids.size() - offset);
        std::string sql =
            "SELECT camera_id,start_utc,end_utc FROM segments WHERE camera_id IN (";
        std::vector<Value> params;
        params.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (i) sql += ',';
            sql += '?';
            params.emplace_back(ids[offset + i]);
        }
        sql += ") ORDER BY camera_id,start_utc,end_utc,id;";

        Result result;
        if (Error e = store_.query(sql, params, result); !e) return e;
        std::string activeCamera;
        std::int64_t mergedStart = 0;
        std::int64_t mergedEnd = 0;
        bool hasMerged = false;
        auto finishMerged = [&]() {
            if (!hasMerged) return;
            out.seconds += mergedEnd - mergedStart;
            hasMerged = false;
        };

        for (const Row& row : result.rows) {
            const std::string camera = std::get<std::string>(row[0]);
            const std::string startUtc = std::get<std::string>(row[1]);
            const std::string endUtc = std::get<std::string>(row[2]);
            std::int64_t start = 0;
            std::int64_t end = 0;
            if (!parseCanonicalUtc(startUtc, start) ||
                !parseCanonicalUtc(endUtc, end) || end <= start) {
                ++out.invalidSegments;
                continue;
            }
            ++out.segmentCount;
            out.rawSeconds += end - start;
            if (camera != activeCamera) {
                finishMerged();
                activeCamera = camera;
                ++out.camerasWithFootage;
                mergedStart = start;
                mergedEnd = end;
                hasMerged = true;
                continue;
            }
            if (start <= mergedEnd) {
                if (start < mergedEnd) ++out.overlappingSegments;
                mergedEnd = std::max(mergedEnd, end);
            } else {
                finishMerged();
                mergedStart = start;
                mergedEnd = end;
                hasMerged = true;
            }
        }
        finishMerged();
    }
    out.overlapRemovedSeconds = out.rawSeconds - out.seconds;
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
