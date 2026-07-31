#include "DeviceController.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "onvif/OnvifTransport.h"

using vms::health::HealthMonitor;
using vms::health::State;
using vms::health::Reach;
using vms::health::Stream;
using vms::health::Storage;
using vms::persist::DeviceRepo;
using vms::persist::DeviceOnboard;
using vms::persist::DeviceSummary;
using vms::persist::CameraChannel;
using vms::persist::ChannelSummary;
using vms::persist::Error;
using vms::onvif::DiscoverySource;
using vms::onvif::DiscoveryCandidate;
using vms::onvif::OnvifDevice;
using vms::onvif::MediaProfile;

namespace {

// A stable kebab category QML switches on for colour, matching the tile-state
// vocabulary (green/amber/red/slate/grey), plus a display string.
struct HealthView { QString category; QString text; };

HealthView stateView(State s) {
    switch (s) {
        case State::Online:      return {QStringLiteral("online"), QStringLiteral("Online")};
        case State::Degraded:    return {QStringLiteral("degraded"), QStringLiteral("Degraded")};
        case State::Offline:     return {QStringLiteral("offline"), QStringLiteral("Offline")};
        case State::Unsupported: return {QStringLiteral("unsupported"), QStringLiteral("Unsupported")};
        case State::Unknown:     return {QStringLiteral("unknown"), QStringLiteral("Unknown")};
    }
    return {QStringLiteral("unknown"), QStringLiteral("Unknown")};
}

QString reachText(Reach r) {
    switch (r) {
        case Reach::Online:  return QStringLiteral("online");
        case Reach::Offline: return QStringLiteral("offline");
        case Reach::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString streamText(Stream s) {
    switch (s) {
        case Stream::Ok:       return QStringLiteral("ok");
        case Stream::Degraded: return QStringLiteral("degraded");
        case Stream::Failed:   return QStringLiteral("failed");
        case Stream::Unknown:  return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString storageText(Storage s) {
    switch (s) {
        case Storage::Ok:            return QStringLiteral("ok");
        case Storage::Low:           return QStringLiteral("low");
        case Storage::Failed:        return QStringLiteral("failed");
        case Storage::NotApplicable: return QStringLiteral("n/a");
        case Storage::Unknown:       return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString errMessage(const Error& e) {
    QString m = QString::fromLatin1(vms::persist::StatusName(e.status));
    if (!e.message.empty())
        m += QStringLiteral(": ") + QString::fromStdString(e.message);
    return m;
}

}  // namespace

DeviceController::DeviceController(DeviceRepo* repo, HealthMonitor* health,
                                   DiscoverySource* discovery,
                                   vms::health::DeviceHealthProbe* probe,
                                   QObject* parent)
    : QObject(parent), repo_(repo), health_(health), discovery_(discovery),
      probe_(probe) {
    refresh();
}

QVariantMap DeviceController::healthMap(const std::string& deviceId) const {
    QVariantMap m;
    const vms::health::DeviceHealth h = health_->get(deviceId);
    const HealthView sv = stateView(h.state);
    m.insert(QStringLiteral("state"), sv.category);
    m.insert(QStringLiteral("stateText"), sv.text);
    m.insert(QStringLiteral("reach"), reachText(h.reach));
    m.insert(QStringLiteral("stream"), streamText(h.stream));
    m.insert(QStringLiteral("storage"), storageText(h.storage));
    m.insert(QStringLiteral("supported"), h.supported);
    m.insert(QStringLiteral("firmware"), QString::fromStdString(h.firmware));
    m.insert(QStringLiteral("exceptionActive"), h.exceptionActive);
    m.insert(QStringLiteral("exceptionAcknowledged"), h.exceptionAcknowledged);
    m.insert(QStringLiteral("exceptionReason"),
             QString::fromStdString(h.exceptionReason));
    m.insert(QStringLiteral("inMaintenance"), h.inMaintenance);
    // shouldNotify(): an active, unacknowledged, non-maintenance exception.
    m.insert(QStringLiteral("needsAttention"), health_->shouldNotify(deviceId));
    return m;
}

void DeviceController::rebuild() {
    devices_.clear();
    std::vector<DeviceSummary> list;
    if (Error e = repo_->listDevices(list); !e) {
        lastError_ = errMessage(e);
        emit changed();
        return;
    }
    for (const DeviceSummary& d : list) {
        // A device the monitor has not seen yet appears Unknown-until-observed;
        // add() is idempotent so re-adds keep any observed state.
        health_->add(d.id);
        QVariantMap m;
        m.insert(QStringLiteral("id"), QString::fromStdString(d.id));
        m.insert(QStringLiteral("name"), QString::fromStdString(d.name));
        m.insert(QStringLiteral("address"), QString::fromStdString(d.address));
        m.insert(QStringLiteral("vendor"), QString::fromStdString(d.vendor));
        m.insert(QStringLiteral("kind"),
                 QString::fromStdString(d.kind.empty() ? std::string("camera")
                                                       : d.kind));
        m.insert(QStringLiteral("cameraCount"), d.cameraCount);
        m.insert(QStringLiteral("disabled"), d.disabled);
        m.insert(QStringLiteral("health"), healthMap(d.id));
        // The device's channels (increment 17): the channel-management panel
        // binds to this, so it refreshes with the model on every mutation.
        QVariantList channels;
        std::vector<ChannelSummary> chans;
        if (Error ce = repo_->listChannels(d.id, chans); ce) {
            for (const ChannelSummary& c : chans) {
                QVariantMap cm;
                cm.insert(QStringLiteral("id"), QString::fromStdString(c.id));
                cm.insert(QStringLiteral("name"), QString::fromStdString(c.name));
                cm.insert(QStringLiteral("mainUrl"), QString::fromStdString(c.mainUrl));
                cm.insert(QStringLiteral("subUrl"), QString::fromStdString(c.subUrl));
                cm.insert(QStringLiteral("disabled"), c.disabled);
                cm.insert(QStringLiteral("width"), c.mainWidth);
                cm.insert(QStringLiteral("height"), c.mainHeight);
                cm.insert(QStringLiteral("codec"), QString::fromStdString(c.mainCodec));
                channels.push_back(cm);
            }
        }
        m.insert(QStringLiteral("channels"), channels);
        devices_.push_back(m);
    }
    emit changed();
}

void DeviceController::refresh() { rebuild(); }

int DeviceController::attentionCount() const {
    int n = 0;
    for (const QVariant& v : devices_) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("health")).toMap()
                .value(QStringLiteral("needsAttention")).toBool())
            ++n;
    }
    return n;
}

QString DeviceController::onboard(const QString& id, const QString& name,
                                  const QString& address, const QString& vendor,
                                  const QString& user, const QString& password,
                                  const QString& mainUrl, const QString& subUrl) {
    if (id.trimmed().isEmpty() || name.trimmed().isEmpty()) {
        lastError_ = QStringLiteral("device id and name are required");
        emit changed();
        return lastError_;
    }

    DeviceOnboard d;
    d.id = id.trimmed().toStdString();
    d.name = name.trimmed().toStdString();
    d.address = address.trimmed().toStdString();
    d.vendor = vendor.trimmed().toStdString();
    d.kind = "camera";   // single-channel direct IP camera
    // "user:password" is the credential secret; stored only via the SecretStore
    // (never persisted in clear, never read back by this controller).
    d.credentialSecret = (user + QStringLiteral(":") + password).toStdString();

    // A single-channel direct camera (P2-01 one-channel direct-camera behavior):
    // the channel's stable id is the device id (one channel per direct device).
    CameraChannel c;
    c.id = d.id;
    c.name = d.name;
    c.mainUrl = mainUrl.trimmed().toStdString();
    c.subUrl = subUrl.trimmed().toStdString();
    d.cameras.push_back(c);

    const Error e = repo_->onboard(d);
    lastError_ = e ? QString() : errMessage(e);
    if (e) health_->add(d.id);   // track health for the newly onboarded device
    rebuild();
    return lastError_;
}

QString DeviceController::onboardRecorder(
    const QString& id, const QString& name, const QString& address,
    const QString& vendor, const QString& kind, const QString& user,
    const QString& password, int channelCount, const QString& mainTemplate,
    const QString& subTemplate) {
    if (id.trimmed().isEmpty() || name.trimmed().isEmpty()) {
        lastError_ = QStringLiteral("device id and name are required");
        emit changed();
        return lastError_;
    }
    if (channelCount < 1) {
        lastError_ = QStringLiteral("a recorder needs at least one channel");
        emit changed();
        return lastError_;
    }

    DeviceOnboard d;
    d.id = id.trimmed().toStdString();
    d.name = name.trimmed().toStdString();
    d.address = address.trimmed().toStdString();
    d.vendor = vendor.trimmed().toStdString();
    const QString k = kind.trimmed().isEmpty() ? QStringLiteral("nvr")
                                               : kind.trimmed();
    d.kind = k.toStdString();
    d.credentialSecret = (user + QStringLiteral(":") + password).toStdString();

    // Expand the per-channel RTSP templates: "{ch}" -> the 1-based channel
    // number. Channel ids are stable ("<id>-ch<N>") so a later channel sync
    // (or ONVIF re-scan) preserves them and the operator-customized names.
    const QString mainT = mainTemplate.trimmed();
    const QString subT = subTemplate.trimmed();
    for (int n = 1; n <= channelCount; ++n) {
        const QString num = QString::number(n);
        CameraChannel c;
        c.id = (id.trimmed() + QStringLiteral("-ch") + num).toStdString();
        c.name = (name.trimmed() + QStringLiteral(" — ch ") + num).toStdString();
        if (!mainT.isEmpty())
            c.mainUrl = QString(mainT).replace(QStringLiteral("{ch}"), num)
                            .toStdString();
        if (!subT.isEmpty())
            c.subUrl = QString(subT).replace(QStringLiteral("{ch}"), num)
                           .toStdString();
        d.cameras.push_back(c);
    }

    const Error e = repo_->onboard(d);
    lastError_ = e ? QString() : errMessage(e);
    if (e) health_->add(d.id);
    rebuild();
    return lastError_;
}

QString DeviceController::renameDevice(const QString& id, const QString& name) {
    if (name.trimmed().isEmpty()) {
        lastError_ = QStringLiteral("device name is required");
        emit changed();
        return lastError_;
    }
    const Error e =
        repo_->renameDevice(id.toStdString(), name.trimmed().toStdString());
    lastError_ = e ? QString() : errMessage(e);
    rebuild();
    return lastError_;
}

QString DeviceController::setDeviceDisabled(const QString& id, bool disabled) {
    const Error e =
        repo_->setDeviceDisabled(id.toStdString(), disabled);
    lastError_ = e ? QString() : errMessage(e);
    rebuild();
    return lastError_;
}

QString DeviceController::removeDevice(const QString& id) {
    const Error e = repo_->remove(id.toStdString());
    lastError_ = e ? QString() : errMessage(e);
    rebuild();
    return lastError_;
}

QString DeviceController::renameChannel(const QString& /*deviceId*/,
                                        const QString& cameraId,
                                        const QString& name) {
    const Error e =
        repo_->renameChannel(cameraId.toStdString(), name.trimmed().toStdString());
    lastError_ = e ? QString() : errMessage(e);
    rebuild();
    return lastError_;
}

QString DeviceController::setChannelDisabled(const QString& deviceId,
                                             const QString& cameraId,
                                             bool disabled) {
    const Error e = repo_->setChannelDisabled(deviceId.toStdString(),
                                              cameraId.toStdString(), disabled);
    lastError_ = e ? QString() : errMessage(e);
    rebuild();
    return lastError_;
}

QString DeviceController::removeChannel(const QString& deviceId,
                                        const QString& cameraId) {
    const Error e = repo_->removeChannel(deviceId.toStdString(),
                                         cameraId.toStdString());
    lastError_ = e ? QString() : errMessage(e);
    rebuild();
    return lastError_;
}

void DeviceController::acknowledge(const QString& id) {
    health_->acknowledge(id.toStdString());
    rebuild();
}

void DeviceController::setMaintenance(const QString& id, bool inMaintenance) {
    health_->setMaintenance(id.toStdString(), inMaintenance);
    rebuild();
}

// inc 27: after any health report, surface a derived-state boundary crossing
// as a normalized event (the alarm engine's first source). Only transitions
// emit — steady state is silence, so the feed cannot spam the rule engine.
void DeviceController::emitTransition(const std::string& deviceId,
                                      vms::health::State before) {
    using vms::health::State;
    const State after = health_->state(deviceId);
    if (after == before) return;
    const QString id = QString::fromStdString(deviceId);
    if (after == State::Offline)
        emit healthTransition(id, QStringLiteral("device-offline"),
                              QStringLiteral("device unreachable"));
    else if (after == State::Degraded)
        emit healthTransition(id, QStringLiteral("device-degraded"),
                              QStringLiteral("device degraded"));
    else if (after == State::Online &&
             (before == State::Offline || before == State::Degraded))
        emit healthTransition(id, QStringLiteral("device-recovered"),
                              QStringLiteral("device recovered"));
}

void DeviceController::reportReach(const QString& id, int level) {
    const vms::health::State before = health_->state(id.toStdString());
    health_->reportReachable(id.toStdString(), level != 0);
    emitTransition(id.toStdString(), before);
    rebuild();
}

void DeviceController::reportStream(const QString& id, int level) {
    Stream s = Stream::Unknown;
    switch (level) {
        case 0:  s = Stream::Failed;   break;
        case 1:  s = Stream::Degraded; break;
        case 2:  s = Stream::Ok;       break;
        default: s = Stream::Unknown;  break;
    }
    const vms::health::State before = health_->state(id.toStdString());
    health_->reportStream(id.toStdString(), s);
    emitTransition(id.toStdString(), before);
    rebuild();
}

void DeviceController::reportStorage(const QString& id, int level) {
    Storage s = Storage::Unknown;
    switch (level) {
        case 0:  s = Storage::Failed;        break;
        case 1:  s = Storage::Low;           break;
        case 2:  s = Storage::Ok;            break;
        case 3:  s = Storage::NotApplicable; break;
        default: s = Storage::Unknown;       break;
    }
    const vms::health::State before = health_->state(id.toStdString());
    health_->reportStorage(id.toStdString(), s);
    emitTransition(id.toStdString(), before);
    rebuild();
}

void DeviceController::setFirmware(const QString& id, const QString& firmware) {
    health_->setFirmware(id.toStdString(), firmware.toStdString());
    rebuild();
}

namespace {
// Split a device address into host + port, defaulting to RTSP 554. Only a
// trailing ":<digits>" is treated as a port (so a bare IPv4/host, or an IPv6
// literal with multiple colons, keeps the whole string as the host and 554).
void splitHostPort(const std::string& address, std::string& host, int& port) {
    host = address;
    port = 554;
    const auto pos = address.rfind(':');
    if (pos == std::string::npos || pos + 1 >= address.size()) return;
    const std::string tail = address.substr(pos + 1);
    if (address.find(':') != pos) return;   // more than one ':' -> not host:port
    for (char c : tail)
        if (!std::isdigit(static_cast<unsigned char>(c))) return;
    host = address.substr(0, pos);
    port = std::stoi(tail);
}
}  // namespace

void DeviceController::pollHealth() {
    if (!probe_) return;
    std::vector<DeviceSummary> list;
    if (Error e = repo_->listDevices(list); !e) return;
    for (const DeviceSummary& d : list) {
        // A detached device (P2-06) is not being managed — don't probe it, and
        // a device with no address can't be probed. Leave health as-is either
        // way (honest: no observation rather than a false Offline).
        if (d.disabled || d.address.empty()) continue;
        vms::health::ProbeTarget t;
        t.deviceId = d.id;
        splitHostPort(d.address, t.host, t.port);
        const vms::health::ProbeResult r = probe_->probe(t);
        // Reachable -> Online; unreachable -> Offline (raises an exception).
        // Stream is left as-is: reachability alone doesn't prove a stream.
        const vms::health::State before = health_->state(d.id);
        health_->reportReachable(d.id, r.reachable);
        emitTransition(d.id, before);   // inc 27: feed the alarm engine
    }
    rebuild();
}

// ---- ONVIF discovery-as-a-source (increment 16, P2-03/P2-14) ---------------

std::string DeviceController::deviceIdFromHost(const std::string& host) {
    std::string s = "onvif-";
    for (char ch : host) {
        // Keep the id filesystem/URL-safe and stable: letters/digits pass;
        // dots/colons/other separators collapse to a single dash.
        if (std::isalnum(static_cast<unsigned char>(ch)))
            s += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        else if (s.empty() || s.back() != '-')
            s += '-';
    }
    while (!s.empty() && s.back() == '-') s.pop_back();
    return s;
}

void DeviceController::chooseStreams(const OnvifDevice& dev, std::string& mainUrl,
                                     std::string& subUrl, MediaProfile* mainProf,
                                     MediaProfile* subProf) {
    mainUrl.clear();
    subUrl.clear();
    // A usable profile: a video encoding we handle (H.264/H.265) with a
    // non-empty RTSP URI (streamUris is parallel to profiles). JPEG-only or
    // URI-less profiles are ignored.
    struct Usable { long long px; std::string uri; MediaProfile prof; };
    std::vector<Usable> usable;
    const std::size_t n =
        std::min(dev.profiles.size(), dev.streamUris.size());
    for (std::size_t i = 0; i < n; ++i) {
        const MediaProfile& p = dev.profiles[i];
        const std::string& uri = dev.streamUris[i];
        if (uri.empty()) continue;
        if (p.encoding != "H264" && p.encoding != "H265") continue;
        usable.push_back({static_cast<long long>(p.width) * p.height, uri, p});
    }
    if (usable.empty()) return;
    // Main = largest resolution; Sub = smallest resolution distinct from Main.
    std::sort(usable.begin(), usable.end(),
              [](const Usable& a, const Usable& b) { return a.px > b.px; });
    mainUrl = usable.front().uri;
    if (mainProf) *mainProf = usable.front().prof;
    if (usable.size() > 1 && usable.back().uri != mainUrl) {
        subUrl = usable.back().uri;
        if (subProf) *subProf = usable.back().prof;
    }
}

void DeviceController::rebuildDiscovered() {
    discovered_.clear();
    // The set of hosts already onboarded, for duplicate detection (P2-03).
    std::vector<DeviceSummary> inventory;
    repo_->listDevices(inventory);
    for (const DiscoveryCandidate& c : candidates_) {
        bool onboarded = false;
        for (const DeviceSummary& d : inventory) {
            if (!c.host.empty() && d.address == c.host) { onboarded = true; break; }
        }
        QVariantMap m;
        m.insert(QStringLiteral("endpointRef"), QString::fromStdString(c.endpointRef));
        m.insert(QStringLiteral("name"),
                 QString::fromStdString(c.name.empty() ? c.hardware : c.name));
        m.insert(QStringLiteral("hardware"), QString::fromStdString(c.hardware));
        m.insert(QStringLiteral("host"), QString::fromStdString(c.host));
        m.insert(QStringLiteral("xaddr"), QString::fromStdString(c.xaddr));
        m.insert(QStringLiteral("alreadyOnboarded"), onboarded);
        discovered_.push_back(m);
    }
    emit discoveryChanged();
}

void DeviceController::startDiscovery(int timeoutMs) {
    if (!discovery_) {
        discoveryStatus_ = QStringLiteral("Discovery unavailable in this build.");
        emit discoveryChanged();
        return;
    }
    discovering_ = true;
    discoveryStatus_ = QStringLiteral("Scanning…");
    emit discoveryChanged();

    std::string error;
    candidates_ = discovery_->discover(timeoutMs, error);
    discovering_ = false;
    if (!error.empty()) {
        candidates_.clear();
        discoveryStatus_ = QStringLiteral("Discovery failed: ") +
                           QString::fromStdString(error);
        rebuildDiscovered();
        return;
    }
    discoveryStatus_ = candidates_.empty()
                           ? QStringLiteral("No ONVIF devices found on the LAN.")
                           : QStringLiteral("Found %1 device(s).")
                                 .arg(static_cast<int>(candidates_.size()));
    rebuildDiscovered();
}

QString DeviceController::onboardDiscovered(const QString& endpointRef,
                                            const QString& user,
                                            const QString& password) {
    if (!discovery_) {
        lastError_ = QStringLiteral("discovery unavailable");
        emit changed();
        return lastError_;
    }
    // Locate the candidate by its stable endpoint reference.
    const std::string epr = endpointRef.toStdString();
    const DiscoveryCandidate* cand = nullptr;
    for (const DiscoveryCandidate& c : candidates_)
        if (c.endpointRef == epr) { cand = &c; break; }
    if (!cand) {
        lastError_ = QStringLiteral("discovered device is no longer in the scan");
        emit changed();
        return lastError_;
    }

    // Fetch its ONVIF media config (info + profiles + RTSP stream URIs).
    OnvifDevice dev;
    std::string error;
    if (!discovery_->fetch(cand->xaddr, user.toStdString(),
                           password.toStdString(), dev, error)) {
        lastError_ = QStringLiteral("ONVIF fetch failed: ") +
                     QString::fromStdString(error);
        emit changed();
        return lastError_;
    }

    // Map profiles -> credential-free Main/Sub RTSP URLs (+ the chosen profiles,
    // for stream-profile sync below).
    std::string mainUrl, subUrl;
    MediaProfile mainProf, subProf;
    chooseStreams(dev, mainUrl, subUrl, &mainProf, &subProf);
    if (mainUrl.empty()) {
        lastError_ = QStringLiteral(
            "device reported no usable H.264/H.265 stream");
        emit changed();
        return lastError_;
    }

    // Onboard atomically as a single-channel direct camera. Honest fallbacks
    // for the display fields when a scope/info field is absent.
    DeviceOnboard d;
    d.id = deviceIdFromHost(cand->host.empty() ? cand->endpointRef : cand->host);
    d.name = !dev.info.model.empty() ? dev.info.model
             : !cand->name.empty()   ? cand->name
                                     : d.id;
    d.address = cand->host;
    d.vendor = !dev.info.manufacturer.empty() ? dev.info.manufacturer
                                              : cand->hardware;
    d.kind = "camera";
    d.credentialSecret =
        (user + QStringLiteral(":") + password).toStdString();
    CameraChannel c;
    c.id = d.id;
    c.name = d.name;
    c.mainUrl = mainUrl;
    c.subUrl = subUrl;
    d.cameras.push_back(c);

    const Error e = repo_->onboard(d);
    lastError_ = e ? QString() : errMessage(e);
    if (e) {
        health_->add(d.id);
        if (!dev.info.firmware.empty())
            health_->setFirmware(d.id, dev.info.firmware);
        // Sync the discovered stream profiles (resolution/codec) for the channel
        // (P2-05 stream-profile sync from a discovery source). Best-effort: an
        // onboarded device is not un-onboarded if only the profile write fails.
        repo_->setStreamProfile(c.id, "main", mainProf.width, mainProf.height,
                                mainProf.encoding);
        if (!subUrl.empty())
            repo_->setStreamProfile(c.id, "sub", subProf.width, subProf.height,
                                    subProf.encoding);
    }
    rebuild();            // inventory changed
    rebuildDiscovered();  // the just-onboarded candidate now shows as onboarded
    return lastError_;
}

QString DeviceController::rescanDevice(const QString& id, const QString& user,
                                       const QString& password) {
    if (!discovery_) {
        lastError_ = QStringLiteral("discovery unavailable");
        emit changed();
        return lastError_;
    }
    const std::string devId = id.toStdString();

    // The device's kind + address (rescan applies to a direct camera).
    std::vector<DeviceSummary> devs;
    repo_->listDevices(devs);
    const DeviceSummary* ds = nullptr;
    for (const DeviceSummary& d : devs)
        if (d.id == devId) { ds = &d; break; }
    if (!ds) {
        lastError_ = QStringLiteral("unknown device");
        emit changed();
        return lastError_;
    }
    if (ds->kind != "camera") {
        lastError_ = QStringLiteral(
            "rescan is for a direct camera — a recorder's channels come from its "
            "URL templates");
        emit changed();
        return lastError_;
    }
    if (ds->address.empty()) {
        lastError_ = QStringLiteral("device has no address to match a discovered device");
        emit changed();
        return lastError_;
    }

    // Match a discovered candidate (from the last scan) by host.
    const DiscoveryCandidate* cand = nullptr;
    for (const DiscoveryCandidate& c : candidates_)
        if (!c.host.empty() && c.host == ds->address) { cand = &c; break; }
    if (!cand) {
        lastError_ = QStringLiteral("no discovered device at ") +
                     QString::fromStdString(ds->address) +
                     QStringLiteral(" — run Discover first");
        emit changed();
        return lastError_;
    }

    // Fetch + map to Main/Sub (+ profiles for the stream-profile re-sync).
    OnvifDevice dev;
    std::string err;
    if (!discovery_->fetch(cand->xaddr, user.toStdString(),
                           password.toStdString(), dev, err)) {
        lastError_ = QStringLiteral("ONVIF fetch failed: ") +
                     QString::fromStdString(err);
        emit changed();
        return lastError_;
    }
    std::string mainUrl, subUrl;
    MediaProfile mainProf, subProf;
    chooseStreams(dev, mainUrl, subUrl, &mainProf, &subProf);
    if (mainUrl.empty()) {
        lastError_ = QStringLiteral("device reported no usable H.264/H.265 stream");
        emit changed();
        return lastError_;
    }

    // Reconcile the single channel — syncChannels preserves its stable id and
    // operator name, refreshing only the technical URLs.
    std::vector<ChannelSummary> chans;
    repo_->listChannels(devId, chans);
    if (chans.size() != 1) {
        lastError_ = QStringLiteral("rescan expects a single-channel camera");
        emit changed();
        return lastError_;
    }
    CameraChannel c;
    c.id = chans.front().id;
    c.name = chans.front().name;
    c.mainUrl = mainUrl;
    c.subUrl = subUrl;
    const Error e = repo_->syncChannels(devId, {c}, /*allowRemoval=*/false);
    if (e) {
        repo_->setStreamProfile(c.id, "main", mainProf.width, mainProf.height,
                                mainProf.encoding);
        if (!subUrl.empty())
            repo_->setStreamProfile(c.id, "sub", subProf.width, subProf.height,
                                    subProf.encoding);
    }
    lastError_ = e ? QString() : errMessage(e);
    rebuild();
    return lastError_;
}

QString DeviceController::moveChannel(const QString& deviceId,
                                      const QString& cameraId, bool up) {
    // Compute the new order from the current persisted order, then persist it.
    std::vector<ChannelSummary> chans;
    if (Error le = repo_->listChannels(deviceId.toStdString(), chans); !le) {
        lastError_ = errMessage(le);
        emit changed();
        return lastError_;
    }
    std::vector<std::string> ids;
    ids.reserve(chans.size());
    for (const ChannelSummary& c : chans) ids.push_back(c.id);
    const std::string cid = cameraId.toStdString();
    auto it = std::find(ids.begin(), ids.end(), cid);
    if (it == ids.end()) {
        lastError_ = QStringLiteral("unknown channel");
        emit changed();
        return lastError_;
    }
    const std::size_t i = static_cast<std::size_t>(it - ids.begin());
    if (up && i > 0)
        std::swap(ids[i], ids[i - 1]);
    else if (!up && i + 1 < ids.size())
        std::swap(ids[i], ids[i + 1]);
    else {
        lastError_.clear();   // already at the end — a no-op, not an error
        return lastError_;
    }
    const Error e = repo_->reorderChannels(deviceId.toStdString(), ids);
    lastError_ = e ? QString() : errMessage(e);
    rebuild();
    return lastError_;
}
