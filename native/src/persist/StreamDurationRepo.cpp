#include "persist/StreamDurationRepo.h"

#include <limits>

namespace vms::persist {

Error StreamDurationRepo::addObservation(const std::string& siteId,
                                         std::int64_t elapsedMilliseconds,
                                         int playingBranches) {
    if (siteId.empty())
        return {Status::Misuse, "stream duration site id is empty", 0};
    if (elapsedMilliseconds < 0 || playingBranches < 0)
        return {Status::Misuse, "stream duration observation is negative", 0};
    if (elapsedMilliseconds == 0 || playingBranches == 0)
        return Error::success();

    const std::int64_t branches = playingBranches;
    const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    if (elapsedMilliseconds > maximum / branches)
        return {Status::Misuse, "stream duration observation overflows", 0};
    const std::int64_t delta = elapsedMilliseconds * branches;

    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;

    Result result;
    if (Error e = store_.query(
            "SELECT total_milliseconds,checkpoint_count "
            "FROM premises_stream_duration "
            "WHERE site_id=?;", {siteId}, result); !e)
        return e;
    const std::int64_t current = result.rows.empty()
        ? 0 : std::get<std::int64_t>(result.rows[0][0]);
    const std::int64_t checkpoints = result.rows.empty()
        ? 0 : std::get<std::int64_t>(result.rows[0][1]);
    if (current < 0 || current > maximum - delta ||
        checkpoints < 0 || checkpoints == maximum)
        return {Status::Misuse, "persisted stream duration would overflow", 0};

    if (Error e = store_.exec(
            "INSERT INTO premises_stream_duration("
            "site_id,total_milliseconds,checkpoint_count,updated_at_utc) "
            "VALUES(?,?,1,datetime('now')) "
            "ON CONFLICT(site_id) DO UPDATE SET "
            "total_milliseconds=excluded.total_milliseconds, "
            "checkpoint_count=premises_stream_duration.checkpoint_count+1, "
            "updated_at_utc=datetime('now');",
            {siteId, current + delta}); !e)
        return e;
    return tx.commit();
}

Error StreamDurationRepo::summary(const std::string& siteId,
                                  StreamDurationSummary& out) {
    out = {};
    if (siteId.empty())
        return {Status::Misuse, "stream duration site id is empty", 0};
    Result result;
    if (Error e = store_.query(
            "SELECT total_milliseconds,checkpoint_count,updated_at_utc "
            "FROM premises_stream_duration WHERE site_id=?;",
            {siteId}, result); !e)
        return e;
    if (result.rows.empty()) return Error::success();
    out.milliseconds = std::get<std::int64_t>(result.rows[0][0]);
    out.checkpoints = std::get<std::int64_t>(result.rows[0][1]);
    out.updatedAtUtc = std::get<std::string>(result.rows[0][2]);
    return Error::success();
}

}  // namespace vms::persist
