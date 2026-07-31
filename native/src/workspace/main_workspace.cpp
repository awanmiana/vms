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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
#include "InstantReplayController.h"
#include "DeviceController.h"
#include "SpatialPolicy.h"
#include "CommandController.h"
#include "AlarmController.h"
#include "command/CoverageCheck.h"
#ifdef VMS_WITH_API
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include "CommandServer.h"
#endif

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
            // Spatial position (v9): -1 = unset, keep the default placement.
            if (t.posX >= 0.0 && t.posY >= 0.0)
                c.setTilePos(t.id, t.posX, t.posY);
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
        st.tiles.push_back({i, c.desiredTierOf(i), c.priorityOf(i),
                            c.tilePosX(i), c.tilePosY(i)});
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

// P3-05 / inc 23: headless check that InstantReplayController turns a look-back
// on a recorded camera into an honest replay window over the SegmentIndex —
// clamping to available footage, never presenting requested time as recorded on
// an unrecorded camera, and returning cleanly to live. No window, no camera.
int runInstantSelftest() {
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
    auto add = [&](const char* cam, const char* s, const char* e, const char* p) {
        Segment seg;
        seg.cameraId = cam; seg.startUtc = s; seg.endUtc = e;
        seg.path = p; seg.codec = "h264"; seg.bytes = 1000;
        std::int64_t id = 0; idx.add(seg, id);
    };
    // cam-1: 90s of contiguous footage ending at 09:01:30 (the live edge).
    add("cam-1", "2026-07-29 09:00:00", "2026-07-29 09:00:30", "a.mp4");
    add("cam-1", "2026-07-29 09:00:30", "2026-07-29 09:01:00", "b.mp4");
    add("cam-1", "2026-07-29 09:01:00", "2026-07-29 09:01:30", "c.mp4");

    std::cout << "vms_workspace --instant-selftest\n";

    InstantReplayController ir(&idx, QStringLiteral("cam-1"));
    check(!ir.active(), "starts inactive (operator is on the live wall)");

    long long earliest = 0, edge = 0;
    check(ir.footageExtent(earliest, edge), "footage extent found for a recorded camera");
    check(edge == ParseUtcSeconds(QStringLiteral("2026-07-29 09:01:30")),
          "live edge is the latest recorded moment");

    // 1) Look back 30s from the live edge -> plays real footage.
    ir.replay(30);
    check(ir.active(), "replay(30) opens the overlay");
    check(ir.available(), "footage available for the last 30s");
    check(ir.lookbackSec() == 30, "look-back is 30s");
    check(ir.pb()->rangeEnd() == QLatin1String("2026-07-29 09:01:30"),
          "window ends at the live edge");
    check(ir.pb()->rangeStart() == QLatin1String("2026-07-29 09:01:00"),
          "window starts 30s before the edge");
    check(ir.pb()->onFootage(),
          "playhead opens ON recorded footage (never a frozen live frame)");
    check(ir.pb()->playing(), "replay auto-plays");
    check(!ir.pb()->segmentAtPlayhead().value(QStringLiteral("path"))
               .toString().isEmpty(),
          "a recorded file backs the playhead");

    // 2) Look back further than the footage -> clamps, honest actual look-back.
    ir.replay(600);
    check(ir.available(), "clamped replay still available");
    check(ir.pb()->rangeStart() == QLatin1String("2026-07-29 09:00:00"),
          "window clamps to the earliest recorded moment");
    check(ir.lookbackSec() == 90,
          "actual look-back reported honestly (90s, not the requested 600)");

    // 3) Return to live.
    ir.returnToLive();
    check(!ir.active(), "returnToLive() closes the overlay");
    check(!ir.pb()->playing(), "decode paused on return to live");

    // 4) A camera with NO local recording -> honest unavailable, no fake footage.
    InstantReplayController ir2(&idx, QStringLiteral("cam-none"));
    ir2.replay(30);
    check(ir2.active(), "replay on an unrecorded camera still opens the overlay");
    check(!ir2.available(), "unrecorded camera is honestly unavailable");
    check(!ir2.pb()->onFootage(), "no requested time is presented as recorded");
    check(ir2.pb()->spans().isEmpty(), "no availability spans fabricated");

    if (failures == 0) {
        std::cout << "PASS: instant-replay look-back, clamp-to-available, honest "
                     "no-footage, and return-to-live verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

// P3-15/P3-01 / inc 24: headless check of the spatial-canvas policy (the
// prototype-exact zones / zoom levels / tier caps / anti-flap dwell) and its
// governor integration (one batched re-plan; culled tiles honestly
// paused-offscreen; dragged positions persisted). No window, no camera.
int runSpatialSelftest() {
    using namespace vms::spatial;
    int failures = 0;
    auto check = [&](bool c, const std::string& what) {
        std::cout << (c ? "  ok  " : "  FAIL ") << what << "\n";
        if (!c) ++failures;
    };
    std::cout << "vms_workspace --spatial-selftest\n";

    // --- 1) The pure policy, prototype-exact -------------------------------
    check(ZoomLevelFor(0.3) == ZoomLevel::Site &&
              ZoomLevelFor(0.5) == ZoomLevel::Wing &&
              ZoomLevelFor(1.0) == ZoomLevel::Room,
          "zoom levels: <0.4 site, <0.9 wing, else room");

    // Zones on a 1280x800 view (minDim 800: focus <=192, peripheral <=440).
    const double hw = kTileW / 2.0, hh = kTileH / 2.0;
    check(ZoneFor(640, 400, 1280, 800, hw, hh) == Zone::Focus,
          "screen center is the focus zone");
    check(ZoneFor(640 + 300, 400, 1280, 800, hw, hh) == Zone::Peripheral,
          "mid-distance is peripheral");
    check(ZoneFor(1280 + hw + 100, 400, 1280, 800, hw, hh) == Zone::Prewarm,
          "just off-screen inside the 240px margin pre-warms");
    check(ZoneFor(1280 + hw + 500, 400, 1280, 800, hw, hh) == Zone::Culled,
          "beyond the margin is culled");

    check(ResolveTierByZone(Zone::Focus, ZoomLevel::Room, false) == 3,
          "focus zone at room zoom -> main");
    check(ResolveTierByZone(Zone::Peripheral, ZoomLevel::Room, false) == 1,
          "peripheral at room zoom -> thumb");
    check(ResolveTierByZone(Zone::Focus, ZoomLevel::Wing, false) == 1,
          "wing zoom caps the focus zone to thumb");
    check(ResolveTierByZone(Zone::Peripheral, ZoomLevel::Site, false) == 0,
          "site zoom caps everything to paused");
    check(ResolveTierByZone(Zone::Peripheral, ZoomLevel::Site, true) == 3,
          "a focused camera is always main (bypasses the cap, as the prototype)");
    check(ResolveTierByZone(Zone::Culled, ZoomLevel::Room, false) == 0 &&
              ResolveTierByZone(Zone::Prewarm, ZoomLevel::Room, false) == 1,
          "culled -> paused, prewarm -> thumb");

    check(!ShouldApplyTierChange(2, 2, kSettleMs), "no-op tier never applies");
    check(ShouldApplyTierChange(3, 1, 0),
          "a downgrade applies immediately (mid-pan)");
    check(!ShouldApplyTierChange(1, 3, 0),
          "a promotion is held while the viewport is unsettled");
    check(ShouldApplyTierChange(1, 3, kSettleMs),
          "a promotion applies once settled (300ms dwell)");

    check([] {
        double x = 0, y = 0;
        GridWorldPos(1, 16, x, y);
        return x == kTileW + kTileW / 2.0 && y == kTileH / 2.0;
    }(), "default world layout matches the prototype grid placement");

    // --- 2) The governor integration ---------------------------------------
    WorkspaceController c(vms::DevBoxProfile(), 16);
    int rePlans = 0;
    QObject::connect(&c, &WorkspaceController::planChanged,
                     [&rePlans]() { ++rePlans; });

    // Default positions flow into the model.
    const QVariantMap t1 = c.tiles()[1].toMap();
    check(t1.value(QStringLiteral("px")).toDouble() == kTileW + kTileW / 2.0,
          "tile model carries the default spatial position");

    // Drag repositions without a re-plan (presentation only).
    c.setTilePos(15, 99999.0, 99999.0);
    check(rePlans == 0, "a tile drag alone never re-plans the governor");
    check(c.tiles()[15].toMap().value(QStringLiteral("px")).toDouble() == 99999.0,
          "dragged position lands in the model");

    // One settled viewport pass: tile 15 (dragged far away) must be culled ->
    // honestly paused-offscreen (visible=false, truly not decoding); the whole
    // batch costs exactly ONE re-plan.
    c.updateSpatialViewport(1280, 800, 1.0, 0, 0, /*settled=*/true);
    check(rePlans == 1, "a viewport pass applies as ONE batched re-plan");
    {
        const QVariantMap t15 = c.tiles()[15].toMap();
        check(t15.value(QStringLiteral("state")).toString() ==
                  QLatin1String("paused-offscreen"),
              "a culled tile is honestly paused-offscreen (no decode)");
        check(c.tiles()[0].toMap().value(QStringLiteral("tier")).toString() ==
                  QLatin1String("MAIN"),
              "the focused tile holds MAIN in the spatial plan");
    }

    // An identical pass changes nothing -> no re-plan (no flicker at rest).
    c.updateSpatialViewport(1280, 800, 1.0, 0, 0, /*settled=*/true);
    check(rePlans == 1, "an unchanged viewport pass does not re-plan");

    // Promotion dwell through the controller: bring tile 15 back to center
    // while the viewport is UNSETTLED -> its promotion must wait; settling
    // applies it.
    c.setTilePos(15, 640.0, 400.0);
    c.updateSpatialViewport(1280, 800, 1.0, 0, 0, /*settled=*/false);
    check(c.tiles()[15].toMap().value(QStringLiteral("state")).toString() ==
              QLatin1String("paused-offscreen"),
          "promotion held while panning (anti-flap)");
    c.updateSpatialViewport(1280, 800, 1.0, 0, 0, /*settled=*/true);
    check(c.tiles()[15].toMap().value(QStringLiteral("tier")).toString() !=
              QLatin1String("PAUSED"),
          "promotion applies once the viewport settles");

    // Zooming out to site level pauses non-focused on-screen tiles at once.
    c.updateSpatialViewport(1280, 800, 0.2, 0, 0, /*settled=*/true);
    check(c.tiles()[5].toMap().value(QStringLiteral("tier")).toString() ==
              QLatin1String("PAUSED"),
          "site-level zoom pauses a non-focused tile (immediate downgrade)");
    check(c.tiles()[0].toMap().value(QStringLiteral("tier")).toString() ==
              QLatin1String("MAIN"),
          "the focused tile survives the site-level cap");

    check(c.zoomLevelName(0.2) == QLatin1String("site") &&
              c.zoomLevelName(1.0) == QLatin1String("room"),
          "zoomLevelName is single-sourced from the policy");

#ifdef VMS_WITH_PERSIST
    // --- 3) Positions persist (schema v9) ----------------------------------
    {
        using namespace vms::persist;
        Store store;
        check(static_cast<bool>(store.open(":memory:")), "open in-memory store");
        check(static_cast<bool>(store.migrate(coreMigrations())),
              "migrate schema");
        check(store.schemaVersion() == 10, "schema at v10 (audit log)");

        WorkspaceController a(vms::DevBoxProfile(), 4);
        a.setTilePos(2, 777.0, 888.0);
        saveInstance(store, "live", a);

        WorkspaceController b(vms::DevBoxProfile(), 16);
        restoreInstance(store, "live", b);
        check(b.tileCount() == 4, "restore applies the saved tile count");
        check(b.tilePosX(2) == 777.0 && b.tilePosY(2) == 888.0,
              "a dragged spatial position survives save/restore");
    }
#endif

    if (failures == 0) {
        std::cout << "PASS: spatial policy (prototype-exact), batched governor "
                     "integration, honest culling, anti-flap, and persisted "
                     "positions verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

// P1-12/A0 / inc 25: headless check that the command envelope drives the REAL
// workspace — text lines through CommandController::run() move the live
// working set, refusals are typed and audited, dangerous commands demand
// confirm, and the catalog is machine-readable. (The pure-registry contract is
// vms_cmdtest; this verifies the wiring.) No window, no camera.
int runCommandSelftest() {
    int failures = 0;
    auto check = [&](bool c, const std::string& what) {
        std::cout << (c ? "  ok  " : "  FAIL ") << what << "\n";
        if (!c) ++failures;
    };
    std::cout << "vms_workspace --command-selftest\n";

    WorkspaceController live(vms::DevBoxProfile(), 16);
    CommandController cmd(&live, /*instant=*/nullptr, /*devices=*/nullptr);

    // Catalog + palette hints.
    const QString cat = cmd.catalogJson();
    check(cat.contains(QLatin1String("\"id\":\"workspace.focus\"")) &&
              cat.contains(QLatin1String("\"id\":\"device.remove\"")) &&
              cat.contains(QLatin1String("\"dangerous\":true")),
          "catalog lists the native verbs and marks the dangerous one");
    check(!cmd.commandHints().isEmpty(), "palette hints populated");

    // Text commands drive the real controller.
    QString r = cmd.run(QStringLiteral("focus 5"));
    check(r.startsWith(QLatin1String("ok")) && live.focusIndex() == 5,
          "'focus 5' moves the real working set");
    r = cmd.run(QStringLiteral("quality 3 thumb"));
    check(r.startsWith(QLatin1String("ok")) && live.desiredTierOf(3) == 1,
          "'quality 3 thumb' caps the real tier");
    r = cmd.run(QStringLiteral("priority 2 high"));
    check(r.startsWith(QLatin1String("ok")) && live.priorityOf(2) == 3,
          "'priority 2 high' sets the real priority");
    r = cmd.run(QStringLiteral("layout 9"));
    check(r.startsWith(QLatin1String("ok")) && live.tileCount() == 9,
          "'layout 9' resizes the real wall");
    live.startAutoSweep(60000);
    r = cmd.run(QStringLiteral("sweep off"));
    check(r.startsWith(QLatin1String("ok")) && !live.autoSweeping(),
          "'sweep off' stops the real auto sweep");

    // Typed refusals out of the same gate.
    check(cmd.run(QStringLiteral("focus abc"))
              .startsWith(QLatin1String("bad-param")),
          "non-integer arg -> bad-param");
    check(cmd.run(QStringLiteral("bogus 1"))
              .startsWith(QLatin1String("unknown-command")),
          "unknown verb -> unknown-command");
    check(cmd.run(QStringLiteral("focus 99"))
              .startsWith(QLatin1String("failed")),
          "a valid call the workspace cannot honor fails honestly");
    check(cmd.run(QStringLiteral("replay.start 30"))
              .startsWith(QLatin1String("failed")),
          "replay without a recording DB refuses honestly");

    // The dangerous gate through the text leg.
    check(cmd.run(QStringLiteral("device.remove cam-1"))
              .startsWith(QLatin1String("needs-confirm")),
          "'device.remove' without confirm -> needs-confirm");
    check(cmd.run(QStringLiteral("device.remove cam-1 confirm"))
              .startsWith(QLatin1String("failed")),
          "confirmed remove without a device store fails honestly (not silently)");

    // Capability membership is enforced (empty grant set -> denied).
    const vms::command::CommandResult denied = cmd.registry().invoke(
        "workspace.focus", {{"tile", std::int64_t{1}}}, {});
    check(denied.outcome == vms::command::Outcome::CapabilityDenied,
          "no capability -> capability-denied (permission-ready)");

    // The UI-state verb round-trips through a signal (QML owns the mode).
    bool spatialOn = false;
    QObject::connect(&cmd, &CommandController::spatialModeRequested,
                     [&spatialOn](bool on) { spatialOn = on; });
    r = cmd.run(QStringLiteral("spatial on"));
    check(r.startsWith(QLatin1String("ok")) && spatialOn,
          "'spatial on' requests the QML mode switch");

    // Audit: every attempt above landed in the session trail, refusals too.
    const QVariantList log = cmd.auditLog();
    check(log.size() >= 13, "every attempt audited (refusals included)");
    int refused = 0, okd = 0;
    for (const QVariant& v : log) {
        if (v.toMap().value(QStringLiteral("ok")).toBool()) ++okd;
        else ++refused;
    }
    check(okd >= 6 && refused >= 6,
          "audit trail carries both executed and refused outcomes");
    check(log.first().toMap().value(QStringLiteral("command")).toString() ==
              QLatin1String("workspace.spatial"),
          "audit trail is newest-first");

    if (failures == 0) {
        std::cout << "PASS: the command envelope drives the real workspace — "
                     "validated text leg, typed refusals, dangerous confirm, "
                     "capability gate, and full audit verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

// P1-06 / inc 28: headless check of the durable audit log — every envelope
// attempt lands one hash-chained row that survives a reopen, the chain
// verifies on an untouched log, and a direct-UPDATE tamper is DETECTED at the
// exact row. No window, no camera.
int runAuditSelftest() {
    using namespace vms::persist;
    int failures = 0;
    auto check = [&](bool c, const std::string& what) {
        std::cout << (c ? "  ok  " : "  FAIL ") << what << "\n";
        if (!c) ++failures;
    };
    std::cout << "vms_workspace --audit-selftest\n";

    const QString dbf = QDir::tempPath() + QStringLiteral("/vms_audit_st.sqlite");
    for (const QString& f : {dbf, dbf + QStringLiteral("-wal"),
                             dbf + QStringLiteral("-shm")})
        QDir().remove(f);

    {
        Store store;
        check(static_cast<bool>(store.open(dbf.toStdString())), "open store file");
        check(static_cast<bool>(store.migrate(coreMigrations())), "migrate schema");
        check(store.schemaVersion() == 10, "schema at v10 (audit_log)");
        AuditRepo repo(store);

        WorkspaceController live(vms::DevBoxProfile(), 4);
        CommandController cmd(&live, nullptr, nullptr, nullptr);
        cmd.setAuditStore(&repo);

        // One executed, one refused — both must land durably.
        check(cmd.run(QStringLiteral("focus 2"))
                  .startsWith(QLatin1String("ok")), "run an executed command");
        check(cmd.run(QStringLiteral("focus abc"))
                  .startsWith(QLatin1String("bad-param")), "run a refused command");
        std::int64_t n = 0;
        check(static_cast<bool>(repo.count(n)) && n == 2,
              "both attempts landed as durable rows (refusal included)");

        std::vector<AuditEntry> rows;
        check(static_cast<bool>(repo.list(0, rows)) && rows.size() == 2 &&
                  rows[0].outcome == "bad-param" && rows[1].outcome == "ok",
              "rows list newest-first with honest outcomes");
        check(rows[0].source == "palette" && rows[1].source == "palette",
              "text-leg attempts attributed to the palette");
        check(rows[1].prevHash.empty() && !rows[1].rowHash.empty() &&
                  rows[0].prevHash == rows[1].rowHash,
              "rows hash-chain to their predecessors (genesis is empty)");

        // API attribution through the same sink.
        cmd.setSource(QStringLiteral("api"));
        cmd.registry().invoke("workspace.focus", {{"tile", std::int64_t{1}}},
                              cmd.capabilities());
        check(static_cast<bool>(repo.list(1, rows)) && rows.size() == 1 &&
                  rows[0].source == "api",
              "API attempts attributed to 'api'");

        std::int64_t broken = -1;
        check(static_cast<bool>(repo.verifyChain(Sha256HexHash(), broken)) &&
                  broken == 0,
              "the untouched chain verifies end-to-end");

        AuditEntry bad;
        check(repo.append(bad, nullptr).status == Status::Misuse,
              "append without a hash function is a typed misuse");
    }

    // Durability + tamper evidence across a reopen.
    {
        Store store;
        check(static_cast<bool>(store.open(dbf.toStdString())), "reopen store");
        AuditRepo repo(store);
        std::int64_t n = 0;
        check(static_cast<bool>(repo.count(n)) && n == 3,
              "audit rows survive the reopen (durable)");

        // Simulated tamper: a direct UPDATE behind the repo's back.
        check(static_cast<bool>(store.exec(
                  "UPDATE audit_log SET message='forged' WHERE id=1;", {})),
              "tamper with row 1 directly (bypassing the repo)");
        std::int64_t broken = 0;
        check(static_cast<bool>(repo.verifyChain(Sha256HexHash(), broken)) &&
                  broken == 1,
              "the tamper is DETECTED at exactly the forged row");

        // The log keeps working; the historical break stays visible.
        AuditEntry e;
        e.timeUtc = "2026-07-31 12:00:00";
        e.source = "ui";
        e.command = "workspace.sweep";
        e.args = "on=false";
        e.outcome = "ok";
        e.message = "post-tamper append";
        check(static_cast<bool>(repo.append(e, Sha256HexHash())) && e.id == 4,
              "appending after the tamper still works");
        check(static_cast<bool>(repo.verifyChain(Sha256HexHash(), broken)) &&
                  broken == 1,
              "the historical break remains evident after new appends");
    }

    for (const QString& f : {dbf, dbf + QStringLiteral("-wal"),
                             dbf + QStringLiteral("-shm")})
        QDir().remove(f);

    if (failures == 0) {
        std::cout << "PASS: durable hash-chained audit — persistence across "
                     "reopen, source attribution, refusals recorded, and "
                     "tamper detection verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

// P1-13 / inc 29: the command-coverage check. Scans the SHIPPED QML (read from
// the Qt resource, so it inspects exactly what ships) for direct controller
// calls and fails if any state-changing UI action bypasses the envelope.
// Returns the report so both --coverage-check and --coverage-selftest use one
// code path.
vms::command::CoverageReport computeCoverage(
    const std::vector<vms::command::MutatorSpec>& declared) {
    std::vector<std::pair<std::string, std::string>> files;
    for (const char* path : {":/Workspace.qml", ":/DevicesView.qml"}) {
        QFile f(QString::fromLatin1(path));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        files.emplace_back(std::string(path + 2),   // strip ":/"
                           f.readAll().toStdString());
    }
    return vms::command::CheckCoverage(files, declared);
}

int runCoverageCheck() {
    const vms::command::CoverageReport r =
        computeCoverage(vms::command::DeclaredMutators());
    std::cout << vms::command::FormatCoverageReport(r);
    if (r.filesScanned == 0) {
        std::cerr << "coverage: no QML found in the resource bundle\n";
        return 2;
    }
    return r.ok() ? 0 : 1;   // non-zero on a bypass: CI-gateable
}

// P1-13 / inc 29: prove the checker itself — the shipped UI is clean AND an
// injected bypass is detected (a check that cannot fail is not a check).
int runCoverageSelftest() {
    using namespace vms::command;
    int failures = 0;
    auto check = [&](bool c, const std::string& what) {
        std::cout << (c ? "  ok  " : "  FAIL ") << what << "\n";
        if (!c) ++failures;
    };
    std::cout << "vms_workspace --coverage-selftest\n";

    const std::vector<MutatorSpec> declared = DeclaredMutators();
    check(declared.size() >= 30, "a declared mutator surface exists");

    // 1) The shipped UI is clean.
    const CoverageReport shipped = computeCoverage(declared);
    check(shipped.filesScanned == 2, "both shipped QML files were scanned");
    if (!shipped.ok()) std::cout << FormatCoverageReport(shipped);
    check(shipped.ok(),
          "every state-changing UI action routes through the envelope");
    check(!shipped.notes.empty(),
          "declared exemptions are found and reported (no silent skips)");

    // 2) The checker DETECTS a bypass (negative test).
    const std::vector<std::pair<std::string, std::string>> bypass = {
        {"Injected.qml",
         "MouseArea {\n"
         "    onClicked: devicesCtrl.removeDevice(modelData.id)\n"
         "}\n"}};
    const CoverageReport bad = CheckCoverage(bypass, declared);
    check(!bad.ok() && bad.violations.size() == 1,
          "an injected direct-controller call is detected as a violation");
    if (bad.violations.size() == 1) {
        check(bad.violations[0].method == "removeDevice" &&
                  bad.violations[0].line == 2,
              "the violation names the method and the exact line");
    }

    // 3) Exempt calls are NOT violations, but ARE reported.
    const std::vector<std::pair<std::string, std::string>> exempt = {
        {"Exempt.qml",
         "Text { text: governor.zoomLevelName(spatialView.zoom) }\n"
         "onWheel: governor.updateSpatialViewport(w, h, z, x, y, false)\n"}};
    const CoverageReport ex = CheckCoverage(exempt, declared);
    check(ex.ok() && ex.notes.size() == 2,
          "pure reads and continuous-viewport calls are exempt, not violations");
    check(FormatCoverageReport(ex).find("continuous-viewport") != std::string::npos,
          "the report prints each exemption's category");

    // 4) A comment mentioning a call is not a call site.
    const std::vector<std::pair<std::string, std::string>> comment = {
        {"Comment.qml", "// calls devicesCtrl.removeDevice(id) historically\n"}};
    check(CheckCoverage(comment, declared).ok(),
          "a commented-out mention is not counted as a bypass");

    // 5) A longer identifier ending in a declared object does not match.
    const std::vector<std::pair<std::string, std::string>> lookalike = {
        {"Lookalike.qml", "myGovernor.focusTile(3)\n"}};
    check(CheckCoverage(lookalike, declared).ok(),
          "identifier-boundary respected (myGovernor != governor)");

    if (failures == 0) {
        std::cout << "PASS: the shipped UI is fully routed through the command "
                     "envelope, and the checker detects an injected bypass\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

// P1-06 / inc 28: print the most recent audit rows + the chain verdict (the
// operator/compliance read of the durable log).
int runAuditDump(int argc, char** argv, std::string dbPath, int limit) {
    QCoreApplication app(argc, argv);   // resolves the default AppData path
    using namespace vms::persist;
    if (dbPath.empty()) {
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        dbPath = (dir + QStringLiteral("/workspace.sqlite")).toStdString();
    }
    Store store;
    if (Error e = store.open(dbPath); !e) {
        std::cerr << "audit: cannot open " << dbPath << ": " << e.message << "\n";
        return 1;
    }
    if (Error e = store.migrate(coreMigrations()); !e) {
        std::cerr << "audit: migrate failed: " << e.message << "\n";
        return 1;
    }
    AuditRepo repo(store);
    std::int64_t total = 0, broken = 0;
    repo.count(total);
    repo.verifyChain(Sha256HexHash(), broken);
    std::vector<AuditEntry> rows;
    repo.list(limit, rows);

    std::cout << "audit log: " << dbPath << "\n"
              << "rows: " << total << "   chain: "
              << (broken == 0 ? "intact"
                              : ("BROKEN at row #" + std::to_string(broken)))
              << "\n";
    for (const AuditEntry& e : rows)
        std::cout << "  #" << e.id << "  " << e.timeUtc << "  [" << e.source
                  << "]  " << e.command << (e.args.empty() ? "" : " ")
                  << e.args << "  -> " << e.outcome
                  << (e.message.empty() ? "" : (": " + e.message)) << "\n";
    return broken == 0 ? 0 : 1;
}

#ifdef VMS_WITH_API
// P1-13/A0 / inc 26: headless check of the external control API — a real HTTP
// client drives the loopback server and every request flows through the SAME
// envelope gate as the palette (auth -> validation -> capability -> dangerous
// confirm -> audit), with the deterministic status mapping. No window.
int runApiSelftest(int argc, char** argv) {
    QCoreApplication app(argc, argv);   // the server + client need an event loop
    int failures = 0;
    auto check = [&](bool c, const std::string& what) {
        std::cout << (c ? "  ok  " : "  FAIL ") << what << "\n";
        if (!c) ++failures;
    };
    std::cout << "vms_workspace --api-selftest\n";

    WorkspaceController live(vms::DevBoxProfile(), 16);
    CommandController cmd(&live, nullptr, nullptr);
    CommandServer server(&cmd, QStringLiteral("test-token-123"));
    QString err;
    check(server.listen(0, err), "server binds a loopback ephemeral port");
    const quint16 port = server.port();
    check(port != 0, "ephemeral port assigned");

    QNetworkAccessManager nam;
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(port);
    // One synchronous round-trip: returns the HTTP status + parsed JSON.
    auto http = [&](const char* method, const QString& path,
                    const QString& token, const QByteArray& body,
                    int& status) -> QJsonDocument {
        QNetworkRequest req{QUrl(base + path)};
        if (!token.isEmpty())
            req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
        QNetworkReply* rep = (qstrcmp(method, "GET") == 0)
                                 ? nam.get(req) : nam.post(req, body);
        QEventLoop loop;
        QObject::connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonDocument doc = QJsonDocument::fromJson(rep->readAll());
        rep->deleteLater();
        return doc;
    };
    auto invoke = [&](const QJsonObject& body, int& status) {
        return http("POST", QStringLiteral("/v1/invoke"), QStringLiteral("test-token-123"),
                    QJsonDocument(body).toJson(QJsonDocument::Compact), status);
    };

    int st = 0;
    // 1) Authentication gate.
    http("GET", QStringLiteral("/v1/commands"), QString(), {}, st);
    check(st == 401, "no bearer token -> 401 (nothing served)");
    http("GET", QStringLiteral("/v1/commands"), QStringLiteral("wrong"), {}, st);
    check(st == 401, "wrong bearer token -> 401");

    // 2) Discovery: the machine-readable catalog.
    QJsonDocument cat = http("GET", QStringLiteral("/v1/commands"),
                             QStringLiteral("test-token-123"), {}, st);
    check(st == 200 && cat.isArray() && !cat.array().isEmpty(),
          "catalog served to an authenticated agent");
    bool hasFocus = false;
    for (const QJsonValue& v : cat.array())
        if (v.toObject().value(QStringLiteral("id")).toString() ==
            QLatin1String("workspace.focus"))
            hasFocus = true;
    check(hasFocus, "catalog carries the drivable command specs");

    // 3) A valid invocation moves the REAL workspace.
    QJsonObject body;
    body.insert(QStringLiteral("command"), QStringLiteral("workspace.focus"));
    body.insert(QStringLiteral("args"), QJsonObject{{QStringLiteral("tile"), 5}});
    QJsonDocument r = invoke(body, st);
    check(st == 200 && r.object().value(QStringLiteral("outcome")).toString() ==
              QLatin1String("ok"),
          "POST /v1/invoke workspace.focus -> 200 ok");
    check(live.focusIndex() == 5, "the API call moved the real working set");

    body = QJsonObject();
    body.insert(QStringLiteral("command"), QStringLiteral("workspace.quality"));
    body.insert(QStringLiteral("args"),
                QJsonObject{{QStringLiteral("tile"), 3},
                            {QStringLiteral("tier"), QStringLiteral("thumb")}});
    invoke(body, st);
    check(st == 200 && live.desiredTierOf(3) == 1,
          "typed JSON args bind (int + enum) and apply");

    // 4) Deterministic refusal mapping over the wire.
    body = QJsonObject{{QStringLiteral("command"), QStringLiteral("no.such")}};
    r = invoke(body, st);
    check(st == 404 && r.object().value(QStringLiteral("outcome")).toString() ==
              QLatin1String("unknown-command"),
          "unknown command -> 404 unknown-command");
    body = QJsonObject{{QStringLiteral("command"), QStringLiteral("workspace.focus")}};
    r = invoke(body, st);
    check(st == 400 && r.object().value(QStringLiteral("outcome")).toString() ==
              QLatin1String("missing-param"),
          "missing param -> 400 missing-param");
    body = QJsonObject();
    body.insert(QStringLiteral("command"), QStringLiteral("device.remove"));
    body.insert(QStringLiteral("args"),
                QJsonObject{{QStringLiteral("id"), QStringLiteral("cam-1")}});
    r = invoke(body, st);
    check(st == 409 && r.object().value(QStringLiteral("outcome")).toString() ==
              QLatin1String("needs-confirm"),
          "dangerous without confirm -> 409, nothing executed");
    body.insert(QStringLiteral("confirm"), true);
    r = invoke(body, st);
    check(st == 500 && r.object().value(QStringLiteral("outcome")).toString() ==
              QLatin1String("failed"),
          "confirmed remove without a device store -> honest 500 failed");

    // 5) Malformed transport input.
    QNetworkRequest bad{QUrl(base + QStringLiteral("/v1/invoke"))};
    bad.setRawHeader("Authorization", "Bearer test-token-123");
    QNetworkReply* rep = nam.post(bad, QByteArray("{not json"));
    {
        QEventLoop loop;
        QObject::connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }
    check(rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 400,
          "malformed JSON body -> 400");
    rep->deleteLater();
    http("GET", QStringLiteral("/nope"), QStringLiteral("test-token-123"), {}, st);
    check(st == 404, "unknown route -> honest JSON 404");

    // 6) The same audit trail as the palette, refusals included.
    int okd = 0, refused = 0;
    for (const QVariant& v : cmd.auditLog()) {
        if (v.toMap().value(QStringLiteral("ok")).toBool()) ++okd;
        else ++refused;
    }
    check(okd >= 2 && refused >= 3,
          "API attempts audited through the same trail (refusals included)");

    if (failures == 0) {
        std::cout << "PASS: loopback external control API verified — bearer "
                     "auth, catalog discovery, real invocation, deterministic "
                     "refusal mapping, dangerous confirm over the wire, and "
                     "shared audit\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
#endif // VMS_WITH_API

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
// P6-03/P6-07 / inc 27: headless check of the alarm surface end-to-end — a
// REAL device stack (repo + health monitor + fake probe + DeviceController)
// feeds health transitions into the AlarmEngine over the same signal wiring
// the app uses, and the lifecycle verbs run THROUGH the command envelope
// (alarm.ack / alarm.escalate / alarm.clear), audited. No window, no camera.
int runAlarmsSelftest() {
    using namespace vms::persist;
    int failures = 0;
    auto check = [&](bool c, const std::string& what) {
        std::cout << (c ? "  ok  " : "  FAIL ") << what << "\n";
        if (!c) ++failures;
    };
    std::cout << "vms_workspace --alarms-selftest\n";

    // The same mini device stack the devices selftest uses.
    Store store;
    check(static_cast<bool>(store.open(":memory:")), "open in-memory store");
    check(static_cast<bool>(store.migrate(coreMigrations())), "migrate schema");
    InMemorySecretStore secrets;
    DeviceRepo repo(store, secrets);
    vms::health::HealthMonitor health;
    FakeHealthProbe fakeProbe;
#ifdef VMS_WITH_ONVIF
    FakeDiscoverySource fakeDisc;
    DeviceController devices(&repo, &health, &fakeDisc, &fakeProbe);
#else
    DeviceController devices(&repo, &health, nullptr, &fakeProbe);
#endif
    check(devices.onboard(QStringLiteral("cam-1"), QStringLiteral("Front Door"),
                          QStringLiteral("192.168.0.11"), QStringLiteral("Acme"),
                          QStringLiteral("admin"), QStringLiteral("pw"),
                          QStringLiteral("rtsp://192.168.0.11:554/1"),
                          QString()).isEmpty(),
          "onboard a device");

    AlarmController alarms;
    QObject::connect(&devices, &DeviceController::healthTransition,
                     &alarms, &AlarmController::onHealthEvent);
    check(alarms.alarms().isEmpty() && alarms.needsAttention() == 0,
          "no alarms before any event");

    // The device goes unreachable: the live feed's transition must raise the
    // default High alarm through the real signal path.
    fakeProbe.reachable["cam-1"] = false;
    devices.pollHealth();
    check(alarms.alarms().size() == 1, "offline transition raises one alarm");
    const QVariantMap a0 = alarms.alarms().isEmpty()
                               ? QVariantMap() : alarms.alarms()[0].toMap();
    check(a0.value(QStringLiteral("priority")).toString() ==
                  QLatin1String("high") &&
              a0.value(QStringLiteral("state")).toString() ==
                  QLatin1String("new") &&
              a0.value(QStringLiteral("device")).toString() ==
                  QLatin1String("cam-1"),
          "alarm is High · New · for the offline device");
    check(alarms.needsAttention() == 1, "alarm demands attention");

    // Steady state does not spam: polling again (still offline) changes nothing
    // (the transition already happened; HealthMonitor state is unchanged).
    devices.pollHealth();
    check(alarms.alarms().size() == 1 &&
              alarms.alarms()[0].toMap().value(QStringLiteral("count")) == 1,
          "steady offline state raises nothing new (transitions only)");

    // Lifecycle through the ENVELOPE: the panel, palette, and API share this.
    WorkspaceController live(vms::DevBoxProfile(), 4);
    CommandController cmd(&live, nullptr, &devices, &alarms);
    const int alarmId =
        static_cast<int>(a0.value(QStringLiteral("id")).toDouble());
    QString r = cmd.run(QStringLiteral("alarm.ack %1").arg(alarmId));
    check(r.startsWith(QLatin1String("ok")) &&
              alarms.alarms()[0].toMap().value(QStringLiteral("state"))
                      .toString() == QLatin1String("acknowledged"),
          "'alarm.ack' through the envelope acknowledges (audited)");
    check(alarms.needsAttention() == 0, "acknowledged no longer demands attention");
    r = cmd.run(QStringLiteral("alarm.escalate %1").arg(alarmId));
    check(r.startsWith(QLatin1String("ok")) && alarms.needsAttention() == 1,
          "'alarm.escalate' re-raises attention");
    check(cmd.run(QStringLiteral("alarm.ack 9999"))
              .startsWith(QLatin1String("failed")),
          "an unknown alarm id fails honestly through the envelope");

    // Recovery auto-clears via the same live feed.
    fakeProbe.reachable["cam-1"] = true;
    devices.pollHealth();
    check(alarms.alarms().isEmpty(),
          "device recovery auto-clears the alarm (active list empty)");
    check(alarms.needsAttention() == 0, "nothing demands attention after recovery");

    // A fresh outage is a FRESH alarm, and maintenance suppresses notification.
    alarms.setDeviceMaintenance(QStringLiteral("cam-1"), true);
    fakeProbe.reachable["cam-1"] = false;
    devices.pollHealth();
    check(alarms.alarms().size() == 1 &&
              static_cast<int>(alarms.alarms()[0].toMap()
                                   .value(QStringLiteral("id")).toDouble()) !=
                  alarmId,
          "a new outage after recovery raises a fresh alarm (new id)");
    check(alarms.needsAttention() == 0 &&
              alarms.alarms()[0].toMap().value(QStringLiteral("suppressed"))
                  .toBool(),
          "maintenance suppresses notification, never the alarm itself");

    // The alarm verbs are in the machine-readable catalog (palette + API).
    check(cmd.catalogJson().contains(QLatin1String("\"id\":\"alarm.ack\"")),
          "alarm verbs discoverable in the command catalog");

    if (failures == 0) {
        std::cout << "PASS: health transitions raise deduplicated alarms end-"
                     "to-end, the lifecycle runs through the audited command "
                     "envelope, recovery auto-clears, and maintenance "
                     "suppresses honestly\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

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
    check(store.schemaVersion() == 10, "schema at v10 (audit log, inc 28)");
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

    // inc 20: device rename (P2-06) preserves every association.
    const int camsBefore =
        ctrl.devices().first().toMap().value(QStringLiteral("cameraCount")).toInt();
    QString ren = ctrl.renameDevice(QStringLiteral("nvr-1"),
                                    QStringLiteral("Main Lobby Recorder"));
    check(ren.isEmpty(), "renameDevice succeeds");
    check(ctrl.devices().first().toMap().value(QStringLiteral("name")).toString() ==
              QLatin1String("Main Lobby Recorder"),
          "device name updated");
    check(ctrl.devices().first().toMap().value(QStringLiteral("cameraCount")).toInt()
              == camsBefore,
          "rename preserves the channels (associations intact)");
    check(groupCount() == camsBefore,
          "rename preserves the default group membership");
    {
        Result gn;
        store.query("SELECT name FROM camera_groups WHERE id=?;",
                    {DeviceRepo::defaultGroupId("nvr-1")}, gn);
        check(!gn.rows.empty() &&
                  std::get<std::string>(gn.rows[0][0]) == "Main Lobby Recorder (default)",
              "rename refreshes the default group's display name");
    }
    check(!ctrl.renameDevice(QStringLiteral("no-such-device"),
                             QStringLiteral("X")).isEmpty(),
          "renaming an unknown device is an honest error");

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

    // inc 22: rescan a discovered camera re-fetches its ONVIF streams and
    // reconciles the channel, PRESERVING the operator-customized name.
    ctrl.renameChannel(QStringLiteral("onvif-192-168-0-77"),
                       QStringLiteral("onvif-192-168-0-77"),
                       QStringLiteral("Operator Name"));
    const QString rescanErr = ctrl.rescanDevice(
        QStringLiteral("onvif-192-168-0-77"),
        QStringLiteral("admin"), QStringLiteral("onvifpw"));
    check(rescanErr.isEmpty(), "rescanDevice re-fetches + reconciles the camera");
    {
        QVariantList chs;
        for (const QVariant& v : ctrl.devices()) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("id")).toString() ==
                QLatin1String("onvif-192-168-0-77"))
                chs = m.value(QStringLiteral("channels")).toList();
        }
        check(chs.size() == 1, "rescan keeps the single channel");
        check(chs.first().toMap().value(QStringLiteral("name")).toString() ==
                  QLatin1String("Operator Name"),
              "rescan preserves the operator-customized channel name");
        check(chs.first().toMap().value(QStringLiteral("mainUrl")).toString() ==
                  QLatin1String("rtsp://192.168.0.77:554/Streaming/Channels/101"),
              "rescan refreshed the channel's main stream URL from the device");
    }
    check(!ctrl.rescanDevice(QStringLiteral("nvr-1"), QStringLiteral("a"),
                             QStringLiteral("b")).isEmpty(),
          "rescan honestly refuses a recorder (not a direct camera)");
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

    // inc 21: device attach/detach (P2-06). A detached device keeps its config
    // (channels preserved) but empties its default group and is skipped by the
    // health poll.
    auto deviceMap = [&](const QString& id) -> QVariantMap {
        for (const QVariant& v : ctrl.devices()) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("id")).toString() == id) return m;
        }
        return QVariantMap{};
    };
    const int nvrCamsBefore =
        deviceMap(QStringLiteral("nvr-1")).value(QStringLiteral("cameraCount")).toInt();
    check(groupCount() == nvrCamsBefore, "nvr-1 group holds its channels while attached");

    ctrl.setDeviceDisabled(QStringLiteral("nvr-1"), true);
    check(deviceMap(QStringLiteral("nvr-1")).value(QStringLiteral("disabled")).toBool(),
          "setDeviceDisabled detaches the device");
    check(groupCount() == 0, "a detached device's default group is emptied");
    check(deviceMap(QStringLiteral("nvr-1")).value(QStringLiteral("cameraCount")).toInt()
              == nvrCamsBefore,
          "detach preserves the channels (config intact)");

    // The poll must skip a detached device: mark it unreachable and confirm a
    // poll does NOT flip it Offline.
    const QString nvrStateBefore = stateOf(QStringLiteral("nvr-1"));
    fakeProbe.reachable[std::string("nvr-1")] = false;
    ctrl.pollHealth();
    check(stateOf(QStringLiteral("nvr-1")) == nvrStateBefore,
          "the health poll skips a detached device (health unchanged)");

    ctrl.setDeviceDisabled(QStringLiteral("nvr-1"), false);
    check(groupCount() == nvrCamsBefore,
          "re-attaching restores the default group to its enabled channels");

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

    // Spatial default: the prototype's grid placement (inc 24). Operator drags
    // and the persisted layout (v9) override these per tile.
    posX_.resize(count);
    posY_.resize(count);
    for (int i = 0; i < count; ++i)
        vms::spatial::GridWorldPos(i, count, posX_[i], posY_[i]);

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
        // Spatial-canvas world position (inc 24) — presentation only.
        m.insert(QStringLiteral("px"), i < static_cast<int>(posX_.size())
                                           ? posX_[i] : 0.0);
        m.insert(QStringLiteral("py"), i < static_cast<int>(posY_.size())
                                           ? posY_[i] : 0.0);
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

    // A new working set gets fresh default spatial positions (dragged spots
    // belong to the previous layout's tiles).
    posX_.resize(count);
    posY_.resize(count);
    for (int i = 0; i < count; ++i)
        vms::spatial::GridWorldPos(i, count, posX_[i], posY_[i]);

    session_.reset();   // a new working set: re-plan from scratch, no stale state
    plan_ = session_.update(requests_);
    rebuildModel(/*isLayoutChange=*/true);
}

// --- Spatial canvas (inc 24, P3-15/P3-01) ------------------------------------

void WorkspaceController::setTilePos(int id, double x, double y) {
    if (id < 0 || id >= static_cast<int>(posX_.size())) return;
    if (posX_[id] == x && posY_[id] == y) return;
    posX_[id] = x;
    posY_[id] = y;
    // Presentation-only: patch the model in place (no re-plan, no planChanged —
    // the video pipeline must not be poked by a drag).
    if (id < tiles_.size()) {
        QVariantMap m = tiles_[id].toMap();
        m.insert(QStringLiteral("px"), x);
        m.insert(QStringLiteral("py"), y);
        tiles_[id] = m;
    }
    emit changed();
}

double WorkspaceController::tilePosX(int id) const {
    return (id >= 0 && id < static_cast<int>(posX_.size())) ? posX_[id] : -1.0;
}
double WorkspaceController::tilePosY(int id) const {
    return (id >= 0 && id < static_cast<int>(posY_.size())) ? posY_[id] : -1.0;
}

QString WorkspaceController::zoomLevelName(double zoom) const {
    return QString::fromLatin1(
        vms::spatial::ZoomLevelName(vms::spatial::ZoomLevelFor(zoom)));
}

void WorkspaceController::updateSpatialViewport(double viewW, double viewH,
                                                double zoom, double offX,
                                                double offY, bool settled) {
    using namespace vms::spatial;
    const int n = static_cast<int>(requests_.size());
    if (n == 0 || viewW <= 0 || viewH <= 0 || zoom <= 0) return;

    const ZoomLevel level = ZoomLevelFor(zoom);
    const double halfW = (kTileW / 2.0) * zoom;
    const double halfH = (kTileH / 2.0) * zoom;
    // The QML settle timer owns the dwell clock: settled=true means the
    // viewport has been still for kSettleMs, so promotions may apply.
    const int msSinceSettled = settled ? kSettleMs : 0;

    bool anyChange = false;
    for (int i = 0; i < n; ++i) {
        const double sx = posX_[i] * zoom + offX;
        const double sy = posY_[i] * zoom + offY;
        const Zone zone = ZoneFor(sx, sy, viewW, viewH, halfW, halfH);
        const int proposed =
            ResolveTierByZone(zone, level, requests_[i].focused);

        // Current effective level: culled/paused count as 0.
        const int current =
            (!requests_[i].visible || requests_[i].desired == vms::Tier::Paused)
                ? 0
                : static_cast<int>(requests_[i].desired);
        if (!ShouldApplyTierChange(current, proposed, msSinceSettled)) continue;

        if (proposed == 0) {
            // Culled: honestly off-screen (visible=false -> PausedOffscreen, no
            // decode at all). A zoom-capped on-screen pause (site level) keeps
            // visible=true with desired=Paused so its state reads as intended.
            if (zone == Zone::Culled) {
                requests_[i].visible = false;
            } else {
                requests_[i].visible = true;
                requests_[i].desired = vms::Tier::Paused;
            }
        } else {
            requests_[i].visible = true;
            requests_[i].desired = static_cast<vms::Tier>(proposed);
        }
        anyChange = true;
    }

    if (!anyChange) return;   // nothing crossed a threshold: no re-plan
    plan_ = session_.update(requests_);
    rebuildModel();
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
    bool instantSelftest = false;        // headless InstantReplayController check (inc 23)
    bool spatialSelftest = false;        // headless spatial-canvas check (inc 24)
    bool spatial = false;                // start the Live tab in spatial mode
    bool commandSelftest = false;        // headless command-envelope check (inc 25)
    bool printCommands = false;          // print the machine-readable catalog
    bool apiSelftest = false;            // headless control-API check (inc 26)
    bool alarmsSelftest = false;         // headless alarm-surface check (inc 27)
    bool auditSelftest = false;          // headless durable-audit check (inc 28)
    bool coverageCheck = false;          // P1-13 UI-bypass gate (inc 29)
    bool coverageSelftest = false;
    bool auditDump = false;              // print the durable audit log
    int auditDumpLimit = 20;
    int apiPort = -1;                    // >=0: serve the loopback control API
    std::string apiToken;                // override the generated bearer token
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
        } else if (a == "--instant-selftest") {
            instantSelftest = true;
        } else if (a == "--spatial-selftest") {
            spatialSelftest = true;
        } else if (a == "--spatial") {
            spatial = true;
        } else if (a == "--command-selftest") {
            commandSelftest = true;
        } else if (a == "--commands") {
            printCommands = true;
        } else if (a == "--api-selftest") {
            apiSelftest = true;
        } else if (a == "--alarms-selftest") {
            alarmsSelftest = true;
        } else if (a == "--audit-selftest") {
            auditSelftest = true;
        } else if (a == "--coverage-check") {
            coverageCheck = true;
        } else if (a == "--coverage-selftest") {
            coverageSelftest = true;
        } else if (a == "--audit-dump") {
            auditDump = true;
            if (i + 1 < argc && std::atoi(argv[i + 1]) > 0)
                auditDumpLimit = std::atoi(argv[++i]);
        } else if (a == "--api-port" && i + 1 < argc) {
            apiPort = std::atoi(argv[++i]);
        } else if (a == "--api-token" && i + 1 < argc) {
            apiToken = argv[++i];
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
                "  --instant-selftest   headless instant-replay check (inc 23)\n"
                "  --spatial            start the Live tab on the spatial canvas\n"
                "  --spatial-selftest   headless spatial-canvas check (inc 24)\n"
                "  --commands           print the machine-readable command catalog\n"
                "  --command-selftest   headless command-envelope check (inc 25)\n"
                "  --api-port N         serve the loopback control API (inc 26;\n"
                "                       prints the bearer token; off by default)\n"
                "  --api-selftest       headless control-API check (inc 26)\n"
                "  --alarms-selftest    headless alarm-surface check (inc 27)\n"
                "  --audit-dump [N]     print the durable audit log + chain verdict\n"
                "  --audit-selftest     headless durable-audit check (inc 28)\n"
                "  --coverage-check     fail if any UI action bypasses the\n"
                "                       command envelope (P1-13; non-zero exit)\n"
                "  --coverage-selftest  headless coverage-checker check (inc 29)\n"
                "  --devices-selftest   headless onboarding/health check (inc 14)\n"
                "  --devices-demo       seed demo devices in the Devices tab\n";
            return 0;
        }
    }

#ifdef VMS_WITH_API
    if (apiSelftest) return runApiSelftest(argc, argv);
#else
    if (apiSelftest || apiPort >= 0) {
        std::cerr << "the control API needs the Qt HttpServer build.\n";
        return 2;
    }
#endif
#ifdef VMS_WITH_PERSIST
    if (playbackSelftest) return runPlaybackSelftest();
    if (instantSelftest) return runInstantSelftest();
    if (spatialSelftest) return runSpatialSelftest();
    if (commandSelftest) return runCommandSelftest();
    if (alarmsSelftest) return runAlarmsSelftest();
    if (auditSelftest) return runAuditSelftest();
    if (coverageCheck) return runCoverageCheck();
    if (coverageSelftest) return runCoverageSelftest();
    if (auditDump) return runAuditDump(argc, argv, dbPathArg, auditDumpLimit);
    if (printCommands) {
        // The machine-readable command catalog (P1-13's discovery seed).
        WorkspaceController live(vms::DevBoxProfile(), 16);
        CommandController cmd(&live, nullptr, nullptr);
        std::cout << cmd.catalogJson().toStdString() << std::endl;
        return 0;
    }
    if (devicesSelftest) return runDevicesSelftest();
#else
    if (playbackSelftest) {
        std::cerr << "--playback-selftest needs the persistence build.\n";
        return 2;
    }
    if (instantSelftest) {
        std::cerr << "--instant-selftest needs the persistence build.\n";
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

    // P3-05 / inc 23: instant playback from live. The Live overlay's controller
    // shares the same recording index/camera as the Playback tab but keeps its own
    // (independent) look-back timeline, so an instant replay never disturbs the
    // Playback tab's range. Null on a build/run without a recording DB.
    InstantReplayController* instant = nullptr;
#ifdef VMS_WITH_PERSIST
    if (recIndex)
        instant = new InstantReplayController(
            recIndex, QString::fromStdString(recCamera), &app);
#endif

    // inc 25 (P1-12/A0): the command envelope. Every operator verb is one
    // validated command over the live controllers; the palette (and later the
    // external agent API, P1-13) invokes through this single gate. Controllers
    // it lacks (no recording DB / no device store) refuse honestly.
    // inc 27 (P6-03/P6-07): the alarm surface. Device health transitions are
    // the first event source; the engine notifies, never actuates (P6-05 is
    // its own gate).
    auto* alarmsCtrl = new AlarmController(&app);
#ifdef VMS_WITH_PERSIST
    if (devices) {
        QObject::connect(devices, &DeviceController::healthTransition,
                         alarmsCtrl, &AlarmController::onHealthEvent);
    }
#endif

    CommandController* commander = nullptr;
#ifdef VMS_WITH_PERSIST
    commander = new CommandController(&liveController, instant, devices,
                                      alarmsCtrl, &app);
    // inc 28 (P1-06): durable audit — from here on, every envelope attempt
    // (palette, API, panel; refusals included) also lands one hash-chained,
    // append-only row in the standalone store. Read it back: --audit-dump.
    if (commander && persisting) {
        auto* auditRepo = new vms::persist::AuditRepo(store);
        commander->setAuditStore(auditRepo);
        std::cout << "audit: durable hash-chained log in " << dbPath
                  << " (--audit-dump to read)" << std::endl;
    }
#endif

#ifdef VMS_WITH_API
    // inc 26 (P1-13/A0): the external control API — loopback-only, OFF unless
    // --api-port is given, bearer-authenticated with a per-session token. An
    // external agent drives the SAME envelope (validation, capability,
    // dangerous confirm, audit) the palette uses.
    if (commander && apiPort >= 0) {
        QString token = QString::fromStdString(apiToken);
        if (token.isEmpty()) {
            // A fresh random session token, printed once below (the console is
            // the deployer-controlled handoff channel for this first slice).
            token = QStringLiteral("%1%2")
                        .arg(QRandomGenerator::system()->generate64(), 16, 16,
                             QLatin1Char('0'))
                        .arg(QRandomGenerator::system()->generate64(), 16, 16,
                             QLatin1Char('0'));
        }
        auto* apiServer = new CommandServer(commander, token, &app);
        QString apiErr;
        if (!apiServer->listen(static_cast<quint16>(apiPort), apiErr)) {
            std::cerr << "api: failed to listen on 127.0.0.1:" << apiPort
                      << ": " << apiErr.toStdString() << std::endl;
            return 1;
        }
        std::cout << "api: http://127.0.0.1:" << apiServer->port()
                  << "  (GET /v1/commands · POST /v1/invoke · bearer token: "
                  << token.toStdString() << ")" << std::endl;
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
    // `instant` is the InstantReplayController for the Live look-back overlay (or
    // null without a recording DB); the Live QML guards its use with `instant &&`.
    engine.rootContext()->setContextProperty(QStringLiteral("instant"), instant);
    // `devicesCtrl` is the DeviceController (or null on a build without
    // persistence); the Devices-tab QML guards its use behind a null check.
    engine.rootContext()->setContextProperty(QStringLiteral("devicesCtrl"),
                                              devicesCtrl);
    engine.rootContext()->setContextProperty(QStringLiteral("videoActive"),
                                              video);
    // inc 24: start the Live tab on the spatial canvas (also lets the offscreen
    // smoke exercise the spatial bindings).
    engine.rootContext()->setContextProperty(QStringLiteral("spatialDefault"),
                                              spatial);
    // inc 25: the command envelope (null on a build without persistence); the
    // palette QML guards every use with `commander &&`.
    engine.rootContext()->setContextProperty(QStringLiteral("commander"),
                                              commander);
    // inc 27: the alarm surface (always present; empty until events arrive).
    engine.rootContext()->setContextProperty(QStringLiteral("alarmsCtrl"),
                                              alarmsCtrl);
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

    // P3-05 / inc 23: the instant-replay overlay decodes into its OWN VideoItem,
    // driven by its own PlaybackController (instant->pb()) — the same intent→video
    // wiring as the Playback tab above, but independent of it, so a look-back over
    // the Live wall never moves the Playback tab's playhead. Pauses when the
    // operator returns to live.
    PlaybackPipeline* irPipe = nullptr;
    QTimer irPoll;
    long long irSegStartAbs = 0;
    bool irInternal = false;
    // Hoisted to function scope (not the if-block) so the lambdas below, which
    // outlive this scope, capture a live reference — mirrors pbPipe above.
    PlaybackController* ipb = instant ? instant->pb() : nullptr;
    if (instant && ipb) {
        VideoItem* irItem = engine.rootObjects().first()->findChild<VideoItem*>(
            QStringLiteral("instantVideoOut"));
        if (irItem) {
            irPipe = new PlaybackPipeline(irItem);

            auto irSync = [&]() {
                const QVariantMap seg = ipb->segmentAtPlayhead();
                const QString path = seg.value(QStringLiteral("path")).toString();
                const double off = seg.value(QStringLiteral("offsetSec")).toDouble();
                if (path.isEmpty()) { if (irPipe) irPipe->pause(); return; }  // gap
                std::string e;
                if (irPipe && !irPipe->openFile(path.toStdString(), off, e))
                    std::cerr << "instant video: " << e << std::endl;
                irSegStartAbs = ipb->playheadAbs() - static_cast<long long>(off);
            };

            QObject::connect(ipb, &PlaybackController::playheadChanged,
                             [&]() { if (!irInternal) irSync(); });
            QObject::connect(ipb, &PlaybackController::transportChanged, [&]() {
                if (!irPipe) return;
                irPipe->setRate(ipb->speed());
                if (ipb->playing()) { irSync(); irPipe->play(); }
                else irPipe->pause();
            });
            QObject::connect(&irPoll, &QTimer::timeout, [&]() {
                std::string e;
                if (irPipe && !irPipe->pumpBus(e))
                    std::cerr << "instant video: " << e << std::endl;
                if (!irPipe || !ipb->playing()) return;
                const double pos = irPipe->positionSec();
                if (pos < 0) return;
                irInternal = true;
                ipb->setPlayheadAbs(irSegStartAbs + static_cast<long long>(pos));
                irInternal = false;
                const QVariantMap seg = ipb->segmentAtPlayhead();
                if (seg.value(QStringLiteral("path")).toString().toStdString() !=
                    irPipe->currentFile())
                    irSync();   // crossed into a new segment (or a gap)
            });
            // Returning to live pauses the decode (the overlay hides the video).
            QObject::connect(instant, &InstantReplayController::changed, [&]() {
                if (instant && !instant->active() && irPipe) irPipe->pause();
            });
            irPoll.start(150);
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
        // P3-05 / inc 23: trigger an instant replay so the overlay binds and the
        // instant decode pulls real frames (proves the look-back path offscreen).
        if (irPipe && instant) {
            instant->replay(30);
            QObject::connect(&app, &QGuiApplication::aboutToQuit, [&]() {
                std::cout << "smoke: instant available="
                          << (instant->available() ? "yes" : "no")
                          << " look-back=" << instant->lookbackSec() << "s"
                          << " frames pulled=" << (irPipe ? irPipe->framesPulled() : 0)
                          << std::endl;
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
    delete irPipe;
#endif
    return rc;
}
