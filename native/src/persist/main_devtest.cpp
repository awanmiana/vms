// Device onboarding self-check — native increment 10 (P2-01/P2-02/P2-05). No Qt,
// no network. Onboards a device into the P0-04 store and proves: atomic onboard,
// the auto default group, idempotent re-onboard, channel sync that preserves
// operator names + stable ids and reconciles the group on add/remove, and an
// end-to-end onboard -> ConnectionBroker credential resolve (the onboarded device
// is immediately connectable). Registered with CTest as `device_selfcheck`.

#include "persist/DeviceRepo.h"
#include "persist/CredentialRepo.h"
#include "persist/Schema.h"
#include "persist/SecretStore.h"
#include "persist/Store.h"
#include "broker/ConnectionBroker.h"

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
long count(Store& s, const std::string& sql, const std::vector<Value>& p = {}) {
    Result r;
    if (!s.query(sql, p, r) || r.rows.empty()) return -1;
    return static_cast<long>(std::get<std::int64_t>(r.rows[0][0]));
}
std::string nameOf(Store& s, const std::string& cameraId) {
    Result r;
    s.query("SELECT name FROM cameras WHERE id=?;", {cameraId}, r);
    return r.rows.empty() ? std::string() : std::get<std::string>(r.rows[0][0]);
}
}  // namespace

int main() {
    std::cout << "vms_devtest — device onboarding self-check\n";

    Store store;
    check(static_cast<bool>(store.open(":memory:")), "open in-memory store");
    check(static_cast<bool>(store.migrate(coreMigrations())), "migrate schema");

    InMemorySecretStore secrets;
    DeviceRepo repo(store, secrets);

    DeviceOnboard dev;
    dev.id = "dev-1";
    dev.name = "NVR 1";
    dev.address = "192.168.0.254";
    dev.vendor = "acme";
    dev.credentialSecret = "admin:p@ss";
    dev.cameras = {
        {"cam-1", "Front", "rtsp://192.168.0.254:554/Streaming/Channels/101",
         "rtsp://192.168.0.254:554/Streaming/Channels/102"},
        {"cam-2", "Lobby", "rtsp://192.168.0.254:554/Streaming/Channels/201",
         "rtsp://192.168.0.254:554/Streaming/Channels/202"},
    };

    // ---- onboard (P2-01) + auto group (P2-02) ----
    check(static_cast<bool>(repo.onboard(dev)), "onboard device + 2 cameras + credential");
    check(count(store, "SELECT COUNT(*) FROM devices WHERE id='dev-1';") == 1, "device row written");
    check(count(store, "SELECT COUNT(*) FROM cameras WHERE device_id='dev-1';") == 2, "2 camera rows written");
    check(count(store, "SELECT COUNT(*) FROM camera_group_members WHERE group_id='device:dev-1';") == 2,
          "auto default group has both cameras (P2-02)");

    // Credential is in the SecretStore under the broker's ref scheme, not in SQLite.
    {
        Result r;
        store.query("SELECT credential_ref FROM device_credentials WHERE device_id='dev-1';", {}, r);
        const bool refOk = !r.rows.empty() &&
            std::get<std::string>(r.rows[0][0]) == CredentialRepo::mintRef("dev-1");
        std::string got;
        check(refOk && static_cast<bool>(secrets.get(CredentialRepo::mintRef("dev-1"), got))
                  && got == "admin:p@ss",
              "credential stored via SecretStore under the broker's credential_ref");
    }

    // ---- end-to-end: the onboarded device is immediately broker-connectable ----
    {
        CredentialRepo creds(store, secrets);
        vms::broker::ConnectionBroker broker(store, creds);
        vms::broker::Session s;
        check(static_cast<bool>(broker.connect("cam-1", vms::broker::StreamKind::Main, s)) &&
                  s.url == "rtsp://admin:p%40ss@192.168.0.254:554/Streaming/Channels/101",
              "onboarded camera resolves through the ConnectionBroker (end-to-end)");
    }

    // ---- idempotent re-onboard ----
    check(static_cast<bool>(repo.onboard(dev)), "re-onboard is idempotent (no error)");
    check(count(store, "SELECT COUNT(*) FROM cameras WHERE device_id='dev-1';") == 2,
          "re-onboard leaves exactly 2 cameras");
    check(count(store, "SELECT COUNT(*) FROM camera_group_members WHERE group_id='device:dev-1';") == 2,
          "re-onboard does not duplicate group members");

    // ---- channel sync (P2-05): preserve operator name + stable ids, add channel ----
    check(static_cast<bool>(store.exec(
              "UPDATE cameras SET name='Front Door (renamed)' WHERE id='cam-1';")),
          "operator renames cam-1");
    {
        std::vector<CameraChannel> discovered = {
            {"cam-1", "Front (device default)",  // the device's name differs...
             "rtsp://192.168.0.254:554/Streaming/Channels/101?v2", ""},
            {"cam-2", "Lobby", "rtsp://192.168.0.254:554/Streaming/Channels/201", ""},
            {"cam-3", "Dock", "rtsp://192.168.0.254:554/Streaming/Channels/301", ""},
        };
        check(static_cast<bool>(repo.syncChannels("dev-1", discovered, /*allowRemoval=*/false)),
              "syncChannels adds cam-3");
        check(nameOf(store, "cam-1") == "Front Door (renamed)",
              "channel sync PRESERVES the operator-customized name (P2-05)");
        Result r;
        store.query("SELECT main_url FROM cameras WHERE id='cam-1';", {}, r);
        check(!r.rows.empty() &&
                  std::get<std::string>(r.rows[0][0]).find("?v2") != std::string::npos,
              "channel sync refreshes the technical URL template");
        check(count(store, "SELECT COUNT(*) FROM cameras WHERE device_id='dev-1';") == 3,
              "cam-3 added; stable ids preserved");
        check(count(store, "SELECT COUNT(*) FROM camera_group_members WHERE group_id='device:dev-1';") == 3,
              "default group reconciled to 3 cameras");
    }

    // ---- channel sync with removal ----
    {
        std::vector<CameraChannel> onlyOne = {
            {"cam-1", "x", "rtsp://192.168.0.254:554/Streaming/Channels/101", ""}};
        check(static_cast<bool>(repo.syncChannels("dev-1", onlyOne, /*allowRemoval=*/true)),
              "syncChannels(allowRemoval) drops cam-2 and cam-3");
        check(count(store, "SELECT COUNT(*) FROM cameras WHERE device_id='dev-1';") == 1,
              "only cam-1 remains");
        check(count(store, "SELECT COUNT(*) FROM camera_group_members WHERE group_id='device:dev-1';") == 1,
              "default group reconciled to 1 camera");
        check(nameOf(store, "cam-1") == "Front Door (renamed)",
              "surviving camera keeps its stable id and operator name");
    }

    // ---- remove the device ----
    {
        check(static_cast<bool>(repo.remove("dev-1")), "remove device");
        check(count(store, "SELECT COUNT(*) FROM devices WHERE id='dev-1';") == 0, "device gone");
        check(count(store, "SELECT COUNT(*) FROM cameras WHERE device_id='dev-1';") == 0, "cameras gone");
        check(count(store, "SELECT COUNT(*) FROM camera_groups WHERE id='device:dev-1';") == 0,
              "default group gone");
        std::string got;
        check(secrets.get(CredentialRepo::mintRef("dev-1"), got).status == Status::NotFound,
              "credential secret removed");
    }

    if (failures == 0) {
        std::cout << "PASS: atomic onboard, auto group, idempotent re-onboard, "
                     "operator-preserving channel sync, broker resolve, and removal verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
