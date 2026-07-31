#include "persist/WorkspaceRepo.h"

namespace vms::persist {

Error WorkspaceRepo::save(const std::string& instance,
                          const InstanceState& state) {
    // Atomic replace: delete the old rows and write the new ones in one
    // transaction, so a crash mid-save leaves the previous state intact.
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;

    if (Error e = store_.exec("DELETE FROM workspace_tile WHERE instance=?;",
                              {instance}); !e)
        return e;
    if (Error e = store_.exec(
            "INSERT INTO workspace_instance(name, tile_count, updated_at) "
            "VALUES(?, ?, datetime('now')) "
            "ON CONFLICT(name) DO UPDATE SET tile_count=excluded.tile_count, "
            "updated_at=excluded.updated_at;",
            {instance, static_cast<std::int64_t>(state.tileCount)});
        !e)
        return e;

    for (const TilePref& t : state.tiles) {
        if (Error e = store_.exec(
                "INSERT INTO workspace_tile(instance, tile_id, desired_tier, "
                "priority, pos_x, pos_y) VALUES(?, ?, ?, ?, ?, ?);",
                {instance, static_cast<std::int64_t>(t.id),
                 static_cast<std::int64_t>(t.desiredTier),
                 static_cast<std::int64_t>(t.priority), t.posX, t.posY});
            !e)
            return e;
    }
    return tx.commit();
}

Error WorkspaceRepo::load(const std::string& instance, InstanceState& out,
                          bool& found) {
    out = InstanceState{};
    found = false;

    Result r;
    if (Error e = store_.query(
            "SELECT tile_count FROM workspace_instance WHERE name=?;",
            {instance}, r);
        !e)
        return e;
    if (r.rows.empty()) return Error::success();  // first run

    found = true;
    out.tileCount = static_cast<int>(std::get<std::int64_t>(r.rows[0][0]));

    Result rt;
    if (Error e = store_.query(
            "SELECT tile_id, desired_tier, priority, pos_x, pos_y "
            "FROM workspace_tile WHERE instance=? ORDER BY tile_id;",
            {instance}, rt);
        !e)
        return e;
    // SQLite may hand a REAL column back as an integer when the stored value is
    // integral (e.g. the -1 default), so accept either representation.
    auto asDouble = [](const Value& v) {
        if (std::holds_alternative<double>(v)) return std::get<double>(v);
        if (std::holds_alternative<std::int64_t>(v))
            return static_cast<double>(std::get<std::int64_t>(v));
        return -1.0;
    };
    out.tiles.reserve(rt.rows.size());
    for (const Row& row : rt.rows) {
        TilePref t;
        t.id = static_cast<int>(std::get<std::int64_t>(row[0]));
        t.desiredTier = static_cast<int>(std::get<std::int64_t>(row[1]));
        t.priority = static_cast<int>(std::get<std::int64_t>(row[2]));
        t.posX = asDouble(row[3]);
        t.posY = asDouble(row[4]);
        out.tiles.push_back(t);
    }
    return Error::success();
}

} // namespace vms::persist
