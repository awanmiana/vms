#include "CommandController.h"

#include <QCryptographicHash>
#include <QDateTime>

#include <cmath>
#include <iostream>

#include "AlarmController.h"
#include "DeviceController.h"
#include "InstantReplayController.h"
#include "PlaybackController.h"
#include "PremisesController.h"
#include "WorkspaceController.h"

using vms::command::Args;
using vms::command::CommandResult;
using vms::command::CommandSpec;
using vms::command::Outcome;
using vms::command::OutcomeName;
using vms::command::ParamSpec;
using vms::command::ParamType;

namespace {

// Handlers receive validated args; these unwrap them tersely.
int argInt(const Args& a, const char* name) {
    return static_cast<int>(std::get<std::int64_t>(a.at(name)));
}
std::string argStr(const Args& a, const char* name) {
    return std::get<std::string>(a.at(name));
}
double argNumber(const Args& a, const char* name) {
    const vms::command::Value& v = a.at(name);
    return std::holds_alternative<double>(v)
               ? std::get<double>(v)
               : static_cast<double>(std::get<std::int64_t>(v));
}

CommandResult ok(const std::string& msg) {
    return CommandResult{Outcome::Ok, msg};
}
CommandResult failed(const std::string& msg) {
    return CommandResult{Outcome::Failed, msg};
}

// Map a DeviceController QString-error convention (empty = success).
CommandResult fromDeviceError(const QString& err, const std::string& did) {
    if (err.isEmpty()) return ok("device " + did + ": done");
    return failed(err.toStdString());
}

} // namespace

vms::persist::HashFn Sha256HexHash() {
    return [](const std::string& bytes) {
        return QCryptographicHash::hash(
                   QByteArray(bytes.data(), static_cast<int>(bytes.size())),
                   QCryptographicHash::Sha256)
            .toHex()
            .toStdString();
    };
}

CommandController::CommandController(WorkspaceController* live,
                                     InstantReplayController* instant,
                                     DeviceController* devices,
                                     AlarmController* alarms,
                                     PlaybackController* playback,
                                     QObject* parent)
    : QObject(parent), live_(live), instant_(instant), playback_(playback),
      devices_(devices), alarms_(alarms) {
    registerCommands();

    // P0-01A Administrator session: grant every capability the catalog names.
    for (const std::string& id : registry_.ids())
        if (const CommandSpec* s = registry_.find(id))
            if (!s->capability.empty()) capabilities_.insert(s->capability);

    // Session audit trail + stdout line for every attempt (durable: P1-06).
    registry_.setAuditSink([this](const vms::command::AuditRecord& r) {
        QVariantMap m;
        m.insert(QStringLiteral("time"),
                 QDateTime::currentDateTimeUtc().toString(
                     QStringLiteral("HH:mm:ss")));
        m.insert(QStringLiteral("command"), QString::fromStdString(r.commandId));
        m.insert(QStringLiteral("args"), QString::fromStdString(r.argsText));
        m.insert(QStringLiteral("outcome"),
                 QString::fromLatin1(OutcomeName(r.outcome)));
        m.insert(QStringLiteral("message"), QString::fromStdString(r.message));
        m.insert(QStringLiteral("ok"), r.outcome == Outcome::Ok);
        auditLog_.prepend(m);
        while (auditLog_.size() > 100) auditLog_.removeLast();
        std::cout << "audit: " << r.commandId
                  << (r.argsText.empty() ? "" : " ") << r.argsText << " -> "
                  << OutcomeName(r.outcome)
                  << (r.message.empty() ? "" : (": " + r.message)) << std::endl;

        // inc 28 (P1-06): the durable, hash-chained row — refusals included.
        if (auditRepo_) {
            vms::persist::AuditEntry e;
            e.timeUtc = QDateTime::currentDateTimeUtc()
                            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                            .toStdString();
            e.source = source_.toStdString();
            e.command = r.commandId;
            e.args = r.argsText;
            e.outcome = OutcomeName(r.outcome);
            e.message = r.message;
            if (vms::persist::Error err = auditRepo_->append(e, Sha256HexHash());
                !err)
                std::cerr << "audit: durable append failed: " << err.message
                          << std::endl;
        }
        emit auditChanged();
    });

    // Palette synopsis lines, in catalog order.
    for (const std::string& id : registry_.ids()) {
        const CommandSpec* s = registry_.find(id);
        QString line = QString::fromStdString(id);
        for (const ParamSpec& p : s->params) {
            if (p.type == ParamType::Enum) {
                QString dom;
                for (const std::string& o : p.oneOf) {
                    if (!dom.isEmpty()) dom += QLatin1Char('|');
                    dom += QString::fromStdString(o);
                }
                line += QStringLiteral(" <%1>").arg(dom);
            } else {
                line += QStringLiteral(" <%1>").arg(QString::fromStdString(p.name));
            }
        }
        if (s->dangerous) line += QStringLiteral(" confirm");
        hints_ << line;
    }
}

void CommandController::registerCommands() {
    // --- Workspace verbs (the two ARCHITECTURE.md control axes + layout) ----
    registry_.add(
        {"workspace.focus", "Focus a camera tile (protect it at Main)",
         "workspace.control", false,
         {{"tile", ParamType::Int, true, 0, 4095}}},
        [this](const Args& a) {
            const int t = argInt(a, "tile");
            if (!live_) return failed("no live workspace");
            if (t >= live_->tileCount()) return failed("no such tile");
            live_->focusTile(t);
            return ok("focused tile " + std::to_string(t));
        });
    registry_.add(
        {"workspace.quality", "Set a camera's desired media tier (quality ceiling)",
         "workspace.control", false,
         {{"tile", ParamType::Int, true, 0, 4095},
          {"tier", ParamType::Enum, true, 1, 0, {"main", "sub", "thumb", "off"}}}},
        [this](const Args& a) {
            const int t = argInt(a, "tile");
            if (!live_) return failed("no live workspace");
            if (t >= live_->tileCount()) return failed("no such tile");
            const std::string tier = argStr(a, "tier");
            const int lvl = tier == "main" ? 3 : tier == "sub" ? 2
                            : tier == "thumb" ? 1 : 0;
            live_->setDesiredTier(t, lvl);
            return ok("tile " + std::to_string(t) + " quality -> " + tier);
        });
    registry_.add(
        {"workspace.priority", "Set a camera's device-activity priority",
         "workspace.control", false,
         {{"tile", ParamType::Int, true, 0, 4095},
          {"level", ParamType::Enum, true, 1, 0, {"high", "medium", "low"}}}},
        [this](const Args& a) {
            const int t = argInt(a, "tile");
            if (!live_) return failed("no live workspace");
            if (t >= live_->tileCount()) return failed("no such tile");
            const std::string lvl = argStr(a, "level");
            live_->setPriority(t, lvl == "high" ? 3 : lvl == "low" ? 1 : 2);
            return ok("tile " + std::to_string(t) + " priority -> " + lvl);
        });
    registry_.add(
        {"workspace.layout", "Resize the wall to N tiles",
         "workspace.control", false,
         {{"count", ParamType::Int, true, 1, 64}}},
        [this](const Args& a) {
            if (!live_) return failed("no live workspace");
            const int n = argInt(a, "count");
            live_->setTileCount(n);
            return ok("layout -> " + std::to_string(n) + " tiles");
        });
    registry_.add(
        {"workspace.sweep", "Turn the automatic focus sweep on or off",
         "workspace.control", false,
         {{"on", ParamType::Bool, true}}},
        [this](const Args& a) {
            if (!live_) return failed("no live workspace");
            const bool on = std::get<bool>(a.at("on"));
            live_->setAutoSweep(on);
            return ok(std::string("auto sweep ") + (on ? "on" : "off"));
        });
    registry_.add(
        {"workspace.spatial", "Switch the Live wall between grid and spatial canvas",
         "workspace.control", false,
         {{"on", ParamType::Bool, true}}},
        [this](const Args& a) {
            const bool on = std::get<bool>(a.at("on"));
            // On the spatial canvas the viewport owns the working set, so the
            // auto sweep stops here rather than in QML (keeping the UI free of
            // direct controller calls — P1-13 coverage).
            if (on && live_) live_->setAutoSweep(false);
            emit spatialModeRequested(on);
            return ok(std::string("spatial canvas ") + (on ? "on" : "off"));
        });
    registry_.add(
        {"workspace.orientation", "Set camera facing and field of view on the floor",
         "workspace.control", false,
         {{"tile", ParamType::Int, true, 0, 4095},
          {"facing", ParamType::Number, true, 0.0, 359.999},
          {"fov", ParamType::Number, true, 10.0, 180.0}}},
        [this](const Args& a) {
            if (!live_) return failed("no live workspace");
            const int tile = argInt(a, "tile");
            if (tile >= live_->tileCount()) return failed("no such tile");
            live_->setTileOrientation(tile, argNumber(a, "facing"),
                                      argNumber(a, "fov"));
            return ok("tile " + std::to_string(tile) + " orientation set");
        });
    registry_.add(
        {"premises.site", "Configure and select the active site",
         "premises.manage", false,
         {{"id", ParamType::String, true}, {"name", ParamType::String, true},
          {"timezone", ParamType::String, true}}},
        [this](const Args& a) {
            if (!premises_) return failed("no premises store attached");
            const QString err = premises_->configureSite(
                QString::fromStdString(argStr(a, "id")),
                QString::fromStdString(argStr(a, "name")),
                QString::fromStdString(argStr(a, "timezone")));
            return err.isEmpty() ? ok("active site configured")
                                 : failed(err.toStdString());
        });
    registry_.add(
        {"premises.floor", "Configure and select a floor and optional plan URI",
         "premises.manage", false,
         {{"id", ParamType::String, true}, {"site", ParamType::String, true},
          {"name", ParamType::String, true}, {"plan", ParamType::String, true},
          {"width", ParamType::Number, true, 100.0, 100000.0},
          {"height", ParamType::Number, true, 100.0, 100000.0}}},
        [this](const Args& a) {
            if (!premises_) return failed("no premises store attached");
            QString plan = QString::fromStdString(argStr(a, "plan"));
            if (plan == QLatin1String("none")) plan.clear();
            const QString err = premises_->configureFloor(
                QString::fromStdString(argStr(a, "id")),
                QString::fromStdString(argStr(a, "site")),
                QString::fromStdString(argStr(a, "name")), plan,
                argNumber(a, "width"), argNumber(a, "height"));
            return err.isEmpty() ? ok("active floor configured")
                                 : failed(err.toStdString());
        });
    registry_.add(
        {"premises.hours.add", "Add a weekly operating-hours window",
         "premises.manage", false,
         {{"day", ParamType::Enum, true, 1, 0,
           {"mon", "tue", "wed", "thu", "fri", "sat", "sun"}},
          {"start", ParamType::String, true},
          {"end", ParamType::String, true}}},
        [this](const Args& a) {
            if (!premises_) return failed("no premises store attached");
            const QString err = premises_->addOperatingWindow(
                QString::fromStdString(argStr(a, "day")),
                QString::fromStdString(argStr(a, "start")),
                QString::fromStdString(argStr(a, "end")));
            return err.isEmpty() ? ok("weekly operating window added")
                                 : failed(err.toStdString());
        });
    registry_.add(
        {"premises.hours.clear", "Clear one weekly day (configured closed)",
         "premises.manage", false,
         {{"day", ParamType::Enum, true, 1, 0,
           {"mon", "tue", "wed", "thu", "fri", "sat", "sun"}}}},
        [this](const Args& a) {
            if (!premises_) return failed("no premises store attached");
            const QString err = premises_->clearOperatingDay(
                QString::fromStdString(argStr(a, "day")));
            return err.isEmpty() ? ok("weekly day configured closed")
                                 : failed(err.toStdString());
        });
    registry_.add(
        {"premises.hours.exception", "Set special hours for one local date",
         "premises.manage", false,
         {{"date", ParamType::String, true},
          {"start", ParamType::String, true},
          {"end", ParamType::String, true},
          {"label", ParamType::String, true}}},
        [this](const Args& a) {
            if (!premises_) return failed("no premises store attached");
            const QString err = premises_->setSpecialHours(
                QString::fromStdString(argStr(a, "date")),
                QString::fromStdString(argStr(a, "start")),
                QString::fromStdString(argStr(a, "end")),
                QString::fromStdString(argStr(a, "label")));
            return err.isEmpty() ? ok("date special hours configured")
                                 : failed(err.toStdString());
        });
    registry_.add(
        {"premises.hours.holiday", "Close one local date",
         "premises.manage", false,
         {{"date", ParamType::String, true},
          {"label", ParamType::String, true}}},
        [this](const Args& a) {
            if (!premises_) return failed("no premises store attached");
            const QString err = premises_->setHolidayClosed(
                QString::fromStdString(argStr(a, "date")),
                QString::fromStdString(argStr(a, "label")));
            return err.isEmpty() ? ok("date configured closed")
                                 : failed(err.toStdString());
        });
    registry_.add(
        {"premises.hours.exception.clear", "Remove a local-date exception",
         "premises.manage", false,
         {{"date", ParamType::String, true}}},
        [this](const Args& a) {
            if (!premises_) return failed("no premises store attached");
            const QString err = premises_->clearDateException(
                QString::fromStdString(argStr(a, "date")));
            return err.isEmpty() ? ok("date exception cleared")
                                 : failed(err.toStdString());
        });

    // --- Instant replay (P3-05) — honest refusal without a recording DB -----
    registry_.add(
        {"replay.start", "Instant replay: jump back N seconds on the recorded camera",
         "playback.review", false,
         {{"seconds", ParamType::Int, true, 1, 3600}}},
        [this](const Args& a) {
            if (!instant_)
                return failed("no recording DB attached (--rec-db)");
            instant_->replay(argInt(a, "seconds"));
            return instant_->available()
                       ? ok("replaying the last " +
                            std::to_string(instant_->lookbackSec()) + "s")
                       : failed("no local recording for this camera");
        });
    registry_.add(
        {"replay.live", "Close the instant replay and return to live",
         "playback.review", false, {}},
        [this](const Args&) {
            if (!instant_) return failed("no recording DB attached (--rec-db)");
            instant_->returnToLive();
            return ok("returned to live");
        });

    // --- Recorded playback transport (inc 30, P1-13) -----------------------
    // These commands close the last explicitly deferred discrete UI-mutation
    // gap. The Playback tab, instant-replay overlay, palette, and HTTP API now
    // share the same capability check and audit trail. Drag frames remain a
    // declared continuous-input exemption; QML commits the final seek here.
    registry_.add(
        {"playback.play", "Play the selected recorded footage",
         "playback.review", false, {}},
        [this](const Args&) {
            if (!playback_) return failed("no recording DB attached (--rec-db)");
            if (!playback_->onFootage())
                return failed("no recorded footage at the playback position");
            playback_->play();
            return ok("playback playing");
        });
    registry_.add(
        {"playback.pause", "Pause recorded playback",
         "playback.review", false, {}},
        [this](const Args&) {
            if (!playback_) return failed("no recording DB attached (--rec-db)");
            playback_->pause();
            return ok("playback paused");
        });
    registry_.add(
        {"playback.speed", "Set recorded playback speed",
         "playback.review", false,
         {{"rate", ParamType::Number, true, 0.25, 16.0}}},
        [this](const Args& a) {
            if (!playback_) return failed("no recording DB attached (--rec-db)");
            const double rate = argNumber(a, "rate");
            playback_->setSpeed(rate);
            return ok("playback speed -> " + std::to_string(rate) + "x");
        });
    registry_.add(
        {"playback.step", "Step recorded playback by an approximate frame count",
         "playback.review", false,
         {{"frames", ParamType::Int, true, -10000, 10000}}},
        [this](const Args& a) {
            if (!playback_) return failed("no recording DB attached (--rec-db)");
            const int frames = argInt(a, "frames");
            playback_->stepFrames(frames);
            return ok("playback stepped " + std::to_string(frames) + " frame(s)");
        });
    registry_.add(
        {"playback.seek", "Seek recorded playback to a fraction of its range",
         "playback.review", false,
         {{"position", ParamType::Number, true, 0.0, 1.0}}},
        [this](const Args& a) {
            if (!playback_) return failed("no recording DB attached (--rec-db)");
            const double position = argNumber(a, "position");
            playback_->seekFrac(position);
            return ok("playback seek -> " + std::to_string(position));
        });

    auto instantPb = [this]() -> PlaybackController* {
        return instant_ && instant_->active() && instant_->available()
                   ? instant_->pb()
                   : nullptr;
    };
    registry_.add(
        {"replay.play", "Play the active instant replay",
         "playback.review", false, {}},
        [instantPb](const Args&) {
            PlaybackController* pb = instantPb();
            if (!pb) return failed("no active instant replay with footage");
            if (!pb->onFootage())
                return failed("no recorded footage at the replay position");
            pb->play();
            return ok("instant replay playing");
        });
    registry_.add(
        {"replay.pause", "Pause the active instant replay",
         "playback.review", false, {}},
        [instantPb](const Args&) {
            PlaybackController* pb = instantPb();
            if (!pb) return failed("no active instant replay with footage");
            pb->pause();
            return ok("instant replay paused");
        });
    registry_.add(
        {"replay.speed", "Set the active instant-replay speed",
         "playback.review", false,
         {{"rate", ParamType::Number, true, 0.25, 16.0}}},
        [instantPb](const Args& a) {
            PlaybackController* pb = instantPb();
            if (!pb) return failed("no active instant replay with footage");
            const double rate = argNumber(a, "rate");
            pb->setSpeed(rate);
            return ok("instant replay speed -> " + std::to_string(rate) + "x");
        });
    registry_.add(
        {"replay.seek", "Seek the active instant replay to a fraction of its range",
         "playback.review", false,
         {{"position", ParamType::Number, true, 0.0, 1.0}}},
        [instantPb](const Args& a) {
            PlaybackController* pb = instantPb();
            if (!pb) return failed("no active instant replay with footage");
            const double position = argNumber(a, "position");
            pb->seekFrac(position);
            return ok("instant replay seek -> " + std::to_string(position));
        });

    // --- Device management (P2) — device.remove is the dangerous exemplar ---
    registry_.add(
        {"device.rename", "Rename a device (associations preserved)",
         "devices.manage", false,
         {{"id", ParamType::String, true}, {"name", ParamType::String, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            return fromDeviceError(
                devices_->renameDevice(QString::fromStdString(id),
                                       QString::fromStdString(argStr(a, "name"))),
                id);
        });
    registry_.add(
        {"device.detach", "Detach (true) or re-attach (false) a device",
         "devices.manage", false,
         {{"id", ParamType::String, true}, {"detached", ParamType::Bool, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            return fromDeviceError(
                devices_->setDeviceDisabled(QString::fromStdString(id),
                                            std::get<bool>(a.at("detached"))),
                id);
        });
    registry_.add(
        {"device.site", "Assign a device to a premises site (or none)",
         "devices.manage", false,
         {{"id", ParamType::String, true},
          {"site", ParamType::String, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            return fromDeviceError(
                devices_->assignSite(
                    QString::fromStdString(id),
                    QString::fromStdString(argStr(a, "site"))),
                id);
        });
    registry_.add(
        {"device.remove", "Remove a device and its credential (destructive)",
         "devices.manage", /*dangerous=*/true,
         {{"id", ParamType::String, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            return fromDeviceError(
                devices_->removeDevice(QString::fromStdString(id)), id);
        });

    // --- The rest of the device surface (inc 29, P1-13 coverage) -----------
    // Every state-changing Devices-tab action is a command, so a click, a
    // palette line, and an API call are one validated, audited path.
    auto optStr = [](const Args& a, const char* name) {
        const auto it = a.find(name);
        return it == a.end() ? std::string()
                             : std::get<std::string>(it->second);
    };
    registry_.add(
        {"device.onboard", "Onboard a direct IP camera", "devices.manage", false,
         {{"id", ParamType::String, true},
          {"name", ParamType::String, true},
          {"address", ParamType::String, true},
          {"vendor", ParamType::String, false},
          {"user", ParamType::String, false},
          {"password", ParamType::String, false, 1, 0, {}, /*secret=*/true},
          {"mainUrl", ParamType::String, false},
          {"subUrl", ParamType::String, false}}},
        [this, optStr](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            return fromDeviceError(
                devices_->onboard(QString::fromStdString(id),
                                  QString::fromStdString(argStr(a, "name")),
                                  QString::fromStdString(argStr(a, "address")),
                                  QString::fromStdString(optStr(a, "vendor")),
                                  QString::fromStdString(optStr(a, "user")),
                                  QString::fromStdString(optStr(a, "password")),
                                  QString::fromStdString(optStr(a, "mainUrl")),
                                  QString::fromStdString(optStr(a, "subUrl"))),
                id);
        });
    registry_.add(
        {"device.onboardRecorder", "Onboard a multi-channel recorder",
         "devices.manage", false,
         {{"id", ParamType::String, true},
          {"name", ParamType::String, true},
          {"address", ParamType::String, true},
          {"vendor", ParamType::String, false},
          {"kind", ParamType::Enum, true, 1, 0, {"nvr", "dvr", "hybrid"}},
          {"user", ParamType::String, false},
          {"password", ParamType::String, false, 1, 0, {}, /*secret=*/true},
          {"channels", ParamType::Int, true, 1, 512},
          {"mainTemplate", ParamType::String, false},
          {"subTemplate", ParamType::String, false}}},
        [this, optStr](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            return fromDeviceError(
                devices_->onboardRecorder(
                    QString::fromStdString(id),
                    QString::fromStdString(argStr(a, "name")),
                    QString::fromStdString(argStr(a, "address")),
                    QString::fromStdString(optStr(a, "vendor")),
                    QString::fromStdString(argStr(a, "kind")),
                    QString::fromStdString(optStr(a, "user")),
                    QString::fromStdString(optStr(a, "password")),
                    argInt(a, "channels"),
                    QString::fromStdString(optStr(a, "mainTemplate")),
                    QString::fromStdString(optStr(a, "subTemplate"))),
                id);
        });
    registry_.add(
        {"device.onboardDiscovered", "Onboard a discovered ONVIF camera",
         "devices.manage", false,
         {{"endpoint", ParamType::String, true},
          {"user", ParamType::String, false},
          {"password", ParamType::String, false, 1, 0, {}, /*secret=*/true}}},
        [this, optStr](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string ep = argStr(a, "endpoint");
            return fromDeviceError(
                devices_->onboardDiscovered(
                    QString::fromStdString(ep),
                    QString::fromStdString(optStr(a, "user")),
                    QString::fromStdString(optStr(a, "password"))),
                ep);
        });
    registry_.add(
        {"device.discover", "Scan the LAN for ONVIF devices", "devices.manage",
         false, {{"timeoutMs", ParamType::Int, false, 100, 60000}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const auto it = a.find("timeoutMs");
            devices_->startDiscovery(
                it == a.end() ? 3000
                              : static_cast<int>(std::get<std::int64_t>(it->second)));
            return ok("discovery started");
        });
    registry_.add(
        {"device.rescan", "Rescan a camera's streams from discovery",
         "devices.manage", false,
         {{"id", ParamType::String, true},
          {"user", ParamType::String, false},
          {"password", ParamType::String, false, 1, 0, {}, /*secret=*/true}}},
        [this, optStr](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            return fromDeviceError(
                devices_->rescanDevice(
                    QString::fromStdString(id),
                    QString::fromStdString(optStr(a, "user")),
                    QString::fromStdString(optStr(a, "password"))),
                id);
        });
    registry_.add(
        {"device.acknowledge", "Acknowledge a device's health exception",
         "devices.manage", false, {{"id", ParamType::String, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            devices_->acknowledge(QString::fromStdString(id));
            return ok("device " + id + ": acknowledged");
        });
    registry_.add(
        {"device.maintenance", "Put a device in or out of a maintenance window",
         "devices.manage", false,
         {{"id", ParamType::String, true}, {"on", ParamType::Bool, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string id = argStr(a, "id");
            const bool on = std::get<bool>(a.at("on"));
            devices_->setMaintenance(QString::fromStdString(id), on);
            // Alarm notifications follow the same window (inc 27).
            if (alarms_)
                alarms_->setDeviceMaintenance(QString::fromStdString(id), on);
            return ok("device " + id + (on ? ": in maintenance"
                                           : ": maintenance ended"));
        });
    registry_.add(
        {"channel.rename", "Rename a device channel", "devices.manage", false,
         {{"device", ParamType::String, true},
          {"camera", ParamType::String, true},
          {"name", ParamType::String, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string cam = argStr(a, "camera");
            return fromDeviceError(
                devices_->renameChannel(
                    QString::fromStdString(argStr(a, "device")),
                    QString::fromStdString(cam),
                    QString::fromStdString(argStr(a, "name"))),
                cam);
        });
    registry_.add(
        {"channel.disable", "Enable or disable a device channel",
         "devices.manage", false,
         {{"device", ParamType::String, true},
          {"camera", ParamType::String, true},
          {"disabled", ParamType::Bool, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string cam = argStr(a, "camera");
            return fromDeviceError(
                devices_->setChannelDisabled(
                    QString::fromStdString(argStr(a, "device")),
                    QString::fromStdString(cam),
                    std::get<bool>(a.at("disabled"))),
                cam);
        });
    registry_.add(
        {"channel.remove", "Remove a channel from a device (destructive)",
         "devices.manage", /*dangerous=*/true,
         {{"device", ParamType::String, true},
          {"camera", ParamType::String, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string cam = argStr(a, "camera");
            return fromDeviceError(
                devices_->removeChannel(
                    QString::fromStdString(argStr(a, "device")),
                    QString::fromStdString(cam)),
                cam);
        });
    registry_.add(
        {"channel.move", "Reorder a channel within its device",
         "devices.manage", false,
         {{"device", ParamType::String, true},
          {"camera", ParamType::String, true},
          {"up", ParamType::Bool, true}}},
        [this](const Args& a) {
            if (!devices_) return failed("no device store attached");
            const std::string cam = argStr(a, "camera");
            return fromDeviceError(
                devices_->moveChannel(
                    QString::fromStdString(argStr(a, "device")),
                    QString::fromStdString(cam), std::get<bool>(a.at("up"))),
                cam);
        });
    registry_.add(
        {"workspace.place", "Place a camera tile on the spatial canvas",
         "workspace.control", false,
         {{"tile", ParamType::Int, true, 0, 4095},
          {"x", ParamType::Number, true},
          {"y", ParamType::Number, true}}},
        [this](const Args& a) {
            if (!live_) return failed("no live workspace");
            const int t = argInt(a, "tile");
            if (t >= live_->tileCount()) return failed("no such tile");
            auto num = [&](const char* n) {
                const vms::command::Value& v = a.at(n);
                return std::holds_alternative<double>(v)
                           ? std::get<double>(v)
                           : static_cast<double>(std::get<std::int64_t>(v));
            };
            live_->setTilePos(t, num("x"), num("y"));
            return ok("tile " + std::to_string(t) + " placed");
        });

    // --- Alarm lifecycle (P6-07, inc 27) — the panel, the palette, and the
    // API all acknowledge through this same audited verb.
    registry_.add(
        {"alarm.ack", "Acknowledge an alarm (silences it; it persists)",
         "alarms.manage", false,
         {{"id", ParamType::Int, true, 1, 1e12}}},
        [this](const Args& a) {
            if (!alarms_) return failed("no alarm engine attached");
            const int id = argInt(a, "id");
            return alarms_->acknowledge(id)
                       ? ok("alarm " + std::to_string(id) + " acknowledged")
                       : failed("no active alarm " + std::to_string(id));
        });
    registry_.add(
        {"alarm.escalate", "Escalate an alarm (re-raises attention)",
         "alarms.manage", false,
         {{"id", ParamType::Int, true, 1, 1e12}}},
        [this](const Args& a) {
            if (!alarms_) return failed("no alarm engine attached");
            const int id = argInt(a, "id");
            return alarms_->escalate(id)
                       ? ok("alarm " + std::to_string(id) + " escalated")
                       : failed("no active alarm " + std::to_string(id));
        });
    registry_.add(
        {"alarm.clear", "Clear an alarm (ends it; history keeps it)",
         "alarms.manage", false,
         {{"id", ParamType::Int, true, 1, 1e12}}},
        [this](const Args& a) {
            if (!alarms_) return failed("no alarm engine attached");
            const int id = argInt(a, "id");
            return alarms_->clearAlarm(id)
                       ? ok("alarm " + std::to_string(id) + " cleared")
                       : failed("no active alarm " + std::to_string(id));
        });
}

void CommandController::setAuditStore(vms::persist::AuditRepo* repo) {
    auditRepo_ = repo;
}

QVariantMap CommandController::invoke(const QString& id, const QVariantMap& args,
                                      bool confirm) {
    Args a;
    for (auto it = args.begin(); it != args.end(); ++it) {
        const QVariant& v = it.value();
        const std::string key = it.key().toStdString();
        switch (v.typeId()) {
            case QMetaType::Bool:
                a[key] = v.toBool();
                break;
            case QMetaType::Int:
            case QMetaType::LongLong:
            case QMetaType::UInt:
            case QMetaType::ULongLong:
                a[key] = static_cast<std::int64_t>(v.toLongLong());
                break;
            case QMetaType::Double:
            case QMetaType::Float: {
                const double d = v.toDouble();
                // QML numbers are doubles; integral values bind to Int params.
                if (d == std::floor(d) && std::abs(d) < 9.0e15)
                    a[key] = static_cast<std::int64_t>(d);
                else
                    a[key] = d;
                break;
            }
            default:
                a[key] = v.toString().toStdString();
                break;
        }
    }
    const CommandResult r = registry_.invoke(id.toStdString(), a, capabilities_,
                                             confirm);
    QVariantMap out;
    out.insert(QStringLiteral("ok"), static_cast<bool>(r));
    out.insert(QStringLiteral("outcome"),
               QString::fromLatin1(OutcomeName(r.outcome)));
    out.insert(QStringLiteral("message"), QString::fromStdString(r.message));
    return out;
}

QString CommandController::run(const QString& line) {
    source_ = QStringLiteral("palette");   // the text-envelope entry point
    const CommandResult r =
        registry_.invokeText(line.trimmed().toStdString(), capabilities_);
    source_ = QStringLiteral("ui");
    if (r) return QStringLiteral("ok · %1").arg(QString::fromStdString(r.message));
    return QStringLiteral("%1 · %2")
        .arg(QString::fromLatin1(OutcomeName(r.outcome)))
        .arg(QString::fromStdString(r.message));
}

QString CommandController::catalogJson() const {
    return QString::fromStdString(registry_.catalogJson());
}
