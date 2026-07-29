#pragma once

// Device onboarding — native increment 10 (P2-01 / P2-02 / P2-05). Scope:
// ../Development_plan.md (Approval Log "Device onboarding").
//
// The inventory layer that turns "a recorder / camera the operator wants managed"
// into persisted rows the rest of the runtime already knows how to use: the
// governor decodes them, the ConnectionBroker connects to them, the workspace
// shows them. It is standards-correct (stable ids, idempotent channel sync,
// operator metadata preserved, credentials via the SecretStore under the SAME
// credential_ref scheme the broker resolves) — NOT a protocol mock. Real ONVIF /
// LAN discovery (P2-03/P2-14) will later feed this repo as a data source.
//
// Built on the P0-04 Store (declared schema: devices, cameras, camera_groups,
// camera_group_members, device_credentials) behind the C0-03 contract, so onboard
// is atomic (all-or-nothing) and failures are typed. Qt-free and unit-testable.

#include <string>
#include <vector>

#include "persist/CredentialRepo.h"
#include "persist/SecretStore.h"
#include "persist/Store.h"

namespace vms::persist {

// One camera channel on a device. `id` is the STABLE camera id (preserved across
// re-onboard / channel sync). URL templates are credential-free — the broker
// materializes credentials from the SecretStore at connect time.
struct CameraChannel {
    std::string id;
    std::string name;
    std::string mainUrl;
    std::string subUrl;   // optional
};

// A device the operator is onboarding, with its channels and its credential.
struct DeviceOnboard {
    std::string id;
    std::string name;
    std::string address;   // host / IP (credential-free)
    std::string vendor;
    std::string credentialSecret;   // "user:password"; stored via SecretStore only
    std::vector<CameraChannel> cameras;
};

class DeviceRepo {
public:
    DeviceRepo(Store& store, SecretStore& secrets)
        : store_(store), secrets_(secrets) {}

    // Onboard (or re-onboard) a device atomically: the device row, its camera
    // channels, its default group, and its credential are all written or none are
    // (P2-01). Re-onboarding the same id updates it idempotently. The credential
    // is stored only in the SecretStore, keyed by the broker's credential_ref.
    Error onboard(const DeviceOnboard& device);

    // Idempotently reconcile a device's channels to `cameras` (P2-05): new ids are
    // added, existing ids keep their operator-customized name (only the technical
    // URL templates are refreshed), and — when allowRemoval is true — ids no longer
    // present are removed. Stable camera ids are preserved throughout. The default
    // group is reconciled to match. Does NOT touch the credential.
    Error syncChannels(const std::string& deviceId,
                       const std::vector<CameraChannel>& cameras,
                       bool allowRemoval);

    // Remove a device and everything owned by it (cameras, group, credential).
    Error remove(const std::string& deviceId);

    // The device's current camera ids (stable order by id).
    Error cameraIds(const std::string& deviceId, std::vector<std::string>& out);

    // The default device group id for a device (P2-02).
    static std::string defaultGroupId(const std::string& deviceId);

private:
    // Reconcile the default group's membership to exactly `cameraIds` (P2-02).
    Error reconcileGroup(const std::string& deviceId, const std::string& deviceName,
                         const std::vector<std::string>& cameraIds);

    Store& store_;
    SecretStore& secrets_;
};

} // namespace vms::persist
