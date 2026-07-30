// P3-14 slice 1 — Qt Quick workspace rendering the governor's honest per-tile
// state. See WorkspaceController.h for the rationale. Two modes:
//
//   vms_workspace [--count N] [--profile devbox|lowend] [--sweep-interval S]
//       Opens a Qt Quick window: a governed grid of tiles, each drawn in its
//       honest state (live / degraded / capacity-paused / off-screen), with an
//       aggregate capacity meter. A focus sweep re-plans every S seconds so the
//       state chrome visibly moves, exactly as vms_grid --sweep does.
//
//   vms_workspace --selftest [--count N] [--profile ...]
//       Headless: builds the same plan, prints it, runs a few sweeps, checks
//       the governor actually degraded a large grid, and exits. No window — used
//       to build-verify this increment without a display or a camera.
//
// This slice has no media pipeline; it depends only on Qt Quick and the pure
// vms_governor decision library. Routing the live d3d11-composited video under
// the state chrome (and seeding the profile from the live hardware probe, as
// vms_grid --profile auto already does) is the next slice.

#include "WorkspaceController.h"
#include "VideoItem.h"

#include "governor/Governor.h"

#ifdef VMS_WITH_OPTIMIZE
#include <memory>

#include "optimize/Optimizer.h"
#include "optimize/SystemHealth.h"
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

#ifdef VMS_WITH_PERSIST
#include <map>
#include <memory>

#include <QDir>
#include <QStandardPaths>
#include <QTimer>

#include "persist/Schema.h"
#include "persist/Store.h"
#include "persist/WorkspaceRepo.h"
#include "persist/SegmentIndex.h"
#include "persist/DeviceRepo.h"
#include "persist/SecretStore.h"
#include "health/HealthMonitor.h"
#include "PlaybackController.h"
#include "DeviceController.h"

#ifdef VMS_WITH_ONVIF
#include "onvif/DiscoverySource.h"

namespace {
// A fixture-backed DiscoverySource for --devices-demo and --devices-selftest:
// it serves two canned ONVIF candidates and, on fetch, a two-profile device
// (main 1080p + sub 480p H.264), so the whole discover -> map -> onboard flow
// verifies offscreen with no LAN. The LIVE source (MakeLiveDiscoverySource) is
// used for real windowed runs; this fake is never used there.
class FakeDiscoverySource : public vms::onvif::DiscoverySource {
public:
    std::vector<vms::onvif::DiscoveryCandidate> discover(
        int /*timeoutMs*/, std::string& error) override {
        error.clear();
        std::vector<vms::onvif::DiscoveryCandidate> out;
        // One host collides with a seeded demo device (192.168.0.11 =
        // cam-front) to exercise duplicate detection; one is new.
        out.push_back({"urn:uuid:fake-existing-0001", "HIKVISION DS-2CD2042",
                       "DS-2CD2042WD-I",
                       "http://192.168.0.11/onvif/device_service", "192.168.0.11"});
        out.push_back({"urn:uuid:fake-new-0002", "AXIS M3045", "M3045-V",
                       "http://192.168.0.77/onvif/device_service", "192.168.0.77"});
        return out;
    }
    bool fetch(const std::string& serviceUrl, const std::string& /*user*/,
               const std::string& /*password*/, vms::onvif::OnvifDevice& out,
               std::string& error) override {
        error.clear();
        out.info.manufacturer = serviceUrl.find("192.168.0.77") != std::string::npos
                                    ? "Axis" : "Hikvision";
        out.info.model = "Fixture Camera";
        out.info.firmware = "V5.4.5";
        out.profiles = {{"Profile_1", "mainStream", "H264", 1920, 1080},
                        {"Profile_2", "subStream", "H264", 640, 480}};
        // Parallel RTSP URIs (credential-free, as ONVIF GetStreamUri returns).
        const std::string host =
            serviceUrl.find("192.168.0.77") != std::string::npos
                ? "192.168.0.77" : "192.168.0.11";
        out.streamUris = {
            "rtsp://" + host + ":554/Streaming/Channels/101",
            "rtsp://" + host + ":554/Streaming/Channels/102"};
        return true;
    }
};
}  // namespace
#endif  // VMS_WITH_ONVIF

namespace {

// P0-04 inc 5c: restore a stateful instance's saved layout (tile count + per-tile
// desired-tier + priority overrides) onto its controller. A no-op on first run.
void restoreInstance(vms::persist::Store& store, const std::string& name,
                     WorkspaceController& c) {
    vms::persist::WorkspaceRepo repo(store);
    vms::persist::InstanceState st;
    bool found = false;
    if (repo.load(name, st, found) && found && st.tileCount > 0) {
        c.setTileCount(st.tileCount);
        for (const auto& t : st.tiles) {
            c.setDesiredTier(t.id, t.desiredTier);
            c.setPriority(t.id, t.priority);
        }
    }
}

// Save an instance's current layout so the next launch restores it.
void saveInstance(vms::persist::Store& store, const std::string& name,
                  const WorkspaceController& c) {
    vms::persist::WorkspaceRepo repo(store);
    vms::persist::InstanceState st;
    st.tileCount = c.tileCount();
    for (int i = 0; i < c.tileCount(); ++i)
        st.tiles.push_back({i, c.desiredTierOf(i), c.priorityOf(i)});
    repo.save(name, st);
}

// P5-02 / inc 7c: headless check that PlaybackController turns the recording
// SegmentIndex into an honest availability model and a playhead that knows when
// it is over footage vs a gap. No window, no camera.
int runPlaybackSelftest() {
    using namespace vms::persist;
    int failures = 0;
    auto check = [&](bool c, const std::string& what) {
        std::cout << (c ? "  ok  " : "  FAIL ") << what << "\n";
        if (!c) ++failures;
    };

    Store store;
    check(static_cast<bool>(store.open(":memory:")), "open in-memory store");
    check(static_cast<bool>(store.migrate(coreMigrations())), "migrate schema");
    SegmentIndex idx(store);
    auto add = [&](const char* s, const char* e, const char* p) {
        Segment seg;
        seg.cameraId = "cam-1"; seg.startUtc = s; seg.endUtc = e;
        seg.path = p; seg.codec = "h264"; seg.bytes = 1000;
        std::int64_t id = 0; idx.add(seg, id);
    };
    add("2026-07-29 09:00:00", "2026-07-29 09:10:00", "a.mp4");
    add("2026-07-29 09:10:00", "2026-07-29 09:20:00", "b.mp4");
    add("2026-07-29 09:30:00", "2026-07-29 09:40:00", "c.mp4");   // 09:20-09:30 is a gap

    PlaybackController pc(&idx, QStringLiteral("cam-1"));
    pc.setRange(QStringLiteral("2026-07-29 09:00:00"),
                QStringLiteral("2026-07-29 09:40:00"));
    std::cout << "vms_workspace --playback-selftest\nspans:\n"
              << pc.dumpSpans().toStdString();

    check(pc.spans().size() == 3, "3 availability spans (available / missing / available)");
    if (pc.spans().size() == 3) {
        check(pc.spans()[0].toMap().value(QStringLiteral("state")).toString() ==
                  QLatin1String("available"),
              "span 0 is Available (contiguous footage merged)");
        check(pc.spans()[1].toMap().value(QStringLiteral("state")).toString() ==
                  QLatin1String("missing"),
              "span 1 is an honest Missing gap");
    }

    pc.seekFrac(0.05);   // ~09:02, inside a.mp4
    check(pc.onFootage(), "playhead over footage reports onFootage=true");
    check(pc.segmentAtPlayhead().value(QStringLiteral("path")).toString() ==
              QLatin1String("a.mp4"),
          "segmentAtPlayhead resolves the backing recorded file");

    pc.seekFrac(0.625);  // 09:25, inside the gap
    check(!pc.onFootage(), "playhead in a gap reports onFootage=false (honest)");
    check(pc.segmentAtPlayhead().value(QStringLiteral("path")).toString().isEmpty(),
          "no backing file over a gap (requested time not shown as recorded)");

    pc.play();
    check(pc.playing(), "play() sets playing");
    pc.setSpeed(2.0);
    check(pc.speed() == 2.0, "setSpeed() updates the transport speed");

    if (failures == 0) {
        std::cout << "PASS: playback availability model, playhead footage/gap "
                     "detection, and transport state verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

// inc 19: a fixture reachability probe for --devices-selftest — scripted per
// device (default reachable) so pollHealth() drives health with no socket.
class FakeHealthProbe : public vms::health::DeviceHealthProbe {
public:
    std::map<std::string, bool> reachable;   // deviceId -> reachable
    vms::health::ProbeResult probe(const vms::health::ProbeTarget& t) override {
        vms::health::ProbeResult r;
        r.deviceId = t.deviceId;
        auto it = reachable.find(t.deviceId);
        r.reachable = (it == reachable.end()) ? true : it->second;
        return r;
    }
};

// increment 14 (onboarding UI): headless check that DeviceController turns the
// DeviceRepo + HealthMonitor cores into the honest model the Devices tab binds
// to — onboard → Unknown-until-observed → Offline/Online derivation → exception
// raise / acknowledge / auto-clear → remove — with the credential stored only
// via the SecretStore (never exposed). No window, no camera.
int runDevicesSelftest() {
    using namespace vms::persist;
    int failures = 0;
    auto check = [&](bool c, const std::string& what) {
        std::cout << (c ? "  ok  " : "  FAIL ") << what << "\n";
        if (!c) ++failures;
    };

    Store store;
    check(static_cast<bool>(store.open(":memory:")), "open in-memory store");
    check(static_cast<bool>(store.migrate(coreMigrations())), "migrate schema");
    check(store.schemaVersion() == 7, "schema at v7 (channel sort_order, inc 18)");
    InMemorySecretStore secrets;
    DeviceRepo repo(store, secrets);
    vms::health::HealthMonitor health;
    FakeHealthProbe fakeProbe;
#ifdef VMS_WITH_ONVIF
    FakeDiscoverySource fakeDisc;
    DeviceController ctrl(&repo, &health, &fakeDisc, &fakeProbe);
#else
    DeviceController ctrl(&repo, &health, nullptr, &fakeProbe);
#endif

    std::cout << "vms_workspace --devices-selftest\n";
    check(ctrl.deviceCount() == 0, "starts with no devices");

    QString err = ctrl.onboard(
        QStringLiteral("cam-1"), QStringLiteral("Front Door"),
        QStringLiteral("192.168.0.11"), QStringLiteral("Hikvision"),
        QStringLiteral("admin"), QStringLiteral("secret@1"),
        QStringLiteral("rtsp://192.168.0.11:554/Streaming/Channels/101"),
        QStringLiteral("rtsp://192.168.0.11:554/Streaming/Channels/102"));
    check(err.isEmpty(), "onboard via the controller succeeds");
    check(ctrl.deviceCount() == 1, "device appears in the model");

    // The credential is stored via the SecretStore under the broker's ref (and
    // never read back by the controller) — proving the onboard atomicity path.
    std::string secret;
    check(static_cast<bool>(secrets.get(CredentialRepo::mintRef("cam-1"), secret)) &&
              secret == "admin:secret@1",
          "credential stored via SecretStore under the broker ref");

    QVariantMap d0 = ctrl.devices().first().toMap();
    check(d0.value(QStringLiteral("cameraCount")).toInt() == 1,
          "one camera channel onboarded");
    check(d0.value(QStringLiteral("health")).toMap()
              .value(QStringLiteral("state")).toString() == QLatin1String("unknown"),
          "new device health is Unknown until observed");

    auto healthOf = [&]() {
        return ctrl.devices().first().toMap()
            .value(QStringLiteral("health")).toMap();
    };

    ctrl.reportReach(QStringLiteral("cam-1"), 0);   // offline
    check(healthOf().value(QStringLiteral("state")).toString() ==
              QLatin1String("offline"),
          "reported offline -> Offline");
    check(healthOf().value(QStringLiteral("exceptionActive")).toBool(),
          "offline raises an exception");
    check(ctrl.attentionCount() == 1, "offline device needs attention");

    ctrl.acknowledge(QStringLiteral("cam-1"));
    check(healthOf().value(QStringLiteral("exceptionActive")).toBool() &&
              healthOf().value(QStringLiteral("exceptionAcknowledged")).toBool(),
          "acknowledge silences the alert but keeps the exception");
    check(ctrl.attentionCount() == 0,
          "acknowledged device no longer needs attention");

    ctrl.reportReach(QStringLiteral("cam-1"), 1);   // online
    ctrl.reportStream(QStringLiteral("cam-1"), 2);  // ok
    check(healthOf().value(QStringLiteral("state")).toString() ==
              QLatin1String("online"),
          "reachable + stream ok -> Online");
    check(!healthOf().value(QStringLiteral("exceptionActive")).toBool(),
          "recovery auto-clears the exception");

    err = ctrl.removeDevice(QStringLiteral("cam-1"));
    check(err.isEmpty() && ctrl.deviceCount() == 0, "remove clears the device");
    check(!secrets.contains(CredentialRepo::mintRef("cam-1")),
          "remove deletes the credential secret");

    // inc 15: a multi-channel recorder onboards atomically — N channels expand
    // from the URL templates and the default group is reconciled to all N.
    err = ctrl.onboardRecorder(
        QStringLiteral("nvr-1"), QStringLiteral("Lobby NVR"),
        QStringLiteral("192.168.0.20"), QStringLiteral("Hikvision"),
        QStringLiteral("nvr"), QStringLiteral("admin"), QStringLiteral("pw"),
        4, QStringLiteral("rtsp://192.168.0.20:554/Streaming/Channels/{ch}01"),
        QStringLiteral("rtsp://192.168.0.20:554/Streaming/Channels/{ch}02"));
    check(err.isEmpty(), "recorder onboard succeeds");
    check(ctrl.deviceCount() == 1, "recorder appears as one device");
    QVariantMap nvr = ctrl.devices().first().toMap();
    check(nvr.value(QStringLiteral("kind")).toString() == QLatin1String("nvr"),
          "device kind persisted as nvr");
    check(nvr.value(QStringLiteral("cameraCount")).toInt() == 4,
          "4 channels expanded from the templates");

    std::vector<std::string> ids;
    repo.cameraIds("nvr-1", ids);
    check(ids.size() == 4 && ids.front() == "nvr-1-ch1" && ids.back() == "nvr-1-ch4",
          "channels have stable ids nvr-1-ch1..ch4");

    // The default group (P2-02) holds exactly the 4 channels.
    Result gr;
    store.query("SELECT COUNT(*) FROM camera_group_members WHERE group_id=?;",
                {DeviceRepo::defaultGroupId("nvr-1")}, gr);
    check(!gr.rows.empty() && std::get<std::int64_t>(gr.rows[0][0]) == 4,
          "default group reconciled to all 4 channels");

    // Idempotent re-scan to 6 channels grows the inventory + the group.
    ctrl.onboardRecorder(
        QStringLiteral("nvr-1"), QStringLiteral("Lobby NVR"),
        QStringLiteral("192.168.0.20"), QStringLiteral("Hikvision"),
        QStringLiteral("nvr"), QStringLiteral("admin"), QStringLiteral("pw"),
        6, QStringLiteral("rtsp://192.168.0.20:554/Streaming/Channels/{ch}01"),
        QString());
    check(ctrl.devices().first().toMap().value(QStringLiteral("cameraCount")).toInt() == 6,
          "re-onboard to 6 channels is idempotent (device stays single, grows to 6)");

    // inc 17: per-channel management (P2-05). The device model carries the
    // channel list the panel binds to; rename/disable/remove operate on one
    // channel and reconcile the default group (disabled/removed leave it).
    auto nvrChannels = [&]() {
        return ctrl.devices().first().toMap()
            .value(QStringLiteral("channels")).toList();
    };
    auto groupCount = [&]() -> std::int64_t {
        Result g;
        store.query("SELECT COUNT(*) FROM camera_group_members WHERE group_id=?;",
                    {DeviceRepo::defaultGroupId("nvr-1")}, g);
        return g.rows.empty() ? -1 : std::get<std::int64_t>(g.rows[0][0]);
    };
    check(nvrChannels().size() == 6, "channel model lists all 6 channels");
    check(groupCount() == 6, "default group holds all 6 enabled channels");

    ctrl.renameChannel(QStringLiteral("nvr-1"), QStringLiteral("nvr-1-ch1"),
                       QStringLiteral("Front Cam"));
    check(nvrChannels().first().toMap().value(QStringLiteral("name")).toString() ==
              QLatin1String("Front Cam"),
          "renameChannel updates the operator name, stable id preserved");

    ctrl.setChannelDisabled(QStringLiteral("nvr-1"), QStringLiteral("nvr-1-ch1"), true);
    check(nvrChannels().first().toMap().value(QStringLiteral("disabled")).toBool(),
          "setChannelDisabled marks the channel disabled");
    check(nvrChannels().size() == 6,
          "a disabled channel stays in inventory (still 6)");
    check(groupCount() == 5,
          "disabled channel is excluded from the default group (6 -> 5)");

    QString rc = ctrl.removeChannel(QStringLiteral("nvr-1"),
                                    QStringLiteral("nvr-1-ch2"));
    check(rc.isEmpty(), "removeChannel succeeds");
    check(ctrl.devices().first().toMap().value(QStringLiteral("cameraCount")).toInt() == 5,
          "removed channel leaves inventory (6 -> 5)");
    check(groupCount() == 4,
          "group now holds 4 (5 channels, 1 still disabled)");

    ctrl.setChannelDisabled(QStringLiteral("nvr-1"), QStringLiteral("nvr-1-ch1"), false);
    check(groupCount() == 5, "re-enabling a channel restores it to the group");

    // inc 18: channel reorder (P2-05). nvr-1's channels list order by sort_order
    // then id — initially ch1, ch3, ch4, ch5, ch6 (ch2 was removed above).
    auto chanIdAt = [&](int i) {
        return nvrChannels().at(i).toMap().value(QStringLiteral("id")).toString();
    };
    check(chanIdAt(0) == QLatin1String("nvr-1-ch1") &&
              chanIdAt(1) == QLatin1String("nvr-1-ch3"),
          "initial channel order is by id");
    ctrl.moveChannel(QStringLiteral("nvr-1"), QStringLiteral("nvr-1-ch1"), false);
    check(chanIdAt(0) == QLatin1String("nvr-1-ch3") &&
              chanIdAt(1) == QLatin1String("nvr-1-ch1"),
          "moveChannel down reorders ch1 below ch3");
    ctrl.moveChannel(QStringLiteral("nvr-1"), QStringLiteral("nvr-1-ch1"), true);
    check(chanIdAt(0) == QLatin1String("nvr-1-ch1"),
          "moveChannel up restores ch1 to the top");
    ctrl.moveChannel(QStringLiteral("nvr-1"), QStringLiteral("nvr-1-ch1"), true);
    check(chanIdAt(0) == QLatin1String("nvr-1-ch1"),
          "moveChannel up at the top is a no-op");

#ifdef VMS_WITH_ONVIF
    // inc 16: ONVIF discovery-as-a-source. The fixture-backed DiscoverySource
    // serves two candidates; the flow discovers, dedups against inventory, maps
    // ONVIF profiles -> Main/Sub RTSP, and onboards atomically — all offscreen.
    check(ctrl.discoveryAvailable(), "discovery is available (source wired)");

    // Pure profile -> stream mapping: Main = highest resolution, Sub = a lower
    // one; a JPEG-only / URI-less profile is ignored.
    {
        vms::onvif::OnvifDevice dev;
        dev.profiles = {{"P1", "main", "H265", 2560, 1440},
                        {"P2", "snap", "JPEG", 640, 480},
                        {"P3", "sub", "H264", 640, 480}};
        dev.streamUris = {"rtsp://x/main", "rtsp://x/jpeg", "rtsp://x/sub"};
        std::string mainUrl, subUrl;
        DeviceController::chooseStreams(dev, mainUrl, subUrl);
        check(mainUrl == "rtsp://x/main" && subUrl == "rtsp://x/sub",
              "chooseStreams: Main=largest H26x, Sub=smaller, JPEG ignored");
    }
    check(DeviceController::deviceIdFromHost("192.168.0.77") ==
              "onvif-192-168-0-77",
          "deviceIdFromHost sanitizes the host to a stable id");

    ctrl.startDiscovery(100);
    check(ctrl.discoveredDevices().size() == 2, "discovery lists 2 candidates");
    // The current inventory (nvr-1 @ .0.20) matches neither host, so both are new.
    check(!ctrl.discoveredDevices().first().toMap()
               .value(QStringLiteral("alreadyOnboarded")).toBool(),
          "candidate not yet onboarded is flagged new");

    QString derr = ctrl.onboardDiscovered(
        QStringLiteral("urn:uuid:fake-new-0002"),
        QStringLiteral("admin"), QStringLiteral("onvifpw"));
    check(derr.isEmpty(), "onboardDiscovered succeeds");
    check(ctrl.deviceCount() == 2, "the discovered device is now in the inventory");

    // The onboarded device carries the mapped id, one channel, and the ONVIF
    // credential — stored only via the SecretStore under the broker ref.
    bool foundOnvif = false;
    for (const QVariant& v : ctrl.devices()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toString() ==
            QLatin1String("onvif-192-168-0-77")) {
            foundOnvif = true;
            check(m.value(QStringLiteral("cameraCount")).toInt() == 1,
                  "discovered camera onboarded with one channel");
            check(m.value(QStringLiteral("vendor")).toString() ==
                      QLatin1String("Axis"),
                  "vendor taken from ONVIF device info");
        }
    }
    check(foundOnvif, "discovered device appears under its mapped id");
    std::string onvifSecret;
    check(static_cast<bool>(
              secrets.get(CredentialRepo::mintRef("onvif-192-168-0-77"),
                          onvifSecret)) &&
              onvifSecret == "admin:onvifpw",
          "ONVIF credential stored via SecretStore under the broker ref");

    // After onboarding, re-scanning flags that candidate as already onboarded
    // (P2-03 duplicate detection against the live inventory).
    ctrl.startDiscovery(100);
    bool dupFlagged = false;
    for (const QVariant& v : ctrl.discoveredDevices()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("host")).toString() ==
            QLatin1String("192.168.0.77"))
            dupFlagged = m.value(QStringLiteral("alreadyOnboarded")).toBool();
    }
    check(dupFlagged, "re-scan flags the onboarded device as already onboarded");

    // inc 18: the discovered device's stream profile (resolution/codec) was
    // synced from its ONVIF media profiles (fixture main = 1920x1080 H264).
    QVariantMap onvifChan;
    for (const QVariant& v : ctrl.devices()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toString() ==
            QLatin1String("onvif-192-168-0-77"))
            onvifChan = m.value(QStringLiteral("channels")).toList()
                            .first().toMap();
    }
    check(onvifChan.value(QStringLiteral("width")).toInt() == 1920 &&
              onvifChan.value(QStringLiteral("height")).toInt() == 1080 &&
              onvifChan.value(QStringLiteral("codec")).toString() ==
                  QLatin1String("H264"),
          "discovered channel's main stream profile synced (1920x1080 H264)");
#endif  // VMS_WITH_ONVIF

    // inc 19: device-health LIVE FEED. A fixture reachability probe drives
    // health automatically via pollHealth(): Unknown -> Online/Offline, and
    // recover when the probe reports reachable again.
    check(ctrl.healthFeedAvailable(), "health feed available (probe wired)");
    ctrl.onboard(QStringLiteral("h-up"), QStringLiteral("Reachable Cam"),
                 QStringLiteral("192.168.9.10"), QStringLiteral("Acme"),
                 QStringLiteral("u"), QStringLiteral("p"),
                 QStringLiteral("rtsp://192.168.9.10/1"), QString());
    ctrl.onboard(QStringLiteral("h-down"), QStringLiteral("Down Cam"),
                 QStringLiteral("192.168.9.11"), QStringLiteral("Acme"),
                 QStringLiteral("u"), QStringLiteral("p"),
                 QStringLiteral("rtsp://192.168.9.11/1"), QString());
    auto stateOf = [&](const QString& id) -> QString {
        for (const QVariant& v : ctrl.devices()) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("id")).toString() == id)
                return m.value(QStringLiteral("health")).toMap()
                    .value(QStringLiteral("state")).toString();
        }
        return QStringLiteral("<none>");
    };
    auto attentionOf = [&](const QString& id) -> bool {
        for (const QVariant& v : ctrl.devices()) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("id")).toString() == id)
                return m.value(QStringLiteral("health")).toMap()
                    .value(QStringLiteral("needsAttention")).toBool();
        }
        return false;
    };
    check(stateOf(QStringLiteral("h-up")) == QLatin1String("unknown"),
          "a newly onboarded device is Unknown until the feed observes it");

    fakeProbe.reachable[std::string("h-up")] = true;
    fakeProbe.reachable[std::string("h-down")] = false;
    ctrl.pollHealth();
    check(stateOf(QStringLiteral("h-up")) == QLatin1String("online"),
          "reachable device -> Online via the live feed");
    check(stateOf(QStringLiteral("h-down")) == QLatin1String("offline"),
          "unreachable device -> Offline via the live feed");
    check(attentionOf(QStringLiteral("h-down")),
          "a device the feed finds offline needs attention");

    fakeProbe.reachable[std::string("h-down")] = true;   // came back
    ctrl.pollHealth();
    check(stateOf(QStringLiteral("h-down")) == QLatin1String("online"),
          "device recovers to Online when the feed reports reachable again");

    if (failures == 0) {
        std::cout << "PASS: onboard -> honest health (Unknown/Offline/Online) -> "
                     "exception raise/ack/auto-clear -> remove; recorder onboard "
                     "expands N channels + reconciles the default group, via the "
                     "DeviceController model\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

} // namespace
#endif // VMS_WITH_PERSIST

#ifdef VMS_WITH_GSTREAMER
#include <gst/gst.h>

#include <QObject>
#include <QTimer>

#include "GridPipeline.h"
#include "PlaybackPipeline.h"
#include "hardware/HardwareProbe.h"

#include <sstream>

namespace {

// First registered GStreamer factory from a preference-ordered list, or nullptr.
const char* PickFactory(const char* const* names) {
    for (const char* const* p = names; *p; ++p) {
        if (GstElementFactory* f = gst_element_factory_find(*p)) {
            gst_object_unref(f);
            return *p;
        }
    }
    return nullptr;
}

// Build a conservative capacity profile from the live hardware probe (ported
// from vms_grid): a tile decodes in hardware only if this machine both reports
// the D3D per-codec capability AND has a registered GStreamer hardware decoder.
// gst_init() must have run first (the registry is queried). See MEASUREMENTS.md.
vms::CapacityProfile AutoCapacityProfile(const std::string& codec) {
    const vms::MachineProfile machine = vms::ProbeMachine();
    const vms::Codec workingCodec =
        (codec == "h264") ? vms::Codec::H264 : vms::Codec::H265;
    const char* const h264Hw[] = {"d3d12h264dec", "d3d11h264dec", "nvh264dec",
                                   "qsvh264dec", nullptr};
    const char* const h265Hw[] = {"d3d12h265dec", "d3d11h265dec", "nvh265dec",
                                   "qsvh265dec", nullptr};
    const bool hasRegisteredHwDecoder =
        PickFactory(workingCodec == vms::Codec::H264 ? h264Hw : h265Hw) != nullptr;

    vms::HardwareInputs hw;
    hw.totalRamMb =
        static_cast<double>(machine.totalRamBytes) / (1024.0 * 1024.0);
    hw.logicalCores = machine.logicalCores;

    std::string decoderAdapter;
    for (const auto& gpu : machine.gpus) {
        if (gpu.isSoftwareAdapter) continue;
        const bool supports = (workingCodec == vms::Codec::H264)
                                  ? gpu.decode.h264
                                  : gpu.decode.h265Main;
        if (!supports || !hasRegisteredHwDecoder) continue;

        hw.hasHardwareDecode = true;
        const double dedicatedMb =
            static_cast<double>(gpu.dedicatedVideoMemoryBytes) / (1024.0 * 1024.0);
        if (dedicatedMb >= 512.0 && dedicatedMb > hw.videoMemoryMb) {
            hw.videoMemoryMb = dedicatedMb;
            decoderAdapter = gpu.description;
        } else if (decoderAdapter.empty()) {
            decoderAdapter = gpu.description;
        }
    }

    std::ostringstream label;
    label << "auto " << codec << " (" << machine.tier << ", "
          << (hw.hasHardwareDecode ? "hardware" : "software") << " decode";
    if (!decoderAdapter.empty()) label << ", " << decoderAdapter;
    label << ")";
    return vms::MakeCapacityProfile(hw, label.str());
}

} // namespace
#endif // VMS_WITH_GSTREAMER

namespace {

// Display-cased tier for the big tile label ("MAIN", "SUB", ...).
QString tierBadge(vms::Tier t) {
    switch (t) {
        case vms::Tier::Main:   return QStringLiteral("MAIN");
        case vms::Tier::Sub:    return QStringLiteral("SUB");
        case vms::Tier::Thumb:  return QStringLiteral("THUMB");
        case vms::Tier::Paused: return QStringLiteral("PAUSED");
    }
    return QStringLiteral("?");
}

// A stable kebab category QML switches on for colour, plus a display string.
struct StateView { QString category; QString text; };

StateView stateView(vms::TileState s) {
    switch (s) {
        case vms::TileState::Live:
            return {QStringLiteral("live"), QStringLiteral("Live")};
        case vms::TileState::Degraded:
            return {QStringLiteral("degraded"), QStringLiteral("Degraded")};
        case vms::TileState::PausedCapacity:
            return {QStringLiteral("paused-capacity"),
                    QStringLiteral("Capacity-paused")};
        case vms::TileState::PausedOffscreen:
            return {QStringLiteral("paused-offscreen"),
                    QStringLiteral("Off-screen")};
    }
    return {QStringLiteral("unknown"), QStringLiteral("?")};
}

} // namespace

// --- WorkspaceController ----------------------------------------------------

WorkspaceController::WorkspaceController(vms::CapacityProfile profile, int count,
                                         QObject* parent)
    : QObject(parent), session_(profile), baseProfile_(profile) {
    profileLabel_ = QString::fromStdString(profile.label.empty()
                                               ? std::string("(unlabelled)")
                                               : profile.label);

    // Same request shape vms_grid uses: every tile wants Main; tile 0 is the
    // focused/High working-set tile, the rest Medium.
    // The two axes are kept orthogonal (ARCHITECTURE.md): `focused` marks the
    // working-set tile (protected at Main); `priority` is the operator's
    // device-activity signal (persistent, a degrade tie-breaker under pressure).
    // Focus never touches priority, so an operator's High mark survives a sweep.
    requests_.reserve(count);
    for (int i = 0; i < count; ++i) {
        vms::TileRequest r;
        r.id = i;
        r.desired = vms::Tier::Main;
        r.priority = vms::Priority::Medium;
        r.focused = (i == 0);
        r.visible = true;
        requests_.push_back(r);
    }
    focusIndex_ = 0;

    columns_ = std::max(1, static_cast<int>(std::ceil(std::sqrt(
                               static_cast<double>(count)))));
    rows_ = std::max(1, static_cast<int>(std::ceil(
                            static_cast<double>(count) / columns_)));

    plan_ = session_.update(requests_);
    rebuildModel();

    connect(&timer_, &QTimer::timeout, this, [this]() { sweep(); });
}

void WorkspaceController::rebuildModel(bool isLayoutChange) {
    const int n = static_cast<int>(requests_.size());

    // Index by id so a tile keeps its grid cell across re-plans (no flapping
    // position). Tiles absent from the plan default to an off-screen pause.
    std::vector<vms::Tier> tier(n, vms::Tier::Paused);
    std::vector<vms::TileState> state(n, vms::TileState::PausedOffscreen);
    for (const auto& d : plan_.tiles) {
        if (d.id >= 0 && d.id < n) {
            tier[d.id] = d.tier;
            state[d.id] = d.state;
        }
    }

    tiles_.clear();
    for (int i = 0; i < n; ++i) {
        const StateView sv = stateView(state[i]);
        QVariantMap m;
        m.insert(QStringLiteral("id"), i);
        m.insert(QStringLiteral("tier"), tierBadge(tier[i]));
        m.insert(QStringLiteral("state"), sv.category);
        m.insert(QStringLiteral("stateText"), sv.text);
        m.insert(QStringLiteral("focused"), i == focusIndex_);
        m.insert(QStringLiteral("priority"),
                 QString::fromLatin1(vms::PriorityName(requests_[i].priority)));
        // The operator's desired-tier ceiling ("off" when Paused), distinct from
        // the tier the governor actually assigned above.
        m.insert(QStringLiteral("desired"),
                 requests_[i].desired == vms::Tier::Paused
                     ? QStringLiteral("off")
                     : QString::fromLatin1(vms::TierName(requests_[i].desired)));
        tiles_.push_back(m);
    }

    overflow_ = plan_.overflow;
    capacity_ = QStringLiteral("decode %1 / %2 main-eq   ·   mem %3 / %4 MB"
                               "   ·   decoding %5 / %6")
                    .arg(plan_.decodeUsed, 0, 'f', 1)
                    .arg(session_.profile().decodeBudget, 0, 'f', 0)
                    .arg(plan_.memoryUsedMb, 0, 'f', 0)
                    .arg(session_.profile().memoryBudgetMb, 0, 'f', 0)
                    .arg(plan_.decoding)
                    .arg(n);

    emit changed();
    // A layout change rebuilds the whole video grid; a same-size re-plan only
    // re-tiers the existing branches. Keep them distinct so the video follows
    // correctly (a planChanged handler must not run against a stale tile count).
    if (isLayoutChange)
        emit layoutChanged();
    else
        emit planChanged();
}

QString WorkspaceController::sweep() {
    const int n = static_cast<int>(requests_.size());
    if (n == 0) return QStringLiteral("no tiles");

    focusIndex_ = (focusIndex_ + 1) % n;
    for (auto& r : requests_) r.focused = false;  // priority is left untouched
    requests_[focusIndex_].focused = true;

    const vms::GovernorResult next = session_.update(requests_);
    const std::vector<vms::TileTransition> moves = vms::DiffPlans(plan_, next);
    plan_ = next;
    rebuildModel();

    QString summary = QStringLiteral("focus -> tile %1; %2 transition(s)")
                          .arg(focusIndex_)
                          .arg(moves.size());
    for (const auto& t : moves) {
        summary += QStringLiteral("\n  tile %1: %2 -> %3 (%4)")
                       .arg(t.id)
                       .arg(QString::fromLatin1(vms::TierName(t.from)))
                       .arg(QString::fromLatin1(vms::TierName(t.to)))
                       .arg(t.release() ? QStringLiteral("release")
                                        : QStringLiteral("acquire"));
    }
    return summary;
}

void WorkspaceController::focusTile(int id) {
    const int n = static_cast<int>(requests_.size());
    if (id < 0 || id >= n) return;

    if (timer_.isActive()) {
        timer_.stop();   // operator took manual control of the working set
        emit sweepingChanged();
    }
    focusIndex_ = id;
    for (auto& r : requests_) r.focused = false;  // priority is left untouched
    requests_[id].focused = true;

    plan_ = session_.update(requests_);
    rebuildModel();
}

void WorkspaceController::setPriority(int id, int level) {
    const int n = static_cast<int>(requests_.size());
    if (id < 0 || id >= n) return;

    vms::Priority p;
    switch (level) {
        case 1:  p = vms::Priority::Low;    break;
        case 3:  p = vms::Priority::High;   break;
        default: p = vms::Priority::Medium; break;
    }
    if (requests_[id].priority == p) return;

    // Device priority is independent of focus, so the sweep keeps running: this
    // marks a camera important wherever it sits, not where the operator looks.
    requests_[id].priority = p;
    plan_ = session_.update(requests_);
    rebuildModel();
}

void WorkspaceController::setDesiredTier(int id, int level) {
    const int n = static_cast<int>(requests_.size());
    if (id < 0 || id >= n) return;
    if (level < 0) level = 0;
    if (level > 3) level = 3;
    const vms::Tier t = static_cast<vms::Tier>(level);
    if (requests_[id].desired == t) return;

    requests_[id].desired = t;
    plan_ = session_.update(requests_);
    rebuildModel();
}

void WorkspaceController::setTileCount(int count) {
    if (count < 1) count = 1;
    if (count == static_cast<int>(requests_.size())) return;

    requests_.clear();
    requests_.reserve(count);
    for (int i = 0; i < count; ++i) {
        vms::TileRequest r;
        r.id = i;
        r.desired = vms::Tier::Main;
        r.priority = vms::Priority::Medium;
        r.focused = (i == 0);
        r.visible = true;
        requests_.push_back(r);
    }
    focusIndex_ = 0;
    columns_ = std::max(1, static_cast<int>(std::ceil(std::sqrt(
                               static_cast<double>(count)))));
    rows_ = std::max(1, static_cast<int>(std::ceil(
                            static_cast<double>(count) / columns_)));

    session_.reset();   // a new working set: re-plan from scratch, no stale state
    plan_ = session_.update(requests_);
    rebuildModel(/*isLayoutChange=*/true);
}

void WorkspaceController::applyOptimizedProfile(const vms::CapacityProfile& p,
                                                const QString& reportText) {
    const vms::CapacityProfile& cur = session_.profile();
    const bool profileChanged =
        std::fabs(p.decodeBudget - cur.decodeBudget) > 1e-6 ||
        std::fabs(p.memoryBudgetMb - cur.memoryBudgetMb) > 1e-6 ||
        std::fabs(p.bandwidthBudgetKbps - cur.bandwidthBudgetKbps) > 1e-6;

    if (profileChanged) {
        // Re-plan under the new budget from scratch (a lowered budget must degrade
        // even though the requests are unchanged, which the stateful fast-path
        // would otherwise skip). The optimizer's own deadband keeps this rare.
        session_ = vms::GovernorSession(p);
        plan_ = session_.update(requests_);
    }

    if (reportText != optimizer_ || profileChanged) {
        optimizer_ = reportText;
        if (profileChanged) rebuildModel();   // refreshes tiles + capacity meter + emits
        else emit changed();                  // just the read-out text
    }
}

void WorkspaceController::startAutoSweep(int intervalMs) {
    sweepIntervalMs_ = intervalMs;
    if (intervalMs > 0) {
        timer_.start(intervalMs);
        emit sweepingChanged();
    }
}

void WorkspaceController::setAutoSweep(bool on) {
    if (on && sweepIntervalMs_ > 0) {
        if (!timer_.isActive()) {
            timer_.start(sweepIntervalMs_);
            emit sweepingChanged();
        }
    } else if (!on && timer_.isActive()) {
        timer_.stop();
        emit sweepingChanged();
    }
}

bool WorkspaceController::autoSweeping() const {
    return timer_.isActive();
}

int WorkspaceController::desiredTierOf(int id) const {
    if (id < 0 || id >= static_cast<int>(requests_.size())) return 3;
    return static_cast<int>(requests_[id].desired);
}

int WorkspaceController::priorityOf(int id) const {
    if (id < 0 || id >= static_cast<int>(requests_.size())) return 2;
    return static_cast<int>(requests_[id].priority);
}

std::vector<vms::Tier> WorkspaceController::currentTiers() const {
    const int n = static_cast<int>(requests_.size());
    std::vector<vms::Tier> tier(n, vms::Tier::Paused);
    for (const auto& d : plan_.tiles)
        if (d.id >= 0 && d.id < n) tier[d.id] = d.tier;
    return tier;
}

QString WorkspaceController::dumpPlan() const {
    const int n = static_cast<int>(requests_.size());
    std::vector<vms::Tier> tier(n, vms::Tier::Paused);
    std::vector<vms::TileState> state(n, vms::TileState::PausedOffscreen);
    for (const auto& d : plan_.tiles) {
        if (d.id >= 0 && d.id < n) {
            tier[d.id] = d.tier;
            state[d.id] = d.state;
        }
    }
    QString out;
    for (int i = 0; i < n; ++i) {
        out += QStringLiteral("  tile %1 [%2/%3]%4\n")
                   .arg(i)
                   .arg(QString::fromLatin1(vms::TierName(tier[i])))
                   .arg(QString::fromLatin1(vms::TileStateName(state[i])))
                   .arg(i == focusIndex_ ? QStringLiteral("  <-- focus")
                                         : QString());
    }
    return out;
}

// --- entry point ------------------------------------------------------------

namespace {

int countTier(const vms::GovernorResult& r, vms::Tier t) {
    int c = 0;
    for (const auto& d : r.tiles)
        if (d.tier == t) ++c;
    return c;
}

// Headless build/logic check: no window, no display, no camera. Confirms the
// governor produced a plan and that a large grid was actually degraded (not all
// tiles left at Main), then runs a few sweeps to exercise re-planning.
int runSelftest(const vms::CapacityProfile& profile, int count) {
    WorkspaceController controller(profile, count);

    std::cout << "vms_workspace --selftest\n"
              << "profile: " << profile.label << "\n"
              << "requested " << count << " main tiles\n"
              << controller.dumpPlan().toStdString();

    if (controller.tiles().isEmpty()) {
        std::cerr << "FAIL: no tiles in model\n";
        return 1;
    }

    std::cout << "sweeps:\n";
    for (int i = 0; i < 3; ++i)
        std::cout << "  " << controller.sweep().toStdString() << "\n";

    // Interactive focus: clicking a tile must focus it and protect it at Main.
    const int pick = (count > 5) ? 5 : count - 1;
    controller.focusTile(pick);
    std::cout << "focusTile(" << pick << "): focusIndex="
              << controller.focusIndex() << "\n";
    if (controller.focusIndex() != pick) {
        std::cerr << "FAIL: focusTile did not move focus\n";
        return 1;
    }
    const std::vector<vms::Tier> tiers = controller.currentTiers();
    if (pick >= static_cast<int>(tiers.size()) ||
        tiers[pick] != vms::Tier::Main) {
        std::cerr << "FAIL: focused tile was not protected at Main\n";
        return 1;
    }

    // Device priority: a High, non-focused camera must out-rank Medium peers
    // under pressure. Focus tile 0, mark a different tile High, and check it
    // holds a strictly better tier than a Medium peer. (Only meaningful when the
    // machine is actually under pressure — i.e. some tiles are degraded.)
    if (count >= 4) {
        controller.focusTile(0);
        const int hi = 1, mid = count - 1;
        controller.setPriority(hi, 3);   // High
        controller.setPriority(mid, 1);  // Low
        const std::vector<vms::Tier> t2 = controller.currentTiers();
        std::cout << "priority: tile " << hi << " (High)=" << vms::TierName(t2[hi])
                  << ", tile " << mid << " (Low)=" << vms::TierName(t2[mid]) << "\n";
        if (t2[mid] != vms::Tier::Main && t2[hi] < t2[mid]) {
            std::cerr << "FAIL: High-priority camera ranked below a Low one\n";
            return 1;
        }
    }

    // Desired-tier ceiling: capping a camera holds it at/below the cap; Off
    // stops it decoding. Honored regardless of pressure (the governor seeds at
    // desired), so this holds on any profile.
    if (count >= 4) {
        controller.setDesiredTier(2, 1);  // Thumb cap
        controller.setDesiredTier(3, 0);  // Off
        const std::vector<vms::Tier> t3 = controller.currentTiers();
        std::cout << "desired: tile 2 (cap Thumb)=" << vms::TierName(t3[2])
                  << ", tile 3 (Off)=" << vms::TierName(t3[3]) << "\n";
        if (t3[2] > vms::Tier::Thumb) {
            std::cerr << "FAIL: desired-tier cap not honored\n";
            return 1;
        }
        if (t3[3] != vms::Tier::Paused) {
            std::cerr << "FAIL: Off camera still decoding\n";
            return 1;
        }
    }

    // Layout change: switching the wall size rebuilds the working set.
    controller.setTileCount(4);
    std::cout << "setTileCount(4): tiles=" << controller.tiles().size()
              << " grid=" << controller.columns() << "x" << controller.rows()
              << "\n";
    if (controller.tiles().size() != 4 || controller.columns() != 2) {
        std::cerr << "FAIL: setTileCount did not rebuild the layout\n";
        return 1;
    }

    std::cout << "PASS: model populated, re-planned across sweeps, focused tile "
                 "protected at Main, priority ordering honored, and layout "
                 "rebuilt on resize\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    int count = 16;
    int sweepIntervalSec = 4;
    bool selftest = false;
    bool playbackSelftest = false;
    bool devicesSelftest = false;        // headless DeviceController check (inc 14)
    bool devicesDemo = false;            // seed demo devices for the Devices tab
    bool video = false;
    bool noPersist = false;
    std::string dbPathArg;
    std::string recDbArg = "rec.db";     // recording index for the Playback tab
    std::string recCamera = "cam-1";
    int smokeMs = 0;                     // >0: load, run this long offscreen, quit
    int optIntervalSec = 2;              // optimizer health-sample cadence (inc 8)
    bool noOptimize = false;             // disable the live optimizer sampler
    int healthIntervalSec = 15;          // device-health live-feed cadence (inc 19)
    bool noHealthFeed = false;           // disable the live device-health probe
    double optFakeRamMb = -1.0;          // >=0: inject this free-RAM (test the loop)
    double optFakeBwKbps = -1.0;         // >=0: inject a link budget
#ifdef VMS_WITH_GSTREAMER
    std::string profileName = "auto";   // seed from the live hardware probe
#else
    std::string profileName = "devbox"; // no registry to probe without GStreamer
#endif

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--selftest") {
            selftest = true;
        } else if (a == "--playback-selftest") {
            playbackSelftest = true;
        } else if (a == "--devices-selftest") {
            devicesSelftest = true;
        } else if (a == "--devices-demo") {
            devicesDemo = true;
        } else if (a == "--video") {
            video = true;
        } else if (a == "--no-persist") {
            noPersist = true;
        } else if (a == "--db" && i + 1 < argc) {
            dbPathArg = argv[++i];
        } else if (a == "--rec-db" && i + 1 < argc) {
            recDbArg = argv[++i];
        } else if (a == "--rec-camera" && i + 1 < argc) {
            recCamera = argv[++i];
        } else if (a == "--smoke-ms" && i + 1 < argc) {
            smokeMs = std::atoi(argv[++i]);
        } else if (a == "--opt-interval" && i + 1 < argc) {
            optIntervalSec = std::atoi(argv[++i]);
        } else if (a == "--no-optimize") {
            noOptimize = true;
        } else if (a == "--opt-fake-ram" && i + 1 < argc) {
            optFakeRamMb = std::atof(argv[++i]);
        } else if (a == "--opt-fake-bw" && i + 1 < argc) {
            optFakeBwKbps = std::atof(argv[++i]);
        } else if (a == "--health-interval" && i + 1 < argc) {
            healthIntervalSec = std::atoi(argv[++i]);
        } else if (a == "--no-health-feed") {
            noHealthFeed = true;
        } else if (a == "--count" && i + 1 < argc) {
            count = std::atoi(argv[++i]);
        } else if (a == "--sweep-interval" && i + 1 < argc) {
            sweepIntervalSec = std::atoi(argv[++i]);
        } else if (a == "--profile" && i + 1 < argc) {
            profileName = argv[++i];
        } else if (a == "--help" || a == "-h") {
            std::cout <<
                "vms_workspace — P3-14 honest per-tile state, rendered\n"
                "  --count N            requested main tiles (default 16)\n"
                "  --profile auto|devbox|lowend   capacity profile (default auto:\n"
                "                       seed from the live hardware probe)\n"
                "  --sweep-interval S   seconds between focus re-plans (default 4)\n"
                "  --video              render live video under the state chrome\n"
                "                       (slice 2; needs the GStreamer build)\n"
                "  --selftest           headless plan check, no window\n"
                "  --devices-selftest   headless onboarding/health check (inc 14)\n"
                "  --devices-demo       seed demo devices in the Devices tab\n";
            return 0;
        }
    }

#ifdef VMS_WITH_PERSIST
    if (playbackSelftest) return runPlaybackSelftest();
    if (devicesSelftest) return runDevicesSelftest();
#else
    if (playbackSelftest) {
        std::cerr << "--playback-selftest needs the persistence build.\n";
        return 2;
    }
    if (devicesSelftest) {
        std::cerr << "--devices-selftest needs the persistence build.\n";
        return 2;
    }
#endif

#ifndef VMS_WITH_GSTREAMER
    if (video) {
        std::cerr << "--video needs the GStreamer build; this binary was built "
                     "without it.\n";
        return 2;
    }
#endif

#ifdef VMS_WITH_GSTREAMER
    // Initialize GStreamer up front so --profile auto can query the decoder
    // registry and --video's pipeline is ready. Consumes any --gst-* args.
    gst_init(&argc, &argv);
#endif

    if (count < 1) count = 1;

    vms::CapacityProfile profile;
    if (profileName == "lowend") {
        profile = vms::LowEndProfile();
    } else if (profileName == "devbox") {
        profile = vms::DevBoxProfile();
    } else if (profileName == "auto") {
#ifdef VMS_WITH_GSTREAMER
        // Seed the governor from THIS machine (slice 2b-3) — the product intent:
        // only decode what the hardware can actually sustain.
        profile = AutoCapacityProfile("h265");
        std::cout << "profile: " << profile.label << "\n";
#else
        std::cerr << "--profile auto needs the GStreamer build; "
                     "use devbox or lowend.\n";
        return 2;
#endif
    } else {
        std::cerr << "unknown --profile '" << profileName
                  << "' (expected auto, devbox, or lowend)\n";
        return 2;
    }

    if (selftest)
        return runSelftest(profile, count);

    QGuiApplication app(argc, argv);

    qmlRegisterType<VideoItem>("Vms", 1, 0, "VideoItem");

    // Two independently stateful instances of one workspace (P3-14): Live and
    // Playback. Each has its own governor session, focus, priority, tiers, and
    // layout. QML binds `governor` to whichever tab is active; the video pipeline
    // always follows the Live instance (Playback has no recorded footage yet —
    // that source is the Phase-4 recording backend).
    WorkspaceController liveController(profile, count);
    WorkspaceController playbackController(profile, count);

#ifdef VMS_WITH_PERSIST
    // P0-04 inc 5c: open the standalone store, migrate to the canonical schema,
    // and restore each instance's saved layout BEFORE the QML/video are built so
    // they come up exactly as the operator left them. Persistence failures are
    // non-fatal — the app runs, just without remembering state.
    vms::persist::Store store;
    bool persisting = false;
    std::string dbPath;   // hoisted: the device SecretStore backing file sits by it
    if (!noPersist) {
        dbPath = dbPathArg;
        if (dbPath.empty()) {
            const QString dir = QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation);
            QDir().mkpath(dir);
            dbPath = (dir + "/workspace.sqlite").toStdString();
        }
        vms::persist::Error e = store.open(dbPath);
        if (e) e = store.migrate(vms::persist::coreMigrations());
        if (!e) {
            std::cerr << "persist: " << vms::persist::StatusName(e.status) << ": "
                      << e.message << " — continuing without persistence"
                      << std::endl;
            store.close();
        } else {
            persisting = true;
            restoreInstance(store, "live", liveController);
            restoreInstance(store, "playback", playbackController);
            std::cout << "persist: " << dbPath << " (schema v"
                      << store.schemaVersion() << ")" << std::endl;
        }
    }
#endif

    // increment 14 (onboarding UI): the Devices tab binds to a DeviceController
    // over the SAME standalone store — the device inventory (DeviceRepo, atomic
    // onboard / channel sync / remove) with the credential secret in the OS
    // secret store (DPAPI), plus honest per-device health (HealthMonitor). Only
    // available when persistence opened; the QML guards every use with a null
    // check. The health feed is Unknown-until-observed — real observations from
    // the broker/ONVIF are the live-blocked remainder; --devices-demo seeds a few
    // devices with scripted health so the surface is verifiable without a camera.
    QObject* devicesCtrl = nullptr;   // exposed to QML (null without persistence)
#ifdef VMS_WITH_PERSIST
    std::unique_ptr<vms::persist::SecretStore> deviceSecrets;
    std::unique_ptr<vms::persist::DeviceRepo> deviceRepo;
    vms::health::HealthMonitor deviceHealth;
    DeviceController* devices = nullptr;
    if (persisting) {
#ifdef _WIN32
        auto dpapi = std::make_unique<vms::persist::DpapiSecretStore>(
            dbPath + ".secrets");
        dpapi->open();   // a missing backing file is not an error (first run)
        deviceSecrets = std::move(dpapi);
#else
        deviceSecrets = std::make_unique<vms::persist::InMemorySecretStore>();
#endif
        deviceRepo =
            std::make_unique<vms::persist::DeviceRepo>(store, *deviceSecrets);
        // inc 16: wire the ONVIF discovery source. --devices-demo uses a
        // fixture-backed fake so the Discover panel is populated without a LAN;
        // a real run uses the live UDP+HTTP source (null when the transports
        // were not compiled in, so the panel honestly reports unavailable).
        vms::onvif::DiscoverySource* discSource = nullptr;
#ifdef VMS_WITH_ONVIF
        if (devicesDemo) {
            static FakeDiscoverySource fakeSource;
            discSource = &fakeSource;
        } else {
            static std::unique_ptr<vms::onvif::DiscoverySource> liveSource =
                vms::onvif::MakeLiveDiscoverySource();
            discSource = liveSource.get();
        }
#endif
        // inc 19: the device-health LIVE FEED. A real run wires the live TCP
        // reachability probe; --devices-demo leaves it off so the scripted demo
        // health stands (a live probe would just find the fake IPs offline).
        static std::unique_ptr<vms::health::DeviceHealthProbe> healthProbe;
        vms::health::DeviceHealthProbe* probeSource = nullptr;
        if (!devicesDemo && !noHealthFeed) {
            healthProbe = vms::health::MakeTcpHealthProbe(800);
            probeSource = healthProbe.get();
        }
        devices = new DeviceController(deviceRepo.get(), &deviceHealth,
                                       discSource, probeSource, &app);
        devicesCtrl = devices;

        // Poll device reachability now and on a timer so the Devices tab health
        // tracks the world without an operator action (a no-op with no probe).
        if (probeSource && healthIntervalSec > 0) {
            devices->pollHealth();
            auto* healthTimer = new QTimer(&app);
            QObject::connect(healthTimer, &QTimer::timeout, devices,
                             [devices]() { devices->pollHealth(); });
            healthTimer->start(healthIntervalSec * 1000);
        }

        if (devicesDemo) {
            struct Demo { const char* id; const char* name; const char* addr;
                          const char* vendor; int reach; int stream; bool maint; };
            const Demo demos[] = {
                {"cam-front", "Front Door", "192.168.0.11", "Hikvision", 1, 2, false},
                {"cam-park",  "Parking Lot", "192.168.0.12", "Axis",      1, 1, false},
                {"cam-bay",   "Loading Bay", "192.168.0.13", "Dahua",     0, 0, false},
                {"cam-lobby", "Lobby",       "192.168.0.14", "Hanwha",    1, 2, true},
                {"cam-roof",  "Rooftop",     "192.168.0.15", "Bosch",    -1, -1, false},
            };
            for (const Demo& d : demos) {
                const QString id = QString::fromLatin1(d.id);
                devices->onboard(id, QString::fromLatin1(d.name),
                                 QString::fromLatin1(d.addr),
                                 QString::fromLatin1(d.vendor),
                                 QStringLiteral("admin"), QStringLiteral("demo"),
                                 QStringLiteral("rtsp://") +
                                     QString::fromLatin1(d.addr) +
                                     QStringLiteral(":554/Streaming/Channels/101"),
                                 QString());
                if (d.reach >= 0) devices->reportReach(id, d.reach);
                if (d.stream >= 0) devices->reportStream(id, d.stream);
                if (d.maint) devices->setMaintenance(id, true);
            }
            // A multi-channel recorder (inc 15): 8 channels expanded from the
            // URL templates, reported online with an ok stream.
            devices->onboardRecorder(
                QStringLiteral("nvr-lobby"), QStringLiteral("Lobby NVR"),
                QStringLiteral("192.168.0.20"), QStringLiteral("Hikvision"),
                QStringLiteral("nvr"), QStringLiteral("admin"), QStringLiteral("demo"),
                8, QStringLiteral("rtsp://192.168.0.20:554/Streaming/Channels/{ch}01"),
                QStringLiteral("rtsp://192.168.0.20:554/Streaming/Channels/{ch}02"));
            devices->reportReach(QStringLiteral("nvr-lobby"), 1);
            devices->reportStream(QStringLiteral("nvr-lobby"), 2);
            // inc 16: run one discovery pass so the Discover panel is populated
            // (the fixture source returns 2 candidates — one collides with the
            // seeded Front Door at .0.11 and shows "onboarded", one is new).
            devices->startDiscovery(100);
            std::cout << "devices: seeded " << devices->deviceCount()
                      << " demo device(s), " << devices->attentionCount()
                      << " need attention; discovery found "
                      << devices->discoveredDevices().size() << " candidate(s)"
                      << std::endl;
        }
    }
#endif

    // inc 7c: the Playback tab's timeline + transport bind to a PlaybackController
    // over the recording SegmentIndex (written by vms_record). A separate store
    // for recordings; the window defaults to the recorded footage extent so the
    // timeline opens on real footage (or shows an honest empty range when none).
    PlaybackController* playback = nullptr;
#ifdef VMS_WITH_PERSIST
    vms::persist::Store recStore;
    vms::persist::SegmentIndex* recIndex = nullptr;
    if (recStore.open(recDbArg) && recStore.migrate(vms::persist::coreMigrations())) {
        recIndex = new vms::persist::SegmentIndex(recStore);
        playback = new PlaybackController(recIndex, QString::fromStdString(recCamera), &app);
        vms::persist::Result r;
        if (recStore.query(
                "SELECT MIN(start_utc), MAX(end_utc) FROM segments WHERE camera_id=?;",
                {recCamera}, r) &&
            !r.rows.empty() && std::holds_alternative<std::string>(r.rows[0][0])) {
            playback->setRange(
                QString::fromStdString(std::get<std::string>(r.rows[0][0])),
                QString::fromStdString(std::get<std::string>(r.rows[0][1])));
        }
        std::cout << "playback: " << recDbArg << " camera '" << recCamera << "' ("
                  << playback->spans().size() << " span(s))" << std::endl;
    }
#endif

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("liveCtrl"),
                                              &liveController);
    engine.rootContext()->setContextProperty(QStringLiteral("playbackCtrl"),
                                              &playbackController);
    // `playback` is the recording-backed controller (or null on a build without
    // persistence); the QML guards every use with `playback &&`.
    engine.rootContext()->setContextProperty(QStringLiteral("playback"), playback);
    // `devicesCtrl` is the DeviceController (or null on a build without
    // persistence); the Devices-tab QML guards its use behind a null check.
    engine.rootContext()->setContextProperty(QStringLiteral("devicesCtrl"),
                                              devicesCtrl);
    engine.rootContext()->setContextProperty(QStringLiteral("videoActive"),
                                              video);
    engine.load(QUrl(QStringLiteral("qrc:/Workspace.qml")));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "Failed to load qrc:/Workspace.qml\n";
        return 1;
    }

    bool videoRunning = false;
#ifdef VMS_WITH_GSTREAMER
    GridPipeline* grid = nullptr;
    QTimer busTimer;
    if (video) {
        VideoItem* item =
            engine.rootObjects().first()->findChild<VideoItem*>(
                QStringLiteral("videoOut"));
        if (!item) {
            std::cerr << "video: could not find the VideoItem in the scene\n";
            return 1;
        }

        // Slice 2b: the governed d3d11-composited grid IS the video source, built
        // from the same plan the chrome shows, so tile-for-tile they agree.
        // `sweepable` pre-encodes every tier's clip so a focus sweep (2b-2) can
        // move any tile to any tier.
        grid = new GridPipeline(item, liveController.columns(),
                                liveController.rows(),
                                liveController.currentTiers(), "h265");
        std::string err;
        if (!grid->start(/*sweepable=*/true, err)) {
            std::cerr << "video: " << err << "\n";
            return 1;
        }
        std::cout << "video: governed grid PLAYING (slice 2b) -- "
                  << grid->summary() << std::endl;
        videoRunning = true;

        // `grid` is rebuilt on a layout change, so every handler captures it by
        // reference and uses whatever pipeline is current.
        QObject::connect(&busTimer, &QTimer::timeout, [&grid]() {
            std::string e;
            if (grid && !grid->pumpBus(e))
                std::cerr << "video: pipeline error: " << e << "\n";
        });
        busTimer.start(100);

        // Decoder selection is async (decodebin negotiates after PLAYING), so
        // report the resolved per-tile decoder once branches have prerolled.
        QTimer::singleShot(2000, [&grid]() {
            if (grid) std::cout << "video: " << grid->summary() << std::endl;
        });

        // The video follows the LIVE instance only (Playback has no footage yet).
        // A same-size re-plan (sweep, click, priority, or tier change) re-tiers the
        // existing branches, keeping picture and chrome in lock-step.
        QObject::connect(&liveController, &WorkspaceController::planChanged,
                         [&liveController, &grid]() {
            std::string e;
            if (grid && !grid->applyPlan(liveController.currentTiers(), e))
                std::cerr << "video: re-plan error: " << e << "\n";
        });

        // A layout change (different tile count) rebuilds the whole grid pipeline
        // for the new geometry; encoded clips are cached so this is fast.
        QObject::connect(&liveController, &WorkspaceController::layoutChanged,
                         [&liveController, &grid, item]() {
            if (grid) { grid->stop(); delete grid; }
            grid = new GridPipeline(item, liveController.columns(),
                                    liveController.rows(),
                                    liveController.currentTiers(), "h265");
            std::string e;
            if (!grid->start(/*sweepable=*/true, e))
                std::cerr << "video: layout rebuild failed: " << e << "\n";
            else
                std::cout << "video: layout -> " << liveController.tiles().size()
                          << " tiles (" << liveController.columns() << "x"
                          << liveController.rows() << ")\n";
        });

        QObject::connect(&app, &QGuiApplication::aboutToQuit, [&grid]() {
            if (grid) grid->stop();
        });
    }
#endif // VMS_WITH_GSTREAMER

#if defined(VMS_WITH_GSTREAMER) && defined(VMS_WITH_PERSIST)
    // inc 7c-3: decode the recorded segment at the playhead into the Playback
    // pane. The PlaybackController owns the timeline/scrub/transport intent; this
    // wiring turns that intent into real decoded video and pushes the decoded
    // position back so the scrubber follows the picture and rolls onto the next
    // segment file at a boundary.
    PlaybackPipeline* pbPipe = nullptr;
    QTimer pbPoll;
    long long pbSegStartAbs = 0;   // abs start of the file currently open
    bool pbInternal = false;       // guard: playhead moves we caused ourselves
    if (playback) {
        VideoItem* pbItem = engine.rootObjects().first()->findChild<VideoItem*>(
            QStringLiteral("playbackVideoOut"));
        if (pbItem) {
            pbPipe = new PlaybackPipeline(pbItem);

            auto syncToPlayhead = [&]() {
                const QVariantMap seg = playback->segmentAtPlayhead();
                const QString path = seg.value(QStringLiteral("path")).toString();
                const double off = seg.value(QStringLiteral("offsetSec")).toDouble();
                if (path.isEmpty()) { if (pbPipe) pbPipe->pause(); return; }  // gap
                std::string e;
                if (pbPipe && !pbPipe->openFile(path.toStdString(), off, e))
                    std::cerr << "playback video: " << e << std::endl;
                pbSegStartAbs = playback->playheadAbs() - static_cast<long long>(off);
            };

            // Operator scrub/step/skip: (re)point the pipeline at the new time.
            QObject::connect(playback, &PlaybackController::playheadChanged,
                             [&]() { if (!pbInternal) syncToPlayhead(); });
            // Play/pause + speed.
            QObject::connect(playback, &PlaybackController::transportChanged, [&]() {
                if (!pbPipe) return;
                pbPipe->setRate(playback->speed());
                if (playback->playing()) { syncToPlayhead(); pbPipe->play(); }
                else pbPipe->pause();
            });
            // Advance the timeline playhead from the decoded position, and roll
            // onto the next segment file when this one ends.
            QObject::connect(&pbPoll, &QTimer::timeout, [&]() {
                std::string e;
                if (pbPipe && !pbPipe->pumpBus(e))
                    std::cerr << "playback video: " << e << std::endl;
                if (!pbPipe || !playback->playing()) return;
                const double pos = pbPipe->positionSec();
                if (pos < 0) return;
                pbInternal = true;
                playback->setPlayheadAbs(pbSegStartAbs + static_cast<long long>(pos));
                pbInternal = false;
                const QVariantMap seg = playback->segmentAtPlayhead();
                if (seg.value(QStringLiteral("path")).toString().toStdString() !=
                    pbPipe->currentFile())
                    syncToPlayhead();   // crossed into a new segment (or a gap)
            });
            pbPoll.start(150);
        }
    }
#endif // VMS_WITH_GSTREAMER && VMS_WITH_PERSIST

#ifdef VMS_WITH_OPTIMIZE
    // inc 8: the live optimization sampler. Every optIntervalSec, read the
    // machine's headroom and adjust the Live wall's capacity so it degrades before
    // exhaustion and recovers when headroom clears — the "watch the bottleneck"
    // loop. --opt-fake-ram injects a scripted free-RAM figure so the loop is
    // verifiable under pressure without actually starving the box.
    vms::optimize::OptimizerSession optimizer;
    std::unique_ptr<vms::optimize::SystemHealthProbe> healthProbe;
    if (optFakeRamMb >= 0.0) {
        auto fake = std::make_unique<vms::optimize::FakeSystemHealthProbe>();
        vms::optimize::SystemSample s;
        s.valid = true;
        s.totalRamMb = std::max(optFakeRamMb, 16000.0);
        s.availRamMb = optFakeRamMb;
        s.logicalCores = 8;
        s.systemCpuLoadPct = 40.0;
        s.estBandwidthKbps = optFakeBwKbps;
        fake->set(s);
        healthProbe = std::move(fake);
    } else {
#ifdef _WIN32
        healthProbe = std::make_unique<vms::optimize::WindowsSystemHealthProbe>();
#endif
    }

    QTimer optTimer;
    if (healthProbe && !noOptimize) {
        auto tick = [&]() {
            const vms::optimize::SystemSample s = healthProbe->sample();
            vms::optimize::OptimizationReport rep;
            const vms::CapacityProfile eff =
                optimizer.adjust(liveController.baseProfile(), s, rep);
            QString text = QStringLiteral("optimizer: mem %1/%2 MB (%3)")
                               .arg(static_cast<long>(rep.memoryBudgetMb))
                               .arg(static_cast<long>(rep.baseMemoryBudgetMb))
                               .arg(QString::fromStdString(rep.clampedBy));
            if (rep.systemCpuLoadPct >= 0.0)
                text += QStringLiteral(" · cpu %1%").arg(static_cast<int>(rep.systemCpuLoadPct));
            if (eff.bandwidthBudgetKbps > 0.0)
                text += QStringLiteral(" · link %1 kbps").arg(static_cast<long>(eff.bandwidthBudgetKbps));
            liveController.applyOptimizedProfile(eff, text);
        };
        QObject::connect(&optTimer, &QTimer::timeout, &liveController, tick);
        optTimer.start(optIntervalSec * 1000);
        tick();   // apply once immediately so the read-out is populated at startup
    }
#endif // VMS_WITH_OPTIMIZE

#ifdef VMS_WITH_PERSIST
    // Save both instances' layouts on exit so the next launch restores them.
    if (persisting) {
        QObject::connect(&app, &QGuiApplication::aboutToQuit,
                         [&store, &liveController, &playbackController]() {
            saveInstance(store, "live", liveController);
            saveInstance(store, "playback", playbackController);
        });
    }
#endif

    // The Live instance runs the automatic focus sweep; in --video the planChanged
    // connection above makes the picture follow it. A tile click stops the timer
    // (the operator has taken over the working set). Playback is not swept — it is
    // paused footage conceptually (and, for now, awaiting the recording backend).
    liveController.startAutoSweep(sweepIntervalSec * 1000);

    // Headless smoke check: load the whole scene (so every QML binding evaluates
    // and any error/warning surfaces), run briefly, then quit. Used with
    // QT_QPA_PLATFORM=offscreen to build-verify the QML without a display.
    if (smokeMs > 0) {
        std::cout << "smoke: running " << smokeMs << "ms then quitting" << std::endl;
        QTimer::singleShot(smokeMs, &app, &QGuiApplication::quit);
#if defined(VMS_WITH_GSTREAMER) && defined(VMS_WITH_PERSIST)
        // Exercise the Playback decode path offscreen: seek to the start of the
        // recorded footage and play, so the pipeline decodes real frames we can
        // count (proves 7c-3 without a display).
        if (pbPipe && playback && !playback->spans().isEmpty()) {
            playback->seekFrac(0.0);
            playback->play();
            QObject::connect(&app, &QGuiApplication::aboutToQuit, [&pbPipe]() {
                std::cout << "smoke: playback frames pulled = "
                          << (pbPipe ? pbPipe->framesPulled() : 0) << std::endl;
            });
        }
#endif
#ifdef VMS_WITH_OPTIMIZE
        QObject::connect(&app, &QGuiApplication::aboutToQuit, [&liveController]() {
            std::cout << "smoke: " << liveController.optimizer().toStdString() << "\n"
                      << "smoke: live " << liveController.capacity().toStdString()
                      << std::endl;
        });
#endif
    }

    const int rc = app.exec();
#ifdef VMS_WITH_GSTREAMER
    delete grid;
#endif
#if defined(VMS_WITH_GSTREAMER) && defined(VMS_WITH_PERSIST)
    delete pbPipe;
#endif
    return rc;
}
