#include "persist/PremisesRepo.h"

#include <cctype>

namespace vms::persist {
namespace {

double asDouble(const Value& value, double fallback) {
    if (std::holds_alternative<double>(value)) return std::get<double>(value);
    if (std::holds_alternative<std::int64_t>(value))
        return static_cast<double>(std::get<std::int64_t>(value));
    return fallback;
}

Error misuse(const std::string& message) {
    return {Status::Misuse, message};
}

bool validDate(const std::string& value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') return false;
    for (std::size_t i = 0; i < value.size(); ++i)
        if (i != 4 && i != 7 && !std::isdigit(
                static_cast<unsigned char>(value[i]))) return false;
    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(5, 2));
    const int day = std::stoi(value.substr(8, 2));
    if (year < 1 || month < 1 || month > 12 || day < 1) return false;
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int maxDay = days[month - 1];
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month == 2 && leap) ++maxDay;
    return day <= maxDay;
}

bool validWindow(int startMinute, int endMinute) {
    return startMinute >= 0 && startMinute < 1440 &&
           endMinute > startMinute && endMinute <= 1440;
}

Error ensureSchedule(Store& store, const std::string& siteId) {
    if (siteId.empty()) return misuse("site id is required");
    return store.exec(
        "INSERT INTO premises_hours_schedule(site_id,updated_at) "
        "VALUES(?,datetime('now')) ON CONFLICT(site_id) DO UPDATE SET "
        "updated_at=excluded.updated_at;", {siteId});
}

Error rejectOverlap(Store& store, const std::string& sql,
                    const std::vector<Value>& params, int startMinute,
                    int endMinute) {
    Result r;
    if (Error e = store.query(sql, params, r); !e) return e;
    for (const Row& row : r.rows) {
        const int start = static_cast<int>(std::get<std::int64_t>(row[0]));
        const int end = static_cast<int>(std::get<std::int64_t>(row[1]));
        if (start == startMinute && end == endMinute) continue;
        if (startMinute < end && endMinute > start)
            return misuse("operating windows must not overlap");
    }
    return Error::success();
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

Error PremisesRepo::loadOperatingSchedule(const std::string& siteId,
                                          OperatingSchedule& out) {
    out = OperatingSchedule{};
    if (siteId.empty()) return misuse("site id is required");
    Result marker;
    if (Error e = store_.query(
            "SELECT 1 FROM premises_hours_schedule WHERE site_id=?;",
            {siteId}, marker); !e) return e;
    if (marker.rows.empty()) return Error::success();
    out.configured = true;

    Result weekly;
    if (Error e = store_.query(
            "SELECT weekday,start_minute,end_minute "
            "FROM premises_hours_weekly WHERE site_id=? "
            "ORDER BY weekday,start_minute,end_minute;", {siteId}, weekly); !e)
        return e;
    for (const Row& row : weekly.rows)
        out.weekly.push_back({
            static_cast<int>(std::get<std::int64_t>(row[0])),
            static_cast<int>(std::get<std::int64_t>(row[1])),
            static_cast<int>(std::get<std::int64_t>(row[2]))});

    Result exceptions;
    if (Error e = store_.query(
            "SELECT local_date,closed,label FROM premises_hours_exception "
            "WHERE site_id=? ORDER BY local_date;", {siteId}, exceptions); !e)
        return e;
    for (const Row& row : exceptions.rows) {
        OperatingException exception;
        exception.localDate = std::get<std::string>(row[0]);
        exception.closed = std::get<std::int64_t>(row[1]) != 0;
        exception.label = std::get<std::string>(row[2]);
        Result windows;
        if (Error e = store_.query(
                "SELECT start_minute,end_minute "
                "FROM premises_hours_exception_window "
                "WHERE site_id=? AND local_date=? ORDER BY start_minute;",
                {siteId, exception.localDate}, windows); !e) return e;
        for (const Row& w : windows.rows)
            exception.windows.push_back({
                0, static_cast<int>(std::get<std::int64_t>(w[0])),
                static_cast<int>(std::get<std::int64_t>(w[1]))});
        out.exceptions.push_back(std::move(exception));
    }
    return Error::success();
}

Error PremisesRepo::addWeeklyWindow(const std::string& siteId, int weekday,
                                    int startMinute, int endMinute) {
    if (weekday < 1 || weekday > 7) return misuse("weekday must be 1..7");
    if (!validWindow(startMinute, endMinute))
        return misuse("window must satisfy 00:00 <= start < end <= 24:00");
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = ensureSchedule(store_, siteId); !e) return e;
    if (Error e = rejectOverlap(
            store_, "SELECT start_minute,end_minute FROM premises_hours_weekly "
                    "WHERE site_id=? AND weekday=?;",
            {siteId, static_cast<std::int64_t>(weekday)}, startMinute,
            endMinute); !e) return e;
    if (Error e = store_.exec(
            "INSERT OR IGNORE INTO premises_hours_weekly"
            "(site_id,weekday,start_minute,end_minute) VALUES(?,?,?,?);",
            {siteId, static_cast<std::int64_t>(weekday),
             static_cast<std::int64_t>(startMinute),
             static_cast<std::int64_t>(endMinute)}); !e) return e;
    return tx.commit();
}

Error PremisesRepo::clearWeeklyDay(const std::string& siteId, int weekday) {
    if (weekday < 1 || weekday > 7) return misuse("weekday must be 1..7");
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = ensureSchedule(store_, siteId); !e) return e;
    if (Error e = store_.exec(
            "DELETE FROM premises_hours_weekly WHERE site_id=? AND weekday=?;",
            {siteId, static_cast<std::int64_t>(weekday)}); !e) return e;
    return tx.commit();
}

Error PremisesRepo::setDateException(const std::string& siteId,
                                     const std::string& localDate,
                                     bool closed, int startMinute,
                                     int endMinute,
                                     const std::string& label) {
    if (!validDate(localDate)) return misuse("date must be a valid YYYY-MM-DD");
    if (!closed && !validWindow(startMinute, endMinute))
        return misuse("special-hours window must satisfy start < end");
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = ensureSchedule(store_, siteId); !e) return e;
    if (Error e = store_.exec(
            "INSERT INTO premises_hours_exception(site_id,local_date,closed,label) "
            "VALUES(?,?,?,?) ON CONFLICT(site_id,local_date) DO UPDATE SET "
            "closed=excluded.closed,label=excluded.label;",
            {siteId, localDate, static_cast<std::int64_t>(closed ? 1 : 0),
             label}); !e) return e;
    if (Error e = store_.exec(
            "DELETE FROM premises_hours_exception_window "
            "WHERE site_id=? AND local_date=?;", {siteId, localDate}); !e)
        return e;
    if (!closed) {
        if (Error e = store_.exec(
                "INSERT INTO premises_hours_exception_window"
                "(site_id,local_date,start_minute,end_minute) VALUES(?,?,?,?);",
                {siteId, localDate, static_cast<std::int64_t>(startMinute),
                 static_cast<std::int64_t>(endMinute)}); !e) return e;
    }
    return tx.commit();
}

Error PremisesRepo::clearDateException(const std::string& siteId,
                                       const std::string& localDate) {
    if (!validDate(localDate)) return misuse("date must be a valid YYYY-MM-DD");
    return store_.exec(
        "DELETE FROM premises_hours_exception WHERE site_id=? AND local_date=?;",
        {siteId, localDate});
}

} // namespace vms::persist
