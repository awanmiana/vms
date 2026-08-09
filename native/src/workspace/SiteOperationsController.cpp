#include "SiteOperationsController.h"

#include "AlarmController.h"
#include "DeviceController.h"
#include "PremisesController.h"
#include "WorkspaceController.h"
#include "persist/SegmentIndex.h"
#include "persist/StreamDurationRepo.h"

#include <QDateTime>
#include <QSet>
#include <QStringList>
#include <QTimeZone>
#include <QVariantList>

#include <algorithm>

namespace {

QVariantMap unavailable(const QString& reason) {
    return {{QStringLiteral("available"), false},
            {QStringLiteral("stateText"), QStringLiteral("Unavailable")},
            {QStringLiteral("reason"), reason}};
}

QString durationText(qint64 seconds) {
    if (seconds < 0) seconds = 0;
    const qint64 days = seconds / 86400;
    const qint64 hours = (seconds % 86400) / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (seconds < 60)
        return QStringLiteral("%1s").arg(seconds);
    if (days > 0)
        return QStringLiteral("%1d %2h").arg(days).arg(hours);
    if (hours > 0)
        return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    return QStringLiteral("%1m").arg(minutes);
}

QDateTime parseUtc(const QString& value) {
    if (value.isEmpty()) return {};
    return QDateTime::fromString(
        QString(value).replace(QLatin1Char(' '), QLatin1Char('T')) +
            QLatin1Char('Z'),
        Qt::ISODate);
}

}  // namespace

SiteOperationsController::SiteOperationsController(
    PremisesController* premises, DeviceController* devices,
    WorkspaceController* live, vms::persist::SegmentIndex* recordings,
    vms::persist::StreamDurationRepo* streams, AlarmController* alarms,
    QObject* parent)
    : QObject(parent), premises_(premises), devices_(devices), live_(live),
      recordings_(recordings), streams_(streams), alarms_(alarms) {
    if (premises_)
        connect(premises_, &PremisesController::changed, this,
                &SiteOperationsController::refresh);
    if (devices_)
        connect(devices_, &DeviceController::changed, this,
                &SiteOperationsController::refresh);
    if (live_)
        connect(live_, &WorkspaceController::diagnosticsChanged, this,
                &SiteOperationsController::refresh);
    if (alarms_)
        connect(alarms_, &AlarmController::changed, this,
                &SiteOperationsController::refresh);
    connect(&clock_, &QTimer::timeout, this,
            &SiteOperationsController::refresh);
    clock_.start(1000);
    refresh();
}

void SiteOperationsController::advanceStreamingObservation(
    const QString& nextSiteId, int nextPlayingBranches) {
    qint64 elapsedMilliseconds = 0;
    if (streamingObservationAge_.isValid())
        elapsedMilliseconds = streamingObservationAge_.restart();
    else
        streamingObservationAge_.start();

    if (streams_ && !observedStreamingSiteId_.isEmpty() &&
        observedPlayingBranches_ > 0 && elapsedMilliseconds > 0) {
        const vms::persist::Error error = streams_->addObservation(
            observedStreamingSiteId_.toStdString(), elapsedMilliseconds,
            observedPlayingBranches_);
        if (!error) {
            streamingCheckpointError_ = QString::fromStdString(error.message);
        } else {
            streamingCheckpointError_.clear();
        }
    }
    observedStreamingSiteId_ = nextSiteId;
    observedPlayingBranches_ = std::max(0, nextPlayingBranches);
}

void SiteOperationsController::flushStreaming() {
    advanceStreamingObservation(QString(), 0);
}

void SiteOperationsController::refresh() {
    QVariantMap next;

    QVariantMap site;
    const bool siteAvailable = premises_ && !premises_->siteId().isEmpty();
    site.insert(QStringLiteral("available"), siteAvailable);
    site.insert(QStringLiteral("id"),
                siteAvailable ? premises_->siteId() : QString());
    site.insert(QStringLiteral("name"),
                siteAvailable ? premises_->siteName()
                              : QStringLiteral("Premises unavailable"));
    site.insert(QStringLiteral("floor"),
                siteAvailable ? premises_->floorName() : QString());
    site.insert(QStringLiteral("timezone"),
                siteAvailable ? premises_->timezone() : QString());
    if (!siteAvailable)
        site.insert(QStringLiteral("reason"),
                    QStringLiteral("No active premises is attached"));
    next.insert(QStringLiteral("site"), site);

    QVariantMap localClock;
    const QByteArray zoneId = site.value(QStringLiteral("timezone")).toString()
                                  .toUtf8();
    const QTimeZone zone(zoneId);
    const bool clockAvailable = siteAvailable && zone.isValid();
    localClock.insert(QStringLiteral("available"), clockAvailable);
    if (clockAvailable) {
        const QDateTime local =
            QDateTime::currentDateTimeUtc().toTimeZone(zone);
        localClock.insert(QStringLiteral("date"),
                          local.toString(QStringLiteral("yyyy-MM-dd")));
        localClock.insert(QStringLiteral("time"),
                          local.toString(QStringLiteral("HH:mm:ss")));
        localClock.insert(QStringLiteral("zone"),
                          local.toString(QStringLiteral("t")));
        localClock.insert(QStringLiteral("stateText"),
                          QStringLiteral("Available"));
    } else {
        localClock.insert(QStringLiteral("date"), QString());
        localClock.insert(QStringLiteral("time"), QString());
        localClock.insert(QStringLiteral("zone"), QString());
        localClock.insert(
            QStringLiteral("stateText"), QStringLiteral("Unavailable"));
        localClock.insert(QStringLiteral("reason"),
                          siteAvailable
                              ? QStringLiteral("Configured timezone is invalid")
                              : QStringLiteral("No active premises/timezone"));
    }
    next.insert(QStringLiteral("localClock"), localClock);

    next.insert(QStringLiteral("operatingHours"),
                premises_
                    ? premises_->operatingHoursAtUtc(
                          QDateTime::currentDateTimeUtc())
                    : unavailable(QStringLiteral(
                          "No active premises schedule source is attached")));
    QVariantMap device;
    device.insert(QStringLiteral("available"), devices_ != nullptr);
    int total = 0, attached = 0, detached = 0, online = 0, degraded = 0;
    int offline = 0, unknown = 0, unsupported = 0, maintenance = 0;
    int attention = 0, unassigned = 0, otherSite = 0;
    int reachObserved = 0, currentReachable = 0, lastSeenCount = 0;
    qint64 longestUptimeSeconds = 0;
    QString latestLastSeenUtc;
    QStringList siteCameraIds;
    const QString activeSiteId = site.value(QStringLiteral("id")).toString();
    if (devices_) {
        for (const QVariant& value : devices_->devices()) {
            const QVariantMap d = value.toMap();
            const QString deviceSite =
                d.value(QStringLiteral("siteId")).toString();
            if (deviceSite.isEmpty()) {
                ++unassigned;
                continue;
            }
            if (!siteAvailable || deviceSite != activeSiteId) {
                ++otherSite;
                continue;
            }
            ++total;
            const QVariantList channels =
                d.value(QStringLiteral("channels")).toList();
            for (const QVariant& channelValue : channels) {
                const QString cameraId =
                    channelValue.toMap().value(QStringLiteral("id")).toString();
                if (!cameraId.isEmpty()) siteCameraIds.push_back(cameraId);
            }
            const bool disabled = d.value(QStringLiteral("disabled")).toBool();
            const QVariantMap health = d.value(QStringLiteral("health")).toMap();
            if (health.value(QStringLiteral("reachObserved")).toBool())
                ++reachObserved;
            if (health.value(QStringLiteral("lastSeenAvailable")).toBool()) {
                ++lastSeenCount;
                const QString seen =
                    health.value(QStringLiteral("lastSeenUtc")).toString();
                if (seen > latestLastSeenUtc) latestLastSeenUtc = seen;
            }
            if (health.value(QStringLiteral("inMaintenance")).toBool())
                ++maintenance;
            if (health.value(QStringLiteral("needsAttention")).toBool())
                ++attention;
            if (disabled) {
                ++detached;
                continue;
            }
            ++attached;
            if (health.value(QStringLiteral("uptimeAvailable")).toBool()) {
                ++currentReachable;
                const QDateTime since = parseUtc(
                    health.value(QStringLiteral("onlineSinceUtc")).toString());
                const QDateTime now = QDateTime::currentDateTimeUtc();
                const qint64 seconds = since.isValid() && since <= now
                    ? since.secsTo(now) : 0;
                longestUptimeSeconds = std::max(
                    longestUptimeSeconds, seconds);
            }
            const QString state = health.value(QStringLiteral("state")).toString();
            if (state == QLatin1String("online")) ++online;
            else if (state == QLatin1String("degraded")) ++degraded;
            else if (state == QLatin1String("offline")) ++offline;
            else if (state == QLatin1String("unsupported")) ++unsupported;
            else ++unknown;
        }
    } else {
        device.insert(QStringLiteral("reason"),
                      QStringLiteral("Device inventory is unavailable"));
    }
    device.insert(QStringLiteral("total"), total);
    device.insert(QStringLiteral("attached"), attached);
    device.insert(QStringLiteral("detached"), detached);
    device.insert(QStringLiteral("online"), online);
    device.insert(QStringLiteral("degraded"), degraded);
    device.insert(QStringLiteral("offline"), offline);
    device.insert(QStringLiteral("unknown"), unknown);
    device.insert(QStringLiteral("unsupported"), unsupported);
    device.insert(QStringLiteral("maintenance"), maintenance);
    device.insert(QStringLiteral("attention"), attention);
    device.insert(QStringLiteral("unassigned"), unassigned);
    device.insert(QStringLiteral("otherSite"), otherSite);
    next.insert(QStringLiteral("devices"), device);

    QVariantMap uptime;
    const bool uptimeAvailable = currentReachable > 0 || lastSeenCount > 0;
    uptime.insert(QStringLiteral("available"), uptimeAvailable);
    uptime.insert(QStringLiteral("siteDevices"), total);
    uptime.insert(QStringLiteral("observed"), reachObserved);
    uptime.insert(QStringLiteral("currentReachable"), currentReachable);
    uptime.insert(QStringLiteral("lastSeenCount"), lastSeenCount);
    uptime.insert(QStringLiteral("longestUptimeSeconds"),
                  longestUptimeSeconds);
    uptime.insert(QStringLiteral("longestUptimeText"),
                  durationText(longestUptimeSeconds));
    uptime.insert(QStringLiteral("latestLastSeenUtc"), latestLastSeenUtc);
    if (uptimeAvailable) {
        uptime.insert(
            QStringLiteral("stateText"),
            currentReachable > 0
                ? QStringLiteral("%1 reachable · longest %2")
                      .arg(currentReachable)
                      .arg(durationText(longestUptimeSeconds))
                : QStringLiteral("No device currently reachable"));
    } else {
        uptime.insert(QStringLiteral("stateText"), QStringLiteral("Unavailable"));
        uptime.insert(
            QStringLiteral("reason"),
            total == 0
                ? QStringLiteral("No devices are assigned to this premises")
                : QStringLiteral("No positive reachability evidence is stored"));
    }
    next.insert(QStringLiteral("uptimeLastSeen"), uptime);

    // P3-18 / inc 38: completed footage is authoritative recording time. The
    // SegmentIndex may be a separate SQLite store from inventory, so ownership
    // crosses the adapter boundary as stable camera ids. Recompute at most every
    // 30 seconds unless the active site/channel set changes; the one-second clock
    // refresh must not rescan a potentially large history every tick.
    siteCameraIds.removeDuplicates();
    siteCameraIds.sort();
    const QString recordingScopeKey =
        activeSiteId + QLatin1Char('|') + siteCameraIds.join(QLatin1Char('|'));
    const bool recordingCacheExpired =
        !recordingCacheAge_.isValid() || recordingCacheAge_.elapsed() >= 30000;
    if (recordingScopeKey_ != recordingScopeKey || recordingCacheExpired) {
        QVariantMap duration;
        if (!siteAvailable) {
            duration = unavailable(
                QStringLiteral("No active premises is attached"));
        } else if (!recordings_) {
            duration = unavailable(QStringLiteral(
                "No completed-recording index is connected"));
        } else if (siteCameraIds.isEmpty()) {
            duration = unavailable(QStringLiteral(
                "No camera channels are assigned to this premises"));
        } else {
            std::vector<std::string> ids;
            ids.reserve(static_cast<std::size_t>(siteCameraIds.size()));
            for (const QString& id : siteCameraIds)
                ids.push_back(id.toStdString());
            vms::persist::RecordingDuration summary;
            const vms::persist::Error error =
                recordings_->recordedDuration(ids, summary);
            if (!error) {
                duration = unavailable(QStringLiteral(
                    "Completed-recording index query failed: %1")
                    .arg(QString::fromStdString(error.message)));
            } else {
                duration.insert(QStringLiteral("available"), true);
                duration.insert(QStringLiteral("stateText"),
                                QStringLiteral("Recorded %1")
                                    .arg(durationText(summary.seconds)));
                duration.insert(QStringLiteral("recordingAvailable"), true);
                duration.insert(QStringLiteral("recordingSeconds"),
                                summary.seconds);
                duration.insert(QStringLiteral("recordingText"),
                                durationText(summary.seconds));
                duration.insert(QStringLiteral("rawRecordingSeconds"),
                                summary.rawSeconds);
                duration.insert(QStringLiteral("overlapRemovedSeconds"),
                                summary.overlapRemovedSeconds);
                duration.insert(QStringLiteral("segments"),
                                summary.segmentCount);
                duration.insert(QStringLiteral("siteCameras"),
                                siteCameraIds.size());
                duration.insert(QStringLiteral("camerasWithFootage"),
                                summary.camerasWithFootage);
                duration.insert(QStringLiteral("overlappingSegments"),
                                summary.overlappingSegments);
                duration.insert(QStringLiteral("invalidSegments"),
                                summary.invalidSegments);
                if (summary.invalidSegments > 0)
                    duration.insert(QStringLiteral("qualityWarning"),
                                    QStringLiteral("%1 malformed legacy segment(s) excluded")
                                        .arg(summary.invalidSegments));
            }
        }
        recordingDurationCache_ = duration;
        recordingScopeKey_ = recordingScopeKey;
        recordingCacheAge_.restart();
    }
    QVariantMap media;
    int mediaTotal = 0, playing = 0, connecting = 0, stalled = 0;
    int paused = 0, mediaUnavailable = 0, fpsSamples = 0;
    double fpsTotal = 0.0;
    if (live_) {
        const QVariantList diagnostics = live_->mediaDiagnostics();
        mediaTotal = diagnostics.size();
        for (const QVariant& value : diagnostics) {
            const QVariantMap d = value.toMap();
            const QString state =
                d.value(QStringLiteral("streamState")).toString();
            if (state == QLatin1String("playing")) ++playing;
            else if (state == QLatin1String("connecting")) ++connecting;
            else if (state == QLatin1String("stalled")) ++stalled;
            else if (state == QLatin1String("paused")) ++paused;
            else ++mediaUnavailable;
            if (d.value(QStringLiteral("fpsAvailable")).toBool()) {
                fpsTotal += d.value(QStringLiteral("fps")).toDouble();
                ++fpsSamples;
            }
        }
    }
    const bool mediaAvailable =
        live_ && (playing + connecting + stalled + paused) > 0;
    media.insert(QStringLiteral("available"), mediaAvailable);
    media.insert(QStringLiteral("total"), mediaTotal);
    media.insert(QStringLiteral("playing"), playing);
    media.insert(QStringLiteral("connecting"), connecting);
    media.insert(QStringLiteral("stalled"), stalled);
    media.insert(QStringLiteral("paused"), paused);
    media.insert(QStringLiteral("unavailable"), mediaUnavailable);
    media.insert(QStringLiteral("displayFpsAvailable"), fpsSamples > 0);
    media.insert(QStringLiteral("displayFps"),
                 fpsSamples > 0 ? fpsTotal / fpsSamples : 0.0);
    if (!mediaAvailable)
        media.insert(QStringLiteral("reason"), QStringLiteral(
            "Live media is disabled or has no active/policy-paused observations"));
    next.insert(QStringLiteral("media"), media);

    // P3-18 / inc 39: settle the elapsed interval against the observation that
    // was in force before this refresh. QElapsedTimer is monotonic, so a wall-
    // clock adjustment cannot create or erase duration. The current observation
    // becomes the basis for the next interval. Only decoded `playing` branches
    // count; connecting/stalled/paused states remain excluded.
    advanceStreamingObservation(activeSiteId, playing);
    QVariantMap cumulative = recordingDurationCache_;
    const bool recordingAvailable =
        cumulative.value(QStringLiteral("recordingAvailable")).toBool();
    if (!recordingAvailable) {
        cumulative.insert(QStringLiteral("recordingAvailable"), false);
        cumulative.insert(QStringLiteral("recordingReason"),
                          cumulative.value(QStringLiteral("reason")));
    }

    bool streamingAvailable = false;
    qint64 streamingMilliseconds = 0;
    if (!siteAvailable) {
        cumulative.insert(QStringLiteral("streamingReason"),
                          QStringLiteral("No active premises is attached"));
    } else if (!streams_) {
        cumulative.insert(QStringLiteral("streamingReason"), QStringLiteral(
            "No persistent stream-duration source is connected"));
    } else if (!streamingCheckpointError_.isEmpty()) {
        cumulative.insert(QStringLiteral("streamingReason"), QStringLiteral(
            "Stream-duration checkpoint failed: %1")
            .arg(streamingCheckpointError_));
    } else {
        vms::persist::StreamDurationSummary summary;
        const vms::persist::Error error =
            streams_->summary(activeSiteId.toStdString(), summary);
        if (!error) {
            cumulative.insert(QStringLiteral("streamingReason"), QStringLiteral(
                "Stream-duration query failed: %1")
                .arg(QString::fromStdString(error.message)));
        } else {
            streamingAvailable = true;
            streamingMilliseconds = summary.milliseconds;
            cumulative.insert(QStringLiteral("streamingMilliseconds"),
                              streamingMilliseconds);
            cumulative.insert(QStringLiteral("streamingSeconds"),
                              streamingMilliseconds / 1000);
            cumulative.insert(QStringLiteral("streamingText"),
                              durationText(streamingMilliseconds / 1000));
            cumulative.insert(QStringLiteral("streamingCheckpoints"),
                              summary.checkpoints);
            cumulative.insert(QStringLiteral("streamingUpdatedAtUtc"),
                              QString::fromStdString(summary.updatedAtUtc));
        }
    }
    cumulative.insert(QStringLiteral("streamingAvailable"),
                      streamingAvailable);
    cumulative.insert(QStringLiteral("partial"),
                      !(recordingAvailable && streamingAvailable));
    cumulative.insert(QStringLiteral("combinedAvailable"),
                      recordingAvailable && streamingAvailable);
    if (recordingAvailable && streamingAvailable) {
        const qint64 combinedSeconds =
            cumulative.value(QStringLiteral("recordingSeconds")).toLongLong() +
            streamingMilliseconds / 1000;
        cumulative.insert(QStringLiteral("combinedSeconds"), combinedSeconds);
        cumulative.insert(QStringLiteral("combinedText"),
                          durationText(combinedSeconds));
        cumulative.insert(QStringLiteral("available"), true);
        cumulative.insert(QStringLiteral("stateText"),
                          QStringLiteral("Combined %1")
                              .arg(durationText(combinedSeconds)));
    } else if (recordingAvailable) {
        cumulative.insert(QStringLiteral("available"), true);
        cumulative.insert(QStringLiteral("stateText"),
                          QStringLiteral("Recorded %1")
                              .arg(cumulative.value(
                                  QStringLiteral("recordingText")).toString()));
    } else if (streamingAvailable) {
        cumulative.insert(QStringLiteral("available"), true);
        cumulative.insert(QStringLiteral("stateText"),
                          QStringLiteral("Streamed %1")
                              .arg(durationText(streamingMilliseconds / 1000)));
    }
    next.insert(QStringLiteral("cumulativeDuration"), cumulative);

    // P3-18 / inc 40: the first analysis source is deliberately narrow. The
    // alarm engine is authoritative for current active device-health alarms;
    // DeviceController is authoritative for each device's current site. This
    // is a session-scoped operational summary, not retained history or VCA.
    QVariantMap analysis;
    if (!siteAvailable) {
        analysis = unavailable(QStringLiteral(
            "No active premises is attached"));
    } else if (!devices_) {
        analysis = unavailable(QStringLiteral(
            "No device ownership source is connected"));
    } else if (!alarms_) {
        analysis = unavailable(QStringLiteral(
            "No active alarm source is connected"));
    } else {
        QSet<QString> siteDeviceIds;
        for (const QVariant& value : devices_->devices()) {
            const QVariantMap device = value.toMap();
            if (device.value(QStringLiteral("siteId")).toString() == activeSiteId)
                siteDeviceIds.insert(device.value(QStringLiteral("id")).toString());
        }

        int active = 0, attention = 0, high = 0, medium = 0, low = 0;
        int acknowledged = 0, suppressed = 0, occurrences = 0;
        QSet<QString> affectedDevices;
        for (const QVariant& value : alarms_->alarms()) {
            const QVariantMap alarm = value.toMap();
            const QString deviceId = alarm.value(QStringLiteral("device")).toString();
            if (!siteDeviceIds.contains(deviceId)) continue;
            ++active;
            affectedDevices.insert(deviceId);
            occurrences += std::max(0, alarm.value(QStringLiteral("count")).toInt());
            const bool isSuppressed =
                alarm.value(QStringLiteral("suppressed")).toBool();
            if (isSuppressed) ++suppressed;
            const QString state = alarm.value(QStringLiteral("state")).toString();
            if (state == QLatin1String("acknowledged")) ++acknowledged;
            if (!isSuppressed &&
                (state == QLatin1String("new") ||
                 state == QLatin1String("escalated")))
                ++attention;
            const QString priority =
                alarm.value(QStringLiteral("priority")).toString();
            if (priority == QLatin1String("high")) ++high;
            else if (priority == QLatin1String("medium")) ++medium;
            else if (priority == QLatin1String("low")) ++low;
        }

        analysis.insert(QStringLiteral("available"), true);
        analysis.insert(QStringLiteral("source"),
                        QStringLiteral("Current device-health alarms"));
        analysis.insert(QStringLiteral("stateText"),
                        active == 0
                            ? QStringLiteral("No active device-health alarms")
                            : QStringLiteral("%1 active · %2 need attention")
                                  .arg(active).arg(attention));
        analysis.insert(QStringLiteral("active"), active);
        analysis.insert(QStringLiteral("needsAttention"), attention);
        analysis.insert(QStringLiteral("high"), high);
        analysis.insert(QStringLiteral("medium"), medium);
        analysis.insert(QStringLiteral("low"), low);
        analysis.insert(QStringLiteral("acknowledged"), acknowledged);
        analysis.insert(QStringLiteral("suppressed"), suppressed);
        analysis.insert(QStringLiteral("occurrences"), occurrences);
        analysis.insert(QStringLiteral("affectedDevices"),
                        affectedDevices.size());
        analysis.insert(QStringLiteral("coverageNote"), QStringLiteral(
            "Current session and device-health rules only; no VCA or retained trend history"));
    }
    next.insert(QStringLiteral("analysis"), analysis);

    if (next == snapshot_) return;
    snapshot_ = next;
    emit changed();
}
