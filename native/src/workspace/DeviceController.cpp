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
                                   DiscoverySource* discovery, QObject* parent)
    : QObject(parent), repo_(repo), health_(health), discovery_(discovery) {
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
        m.insert(QStringLiteral("health"), healthMap(d.id));
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

QString DeviceController::removeDevice(const QString& id) {
    const Error e = repo_->remove(id.toStdString());
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

void DeviceController::reportReach(const QString& id, int level) {
    health_->reportReachable(id.toStdString(), level != 0);
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
    health_->reportStream(id.toStdString(), s);
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
    health_->reportStorage(id.toStdString(), s);
    rebuild();
}

void DeviceController::setFirmware(const QString& id, const QString& firmware) {
    health_->setFirmware(id.toStdString(), firmware.toStdString());
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
                                     std::string& subUrl) {
    mainUrl.clear();
    subUrl.clear();
    // A usable profile: a video encoding we handle (H.264/H.265) with a
    // non-empty RTSP URI (streamUris is parallel to profiles). JPEG-only or
    // URI-less profiles are ignored.
    struct Usable { long long px; std::string uri; };
    std::vector<Usable> usable;
    const std::size_t n =
        std::min(dev.profiles.size(), dev.streamUris.size());
    for (std::size_t i = 0; i < n; ++i) {
        const MediaProfile& p = dev.profiles[i];
        const std::string& uri = dev.streamUris[i];
        if (uri.empty()) continue;
        if (p.encoding != "H264" && p.encoding != "H265") continue;
        usable.push_back({static_cast<long long>(p.width) * p.height, uri});
    }
    if (usable.empty()) return;
    // Main = largest resolution; Sub = smallest resolution distinct from Main.
    std::sort(usable.begin(), usable.end(),
              [](const Usable& a, const Usable& b) { return a.px > b.px; });
    mainUrl = usable.front().uri;
    if (usable.size() > 1 && usable.back().uri != mainUrl)
        subUrl = usable.back().uri;
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

    // Map profiles -> credential-free Main/Sub RTSP URLs.
    std::string mainUrl, subUrl;
    chooseStreams(dev, mainUrl, subUrl);
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
    }
    rebuild();            // inventory changed
    rebuildDiscovered();  // the just-onboarded candidate now shows as onboarded
    return lastError_;
}
