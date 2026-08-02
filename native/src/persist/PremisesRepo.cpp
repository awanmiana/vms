#include "persist/PremisesRepo.h"

namespace vms::persist {
namespace {

double asDouble(const Value& value, double fallback) {
    if (std::holds_alternative<double>(value)) return std::get<double>(value);
    if (std::holds_alternative<std::int64_t>(value))
        return static_cast<double>(std::get<std::int64_t>(value));
    return fallback;
}

} // namespace

Error PremisesRepo::ensureDefault(const std::string& workspace) {
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = store_.exec(
            "INSERT OR IGNORE INTO premises_site(id,name,timezone) "
            "VALUES('site-default','Default site','UTC');"); !e)
        return e;
    if (Error e = store_.exec(
            "INSERT OR IGNORE INTO premises_floor(id,site_id,name,plan_uri,"
            "world_width,world_height) VALUES('floor-main','site-default',"
            "'Main floor','',1600,900);"); !e)
        return e;
    if (Error e = store_.exec(
            "INSERT OR IGNORE INTO premises_active(workspace,site_id,floor_id) "
            "VALUES(?,'site-default','floor-main');", {workspace}); !e)
        return e;
    return tx.commit();
}

Error PremisesRepo::loadActive(const std::string& workspace,
                               PremisesState& out, bool& found) {
    out = PremisesState{};
    found = false;
    Result r;
    if (Error e = store_.query(
            "SELECT s.id,s.name,s.timezone,COALESCE(f.id,''),"
            "COALESCE(f.name,''),COALESCE(f.plan_uri,''),"
            "COALESCE(f.world_width,1600),COALESCE(f.world_height,900) "
            "FROM premises_active a JOIN premises_site s ON s.id=a.site_id "
            "LEFT JOIN premises_floor f ON f.id=a.floor_id AND f.site_id=s.id "
            "WHERE a.workspace=?;", {workspace}, r); !e)
        return e;
    if (r.rows.empty()) return Error::success();
    const Row& row = r.rows.front();
    found = true;
    out.siteId = std::get<std::string>(row[0]);
    out.siteName = std::get<std::string>(row[1]);
    out.timezone = std::get<std::string>(row[2]);
    out.floorId = std::get<std::string>(row[3]);
    out.floorName = std::get<std::string>(row[4]);
    out.planUri = std::get<std::string>(row[5]);
    out.worldWidth = asDouble(row[6], 1600.0);
    out.worldHeight = asDouble(row[7], 900.0);
    return Error::success();
}

Error PremisesRepo::configureSite(const std::string& workspace,
                                  const std::string& id,
                                  const std::string& name,
                                  const std::string& timezone) {
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = store_.exec(
            "INSERT INTO premises_site(id,name,timezone,updated_at) "
            "VALUES(?,?,?,datetime('now')) ON CONFLICT(id) DO UPDATE SET "
            "name=excluded.name,timezone=excluded.timezone,"
            "updated_at=excluded.updated_at;", {id, name, timezone}); !e)
        return e;
    // A rename/update of the active site preserves its selected floor; an
    // actual site switch clears the prior site's floor intentionally.
    if (Error e = store_.exec(
            "INSERT INTO premises_active(workspace,site_id,floor_id) VALUES(?,?,NULL) "
            "ON CONFLICT(workspace) DO UPDATE SET site_id=excluded.site_id,"
            "floor_id=CASE WHEN premises_active.site_id=excluded.site_id "
            "THEN premises_active.floor_id ELSE NULL END;", {workspace, id}); !e)
        return e;
    return tx.commit();
}

Error PremisesRepo::configureFloor(const std::string& workspace,
                                   const std::string& id,
                                   const std::string& siteId,
                                   const std::string& name,
                                   const std::string& planUri,
                                   double worldWidth, double worldHeight) {
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = store_.exec(
            "INSERT INTO premises_floor(id,site_id,name,plan_uri,world_width,"
            "world_height,updated_at) VALUES(?,?,?,?,?,?,datetime('now')) "
            "ON CONFLICT(id) DO UPDATE SET site_id=excluded.site_id,"
            "name=excluded.name,plan_uri=excluded.plan_uri,"
            "world_width=excluded.world_width,world_height=excluded.world_height,"
            "updated_at=excluded.updated_at;",
            {id, siteId, name, planUri, worldWidth, worldHeight}); !e)
        return e;
    if (Error e = store_.exec(
            "INSERT INTO premises_active(workspace,site_id,floor_id) VALUES(?,?,?) "
            "ON CONFLICT(workspace) DO UPDATE SET site_id=excluded.site_id,"
            "floor_id=excluded.floor_id;", {workspace, siteId, id}); !e)
        return e;
    return tx.commit();
}

} // namespace vms::persist
