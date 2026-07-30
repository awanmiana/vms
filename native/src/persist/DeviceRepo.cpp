#include "persist/DeviceRepo.h"

#include <algorithm>

namespace vms::persist {

std::string DeviceRepo::defaultGroupId(const std::string& deviceId) {
    return "device:" + deviceId;   // one protected default group per device (P2-02)
}

Error DeviceRepo::cameraIds(const std::string& deviceId,
                            std::vector<std::string>& out) {
    out.clear();
    Result r;
    if (Error e = store_.query(
            "SELECT id FROM cameras WHERE device_id=? ORDER BY id;", {deviceId}, r);
        !e)
        return e;
    for (const Row& row : r.rows) out.push_back(std::get<std::string>(row[0]));
    return Error::success();
}

namespace {
// A TEXT column that may be NULL (address / vendor) as a std::string.
std::string textOrEmpty(const Value& v) {
    return std::holds_alternative<std::string>(v) ? std::get<std::string>(v)
                                                   : std::string();
}
}  // namespace

Error DeviceRepo::listDevices(std::vector<DeviceSummary>& out) {
    out.clear();
    Result r;
    if (Error e = store_.query(
            "SELECT d.id, d.name, d.address, d.vendor, d.kind, "
            "(SELECT COUNT(*) FROM cameras c WHERE c.device_id = d.id), d.disabled "
            "FROM devices d ORDER BY d.id;",
            {}, r);
        !e)
        return e;
    for (const Row& row : r.rows) {
        DeviceSummary s;
        s.id = std::get<std::string>(row[0]);
        s.name = std::get<std::string>(row[1]);
        s.address = textOrEmpty(row[2]);
        s.vendor = textOrEmpty(row[3]);
        s.kind = textOrEmpty(row[4]);
        s.cameraCount = static_cast<int>(std::get<std::int64_t>(row[5]));
        s.disabled = std::get<std::int64_t>(row[6]) != 0;
        out.push_back(std::move(s));
    }
    return Error::success();
}

Error DeviceRepo::reconcileGroup(const std::string& deviceId,
                                 const std::string& deviceName) {
    const std::string gid = defaultGroupId(deviceId);
    // The default group exists and is named after the device; membership is
    // rewritten to exactly the device's ENABLED cameras (P2-02 + P2-05 — a
    // disabled channel stays in inventory but leaves the active group).
    if (Error e = store_.exec(
            "INSERT INTO camera_groups(id, name) VALUES(?, ?) "
            "ON CONFLICT(id) DO UPDATE SET name=excluded.name;",
            {gid, deviceName + " (default)"});
        !e)
        return e;
    if (Error e = store_.exec("DELETE FROM camera_group_members WHERE group_id=?;",
                              {gid});
        !e)
        return e;
    // A detached device (P2-06) has an empty default group — it is excluded from
    // active use while keeping all its configuration.
    Result dd;
    if (Error e = store_.query("SELECT disabled FROM devices WHERE id=?;",
                               {deviceId}, dd);
        !e)
        return e;
    const bool deviceDetached =
        !dd.rows.empty() && std::get<std::int64_t>(dd.rows[0][0]) != 0;
    if (deviceDetached) return Error::success();
    Result r;
    if (Error e = store_.query(
            "SELECT id FROM cameras WHERE device_id=? AND disabled=0 ORDER BY id;",
            {deviceId}, r);
        !e)
        return e;
    for (const Row& row : r.rows) {
        if (Error e = store_.exec(
                "INSERT INTO camera_group_members(group_id, camera_id) VALUES(?, ?);",
                {gid, std::get<std::string>(row[0])});
            !e)
            return e;
    }
    return Error::success();
}

Error DeviceRepo::onboard(const DeviceOnboard& d) {
    if (d.id.empty() || d.name.empty())
        return {Status::Misuse, "device requires id and name"};

    const std::string ref = CredentialRepo::mintRef(d.id);

    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;

    if (Error e = store_.exec(
            "INSERT INTO devices(id, name, address, vendor, kind) "
            "VALUES(?, ?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET name=excluded.name, "
            "address=excluded.address, vendor=excluded.vendor, "
            "kind=excluded.kind, updated_at=datetime('now');",
            {d.id, d.name,
             d.address.empty() ? Value{nullptr} : Value{d.address},
             d.vendor.empty() ? Value{nullptr} : Value{d.vendor},
             d.kind.empty() ? std::string("camera") : d.kind});
        !e)
        return e;

    for (const CameraChannel& c : d.cameras) {
        if (c.id.empty()) return {Status::Misuse, "camera requires a stable id"};
        // Onboard sets the operator-facing name; URL templates are credential-free.
        // A re-onboard preserves the channel's disabled flag (only name/urls set).
        if (Error e = store_.exec(
                "INSERT INTO cameras(id, device_id, name, main_url, sub_url) "
                "VALUES(?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
                "name=excluded.name, main_url=excluded.main_url, "
                "sub_url=excluded.sub_url, updated_at=datetime('now');",
                {c.id, d.id, c.name,
                 c.mainUrl.empty() ? Value{nullptr} : Value{c.mainUrl},
                 c.subUrl.empty() ? Value{nullptr} : Value{c.subUrl}});
            !e)
            return e;
    }

    if (Error e = reconcileGroup(d.id, d.name); !e) return e;

    // The device_credentials row carries only the opaque ref; the secret is in
    // the SecretStore under that ref (the broker resolves it the same way).
    if (Error e = store_.exec(
            "INSERT INTO device_credentials(device_id, credential_ref) VALUES(?, ?) "
            "ON CONFLICT(device_id) DO UPDATE SET credential_ref=excluded.credential_ref;",
            {d.id, ref});
        !e)
        return e;

    if (Error e = secrets_.put(ref, d.credentialSecret); !e)
        return e;   // tx destructor rolls back every row above -> nothing onboarded

    if (Error e = tx.commit(); !e) {
        secrets_.remove(ref);   // undo the secret so a failed onboard leaves nothing
        return e;
    }
    return Error::success();
}

Error DeviceRepo::syncChannels(const std::string& deviceId,
                               const std::vector<CameraChannel>& cameras,
                               bool allowRemoval) {
    if (deviceId.empty()) return {Status::Misuse, "empty deviceId"};

    // The device's name (for the group) and its current camera ids.
    std::string deviceName = deviceId;
    {
        Result r;
        if (Error e = store_.query("SELECT name FROM devices WHERE id=?;",
                                   {deviceId}, r);
            !e)
            return e;
        if (r.rows.empty()) return {Status::NotFound, "unknown device " + deviceId};
        deviceName = std::get<std::string>(r.rows[0][0]);
    }
    std::vector<std::string> existing;
    if (Error e = cameraIds(deviceId, existing); !e) return e;

    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;

    std::vector<std::string> wanted;
    for (const CameraChannel& c : cameras) {
        if (c.id.empty()) return {Status::Misuse, "camera requires a stable id"};
        const bool isNew =
            std::find(existing.begin(), existing.end(), c.id) == existing.end();
        if (isNew) {
            if (Error e = store_.exec(
                    "INSERT INTO cameras(id, device_id, name, main_url, sub_url) "
                    "VALUES(?, ?, ?, ?, ?);",
                    {c.id, deviceId, c.name,
                     c.mainUrl.empty() ? Value{nullptr} : Value{c.mainUrl},
                     c.subUrl.empty() ? Value{nullptr} : Value{c.subUrl}});
                !e)
                return e;
        } else {
            // Existing channel: refresh only the technical URL templates; the
            // operator-customized name is preserved (P2-05).
            if (Error e = store_.exec(
                    "UPDATE cameras SET main_url=?, sub_url=?, "
                    "updated_at=datetime('now') WHERE id=?;",
                    {c.mainUrl.empty() ? Value{nullptr} : Value{c.mainUrl},
                     c.subUrl.empty() ? Value{nullptr} : Value{c.subUrl}, c.id});
                !e)
                return e;
        }
        wanted.push_back(c.id);
    }

    if (allowRemoval) {
        for (const std::string& old : existing) {
            if (std::find(wanted.begin(), wanted.end(), old) == wanted.end()) {
                if (Error e = store_.exec("DELETE FROM cameras WHERE id=?;", {old}); !e)
                    return e;   // cascade removes its group memberships
            }
        }
    }
    // reconcileGroup reads the current (post-write) enabled camera rows, so a
    // retained-but-not-listed channel (allowRemoval=false) stays in the group
    // and a disabled one is excluded — no in-memory id juggling needed.
    if (Error e = reconcileGroup(deviceId, deviceName); !e) return e;

    return tx.commit();
}

Error DeviceRepo::setDeviceDisabled(const std::string& deviceId, bool disabled) {
    if (deviceId.empty()) return {Status::Misuse, "empty deviceId"};
    Result r;
    if (Error e = store_.query("SELECT name FROM devices WHERE id=?;", {deviceId}, r);
        !e)
        return e;
    if (r.rows.empty()) return {Status::NotFound, "unknown device " + deviceId};
    const std::string deviceName = std::get<std::string>(r.rows[0][0]);

    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = store_.exec(
            "UPDATE devices SET disabled=?, updated_at=datetime('now') WHERE id=?;",
            {Value{static_cast<std::int64_t>(disabled ? 1 : 0)}, deviceId});
        !e)
        return e;
    // reconcileGroup empties the group when the device is detached and restores
    // it to the enabled channels on re-attach.
    if (Error e = reconcileGroup(deviceId, deviceName); !e) return e;
    return tx.commit();
}

Error DeviceRepo::renameDevice(const std::string& deviceId,
                               const std::string& newName) {
    if (deviceId.empty() || newName.empty())
        return {Status::Misuse, "device id and name required"};
    Result r;
    if (Error e = store_.query("SELECT 1 FROM devices WHERE id=?;", {deviceId}, r);
        !e)
        return e;
    if (r.rows.empty()) return {Status::NotFound, "unknown device " + deviceId};

    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = store_.exec(
            "UPDATE devices SET name=?, updated_at=datetime('now') WHERE id=?;",
            {newName, deviceId});
        !e)
        return e;
    // Refresh only the default group's display name; membership + camera rows
    // (ids, operator names, urls) and the credential are untouched.
    if (Error e = reconcileGroup(deviceId, newName); !e) return e;
    return tx.commit();
}

Error DeviceRepo::remove(const std::string& deviceId) {
    if (deviceId.empty()) return {Status::Misuse, "empty deviceId"};
    const std::string ref = CredentialRepo::mintRef(deviceId);
    {
        Store::Tx tx(store_);
        if (Error e = tx.begin(); !e) return e;
        // Cameras + device_credentials cascade from devices; the default group is
        // standalone, so drop it explicitly.
        if (Error e = store_.exec("DELETE FROM camera_groups WHERE id=?;",
                                  {defaultGroupId(deviceId)});
            !e)
            return e;
        if (Error e = store_.exec("DELETE FROM devices WHERE id=?;", {deviceId}); !e)
            return e;
        if (Error e = tx.commit(); !e) return e;
    }
    return secrets_.remove(ref);
}

namespace {
// A nullable INTEGER column as int (0 when NULL — e.g. a channel with no
// synced stream profile).
int intOrZero(const Value& v) {
    return std::holds_alternative<std::int64_t>(v)
               ? static_cast<int>(std::get<std::int64_t>(v))
               : 0;
}
}  // namespace

Error DeviceRepo::listChannels(const std::string& deviceId,
                               std::vector<ChannelSummary>& out) {
    out.clear();
    Result r;
    // The main stream profile (if any was synced from discovery) joins in for
    // display. ORDER BY the operator's sort_order, then id as a stable tiebreak.
    if (Error e = store_.query(
            "SELECT c.id, c.name, c.main_url, c.sub_url, c.disabled, "
            "sp.width, sp.height, sp.codec "
            "FROM cameras c "
            "LEFT JOIN stream_profiles sp "
            "  ON sp.camera_id = c.id AND sp.tier = 'main' "
            "WHERE c.device_id=? ORDER BY c.sort_order, c.id;",
            {deviceId}, r);
        !e)
        return e;
    for (const Row& row : r.rows) {
        ChannelSummary c;
        c.id = std::get<std::string>(row[0]);
        c.name = std::get<std::string>(row[1]);
        c.mainUrl = textOrEmpty(row[2]);
        c.subUrl = textOrEmpty(row[3]);
        c.disabled = std::get<std::int64_t>(row[4]) != 0;
        c.mainWidth = intOrZero(row[5]);
        c.mainHeight = intOrZero(row[6]);
        c.mainCodec = textOrEmpty(row[7]);
        out.push_back(std::move(c));
    }
    return Error::success();
}

Error DeviceRepo::renameChannel(const std::string& cameraId,
                                const std::string& name) {
    if (cameraId.empty() || name.empty())
        return {Status::Misuse, "channel id and name required"};
    // Confirm the channel exists so a bad id is an honest NotFound, not a silent
    // no-op UPDATE.
    Result r;
    if (Error e = store_.query("SELECT 1 FROM cameras WHERE id=?;", {cameraId}, r);
        !e)
        return e;
    if (r.rows.empty())
        return {Status::NotFound, "unknown channel " + cameraId};
    return store_.exec(
        "UPDATE cameras SET name=?, updated_at=datetime('now') WHERE id=?;",
        {name, cameraId});
}

// The device's name, or NotFound — shared by the group-reconciling channel ops.
Error DeviceRepo::setChannelDisabled(const std::string& deviceId,
                                     const std::string& cameraId, bool disabled) {
    if (deviceId.empty() || cameraId.empty())
        return {Status::Misuse, "device and channel id required"};
    Result r;
    if (Error e = store_.query("SELECT name FROM devices WHERE id=?;", {deviceId}, r);
        !e)
        return e;
    if (r.rows.empty()) return {Status::NotFound, "unknown device " + deviceId};
    const std::string deviceName = std::get<std::string>(r.rows[0][0]);

    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = store_.exec(
            "UPDATE cameras SET disabled=?, updated_at=datetime('now') "
            "WHERE id=? AND device_id=?;",
            {Value{static_cast<std::int64_t>(disabled ? 1 : 0)}, cameraId, deviceId});
        !e)
        return e;
    if (Error e = reconcileGroup(deviceId, deviceName); !e) return e;
    return tx.commit();
}

Error DeviceRepo::removeChannel(const std::string& deviceId,
                                const std::string& cameraId) {
    if (deviceId.empty() || cameraId.empty())
        return {Status::Misuse, "device and channel id required"};
    Result r;
    if (Error e = store_.query("SELECT name FROM devices WHERE id=?;", {deviceId}, r);
        !e)
        return e;
    if (r.rows.empty()) return {Status::NotFound, "unknown device " + deviceId};
    const std::string deviceName = std::get<std::string>(r.rows[0][0]);

    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    if (Error e = store_.exec("DELETE FROM cameras WHERE id=? AND device_id=?;",
                              {cameraId, deviceId});
        !e)
        return e;   // cascade removes its group memberships + stream profiles
    if (Error e = reconcileGroup(deviceId, deviceName); !e) return e;
    return tx.commit();
}

Error DeviceRepo::reorderChannels(const std::string& deviceId,
                                  const std::vector<std::string>& orderedIds) {
    if (deviceId.empty()) return {Status::Misuse, "empty deviceId"};
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;
    // Write each listed channel's position as its sort_order (1-based). Scoped
    // to the device so a stray id can't reorder another device's channel.
    std::int64_t pos = 0;
    for (const std::string& id : orderedIds) {
        ++pos;
        if (Error e = store_.exec(
                "UPDATE cameras SET sort_order=? WHERE id=? AND device_id=?;",
                {Value{pos}, id, deviceId});
            !e)
            return e;
    }
    return tx.commit();
}

Error DeviceRepo::setStreamProfile(const std::string& cameraId,
                                   const std::string& tier, int width,
                                   int height, const std::string& codec) {
    if (cameraId.empty() || tier.empty())
        return {Status::Misuse, "camera id and tier required"};
    return store_.exec(
        "INSERT INTO stream_profiles(camera_id, tier, width, height, codec) "
        "VALUES(?, ?, ?, ?, ?) ON CONFLICT(camera_id, tier) DO UPDATE SET "
        "width=excluded.width, height=excluded.height, codec=excluded.codec;",
        {cameraId, tier, Value{static_cast<std::int64_t>(width)},
         Value{static_cast<std::int64_t>(height)},
         codec.empty() ? Value{nullptr} : Value{codec}});
}

} // namespace vms::persist
