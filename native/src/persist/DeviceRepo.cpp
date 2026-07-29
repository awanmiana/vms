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

Error DeviceRepo::reconcileGroup(const std::string& deviceId,
                                 const std::string& deviceName,
                                 const std::vector<std::string>& ids) {
    const std::string gid = defaultGroupId(deviceId);
    // The default group exists and is named after the device; membership is
    // rewritten to exactly the device's current cameras (simple + correct: the
    // default group is always the device's channel set).
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
    for (const std::string& cid : ids) {
        if (Error e = store_.exec(
                "INSERT INTO camera_group_members(group_id, camera_id) VALUES(?, ?);",
                {gid, cid});
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
            "INSERT INTO devices(id, name, address, vendor) VALUES(?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET name=excluded.name, "
            "address=excluded.address, vendor=excluded.vendor, "
            "updated_at=datetime('now');",
            {d.id, d.name,
             d.address.empty() ? Value{nullptr} : Value{d.address},
             d.vendor.empty() ? Value{nullptr} : Value{d.vendor}});
        !e)
        return e;

    std::vector<std::string> ids;
    for (const CameraChannel& c : d.cameras) {
        if (c.id.empty()) return {Status::Misuse, "camera requires a stable id"};
        // Onboard sets the operator-facing name; URL templates are credential-free.
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
        ids.push_back(c.id);
    }

    if (Error e = reconcileGroup(d.id, d.name, ids); !e) return e;

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
    } else {
        // Keep retained channels in the group set too.
        for (const std::string& old : existing)
            if (std::find(wanted.begin(), wanted.end(), old) == wanted.end())
                wanted.push_back(old);
    }

    std::sort(wanted.begin(), wanted.end());
    if (Error e = reconcileGroup(deviceId, deviceName, wanted); !e) return e;

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

} // namespace vms::persist
