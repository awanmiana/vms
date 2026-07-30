#pragma once

// Onboarding UI — native increment 14 (P2-01 / P2-02 / P2-05 / P2-13, the
// operator-facing device-management surface). Scope: ../Development_plan.md
// (Approval Log "Device onboarding" — the onboarding UI remainder) + owner item
// #3 (ONVIF ✅ → device health ✅ → onboarding UI ⬅ this).
//
// The bridge that turns the two pure inventory/health cores into a Qt Quick
// device-management view, exactly as PlaybackController turns the SegmentIndex
// into the Playback timeline model:
//   - DeviceRepo (increment 10) is the persisted inventory authority — atomic
//     onboard, idempotent channel sync, remove — over the P0-04 store + the
//     SecretStore (credential via the broker's credential_ref scheme).
//   - HealthMonitor (increment 13) is the honest per-device health/lifecycle
//     core — separate reach/stream/storage dimensions, Unknown-until-observed,
//     exception raise → acknowledge → auto-clear, maintenance windows.
//
// This controller owns a HealthMonitor (its state must outlive a refresh) and
// references a DeviceRepo. It exposes a `devices` model (each device with its
// credential-free metadata AND its honest health) plus invokable onboard /
// remove / acknowledge / maintenance actions to QML. The credential secret is
// only ever passed INTO onboard(); it is never read back or exposed.
//
// The health feed is honestly Unknown until observed: real observations come
// from the ConnectionBroker's connect outcomes and ONVIF polling, which is the
// live-blocked remainder. The reportReach/Stream/Storage methods are the wiring
// point for that feed and are exercised by --devices-selftest and --devices-demo.
//
// Qt + vms_persist (DeviceRepo) + vms_health (HealthMonitor); verified headlessly
// via --devices-selftest and offscreen --smoke-ms (QML bindings), on-screen
// windowed on the owner's box.

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "health/HealthMonitor.h"
#include "onvif/DiscoverySource.h"  // DiscoverySource + DiscoveryCandidate + OnvifDevice
#include "persist/DeviceRepo.h"

class DeviceController : public QObject {
    Q_OBJECT
    // The device inventory model the QML list repeats over. Each entry is a
    // QVariantMap { id, name, address, vendor, cameraCount, health{...} } where
    // health carries the honest derived state + exception lifecycle. Rebuilt on
    // every onboard / remove / acknowledge / health report.
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY changed)
    // Count of devices that shouldNotify() — an active, unacknowledged, non-
    // maintenance exception. Drives the "N need attention" header badge.
    Q_PROPERTY(int attentionCount READ attentionCount NOTIFY changed)
    // The last onboard/remove result, "" on success or a human error message.
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)

    // --- ONVIF discovery-as-a-source (increment 16, P2-03/P2-14) ---
    // The candidates found by the last LAN scan. Each is a QVariantMap
    // { endpointRef, name, hardware, host, xaddr, alreadyOnboarded } where
    // alreadyOnboarded is true when a device at that host is already in the
    // inventory (P2-03 duplicate detection). Rebuilt by startDiscovery().
    Q_PROPERTY(QVariantList discoveredDevices READ discoveredDevices NOTIFY discoveryChanged)
    // Whether a DiscoverySource is wired at all — QML hides the Discover panel
    // when discovery is unavailable (e.g. a build without the ONVIF transports).
    Q_PROPERTY(bool discoveryAvailable READ discoveryAvailable CONSTANT)
    // True while a scan is running (drives the "Scanning…" affordance).
    Q_PROPERTY(bool discovering READ discovering NOTIFY discoveryChanged)
    // Honest status line: "", "Scanning…", "Found N device(s)", "No devices
    // found", or an error / "unavailable" message.
    Q_PROPERTY(QString discoveryStatus READ discoveryStatus NOTIFY discoveryChanged)

public:
    // `repo`, `health`, and `discovery` are owned by the caller (main) and must
    // outlive this. `discovery` may be null (discovery then reports unavailable).
    DeviceController(vms::persist::DeviceRepo* repo,
                     vms::health::HealthMonitor* health,
                     vms::onvif::DiscoverySource* discovery = nullptr,
                     QObject* parent = nullptr);

    QVariantList devices() const { return devices_; }
    int deviceCount() const { return static_cast<int>(devices_.size()); }
    int attentionCount() const;
    QString lastError() const { return lastError_; }

    QVariantList discoveredDevices() const { return discovered_; }
    bool discoveryAvailable() const { return discovery_ != nullptr; }
    bool discovering() const { return discovering_; }
    QString discoveryStatus() const { return discoveryStatus_; }

    // Re-read the inventory from the repo and rebuild the model (adding any new
    // device to the health monitor as Unknown-until-observed). Idempotent.
    Q_INVOKABLE void refresh();

    // Onboard (or re-onboard) a single-channel direct IP camera (kind "camera")
    // from the form fields. `user`/`password` become the "user:password"
    // credential secret (stored only via the SecretStore). A blank subUrl means
    // no sub stream. Returns "" on success, or a human-readable error; also sets
    // lastError.
    Q_INVOKABLE QString onboard(const QString& id, const QString& name,
                                const QString& address, const QString& vendor,
                                const QString& user, const QString& password,
                                const QString& mainUrl, const QString& subUrl);

    // Onboard (or re-onboard) a multi-channel recorder (kind "nvr"/"dvr"/
    // "hybrid") atomically (P2-01): `channelCount` channels are generated by
    // expanding the RTSP URL templates — each occurrence of "{ch}" becomes the
    // 1-based channel number — and onboarded together with the device, its
    // default group, and its credential. A blank subTemplate means no sub
    // streams. Channel ids are stable ("<id>-ch<N>") so a later channel sync
    // preserves them. Returns "" on success, or a human-readable error.
    Q_INVOKABLE QString onboardRecorder(const QString& id, const QString& name,
                                        const QString& address, const QString& vendor,
                                        const QString& kind,
                                        const QString& user, const QString& password,
                                        int channelCount,
                                        const QString& mainTemplate,
                                        const QString& subTemplate);

    // Remove a device and everything it owns (cameras, group, credential, and
    // its health record). Returns "" on success or a human-readable error.
    Q_INVOKABLE QString removeDevice(const QString& id);

    // Acknowledge the device's active exception (silences the alert; the
    // exception persists until the condition recovers).
    Q_INVOKABLE void acknowledge(const QString& id);

    // Put a device in / out of a maintenance window (suppresses the alert, not
    // the underlying state).
    Q_INVOKABLE void setMaintenance(const QString& id, bool inMaintenance);

    // Live-feed wiring points (the broker/ONVIF observations drive these later;
    // used now by --devices-selftest and --devices-demo). level codes keep QML
    // free of the C++ enums: reach 0=Offline 1=Online; stream 0=Failed 1=Degraded
    // 2=Ok; storage 0=Failed 1=Low 2=Ok 3=N/A.
    Q_INVOKABLE void reportReach(const QString& id, int level);
    Q_INVOKABLE void reportStream(const QString& id, int level);
    Q_INVOKABLE void reportStorage(const QString& id, int level);
    Q_INVOKABLE void setFirmware(const QString& id, const QString& firmware);

    // Scan the LAN for ONVIF devices (blocking up to `timeoutMs`) and rebuild
    // the discoveredDevices model, each candidate flagged alreadyOnboarded
    // against the current inventory (P2-03). A no-op reporting "unavailable"
    // when no DiscoverySource is wired.
    Q_INVOKABLE void startDiscovery(int timeoutMs = 3000);

    // Onboard a discovered candidate (by endpointRef): fetch its ONVIF media
    // profiles + RTSP stream URIs, map the highest-resolution stream to Main and
    // a lower one to Sub, and onboard it atomically via DeviceRepo (kind
    // "camera", one channel). `user`/`password` authenticate the ONVIF fetch AND
    // become the stored credential secret. Returns "" on success or a human
    // error; also sets lastError.
    Q_INVOKABLE QString onboardDiscovered(const QString& endpointRef,
                                          const QString& user,
                                          const QString& password);

    // Pure mapping (no I/O): choose the Main + Sub credential-free RTSP URLs from
    // a fetched device's profiles/stream URIs — Main = the largest-resolution
    // H.264/H.265 profile with a stream URI, Sub = a smaller one, if any. Static
    // + exercised by --devices-selftest. `mainUrl` is empty when nothing usable.
    static void chooseStreams(const vms::onvif::OnvifDevice& dev,
                              std::string& mainUrl, std::string& subUrl);

    // Pure: a stable, sanitized device id derived from a candidate's host
    // ("192.168.0.254" -> "onvif-192-168-0-254"). Static + testable.
    static std::string deviceIdFromHost(const std::string& host);

signals:
    void changed();
    void discoveryChanged();

private:
    QVariantMap healthMap(const std::string& deviceId) const;
    void rebuild();
    void rebuildDiscovered();

    vms::persist::DeviceRepo* repo_ = nullptr;
    vms::health::HealthMonitor* health_ = nullptr;
    vms::onvif::DiscoverySource* discovery_ = nullptr;
    QVariantList devices_;
    QString lastError_;

    // Discovery state: the raw candidates from the last scan (endpointRef ->
    // candidate) plus the UI-facing model rebuilt over them + the inventory.
    std::vector<vms::onvif::DiscoveryCandidate> candidates_;
    QVariantList discovered_;
    bool discovering_ = false;
    QString discoveryStatus_;
};
