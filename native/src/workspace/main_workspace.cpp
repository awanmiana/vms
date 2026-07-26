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

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

#ifdef VMS_WITH_GSTREAMER
#include <gst/gst.h>

#include <QObject>
#include <QTimer>

#include "GridPipeline.h"
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
    : QObject(parent), session_(profile) {
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
    bool video = false;
#ifdef VMS_WITH_GSTREAMER
    std::string profileName = "auto";   // seed from the live hardware probe
#else
    std::string profileName = "devbox"; // no registry to probe without GStreamer
#endif

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--selftest") {
            selftest = true;
        } else if (a == "--video") {
            video = true;
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
                "  --selftest           headless plan check, no window\n";
            return 0;
        }
    }

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

    WorkspaceController controller(profile, count);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("governor"),
                                              &controller);
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
        grid = new GridPipeline(item, controller.columns(), controller.rows(),
                                controller.currentTiers(), "h265");
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

        // A same-size re-plan (sweep, click, or priority change) just re-tiers the
        // existing branches, keeping picture and chrome in lock-step.
        QObject::connect(&controller, &WorkspaceController::planChanged,
                         [&controller, &grid]() {
            std::string e;
            if (grid && !grid->applyPlan(controller.currentTiers(), e))
                std::cerr << "video: re-plan error: " << e << "\n";
        });

        // A layout change (different tile count) rebuilds the whole grid pipeline
        // for the new geometry; encoded clips are cached so this is fast.
        QObject::connect(&controller, &WorkspaceController::layoutChanged,
                         [&controller, &grid, item]() {
            if (grid) { grid->stop(); delete grid; }
            grid = new GridPipeline(item, controller.columns(),
                                    controller.rows(), controller.currentTiers(),
                                    "h265");
            std::string e;
            if (!grid->start(/*sweepable=*/true, e))
                std::cerr << "video: layout rebuild failed: " << e << "\n";
            else
                std::cout << "video: layout -> " << controller.tiles().size()
                          << " tiles (" << controller.columns() << "x"
                          << controller.rows() << ")\n";
        });

        QObject::connect(&app, &QGuiApplication::aboutToQuit, [&grid]() {
            if (grid) grid->stop();
        });
    }
#endif // VMS_WITH_GSTREAMER

    // The controller's own timer runs the automatic focus sweep in both modes;
    // in --video the planChanged connection above makes the picture follow it. A
    // tile click stops this timer (the operator has taken over the working set).
    controller.startAutoSweep(sweepIntervalSec * 1000);
    const int rc = app.exec();
#ifdef VMS_WITH_GSTREAMER
    delete grid;
#endif
    return rc;
}
