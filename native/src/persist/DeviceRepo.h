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
    // Onboarding workflow: "camera" (single-channel direct IP camera) or a
    // recorder ("nvr" / "dvr" / "hybrid") with many channels. Empty defaults to
    // "camera" (the v1 direct-camera behavior, preserved).
    std::string kind;
    std::string credentialSecret;   // "user:password"; stored via SecretStore only
    std::vector<CameraChannel> cameras;
};

// A read-only summary of a persisted device for the inventory/onboarding UI:
// the credential-free device metadata plus how many camera channels it owns.
// The secret is never included — it lives only in the SecretStore.
struct DeviceSummary {
    std::string id;
    std::string name;
    std::string address;
    std::string vendor;
    std::string kind;       // "camera" | "nvr" | "dvr" | "hybrid"
    int cameraCount = 0;
};

// A read-only summary of one camera channel for the channel-management UI
// (P2-05). `disabled` channels stay in inventory but are excluded from the
// default group (active use). `mainWidth`/`mainHeight`/`mainCodec` are the
// synced main stream profile (0/"" when never synced from a discovery source).
// Credential-free.
struct ChannelSummary {
    std::string id;
    std::string name;
    std::string mainUrl;
    std::string subUrl;
    bool disabled = false;
    int mainWidth = 0;
    int mainHeight = 0;
    std::string mainCodec;
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

    // Rename a device (P2-06): updates the device's operator-facing name and
    // refreshes its default group's display name, while PRESERVING every
    // association — camera channels + their stable ids and operator names, group
    // membership, and the credential are untouched. Honest NotFound on a bad id.
    Error renameDevice(const std::string& deviceId, const std::string& newName);

    // Remove a device and everything owned by it (cameras, group, credential).
    Error remove(const std::string& deviceId);

    // The device's current camera ids (stable order by id).
    Error cameraIds(const std::string& deviceId, std::vector<std::string>& out);

    // Every onboarded device (credential-free summaries, ordered by id) with its
    // camera-channel count — the enumeration the onboarding UI lists. Read-only.
    Error listDevices(std::vector<DeviceSummary>& out);

    // A device's camera channels (stable order by id), for the channel-
    // management UI (P2-05). Read-only; credential-free.
    Error listChannels(const std::string& deviceId,
                       std::vector<ChannelSummary>& out);

    // Rename a single channel's operator-facing name (P2-05). The stable id and
    // technical URLs are untouched.
    Error renameChannel(const std::string& cameraId, const std::string& name);

    // Enable/disable a single channel (P2-05). A disabled channel stays in
    // inventory but leaves the default group (excluded from active use); the
    // group is reconciled accordingly.
    Error setChannelDisabled(const std::string& deviceId,
                             const std::string& cameraId, bool disabled);

    // Remove a single channel from a device and reconcile the default group.
    // (Removing the whole device is remove().)
    Error removeChannel(const std::string& deviceId, const std::string& cameraId);

    // Reorder a device's channels (P2-05): `orderedIds` gives the desired order;
    // each listed channel's sort_order is written to its position. Ids not
    // belonging to the device are ignored. listChannels then returns this order.
    Error reorderChannels(const std::string& deviceId,
                          const std::vector<std::string>& orderedIds);

    // Upsert a channel's stream profile for a tier ("main"/"sub"), as synced
    // from a discovery source (ONVIF GetProfiles). width/height in pixels, codec
    // e.g. "H264"/"H265". Idempotent per (cameraId, tier).
    Error setStreamProfile(const std::string& cameraId, const std::string& tier,
                           int width, int height, const std::string& codec);

    // The default device group id for a device (P2-02).
    static std::string defaultGroupId(const std::string& deviceId);

private:
    // Reconcile the default group's membership to the device's ENABLED channels
    // (P2-02 + P2-05). Queries the current camera rows, so callers just write
    // the camera rows first and call this.
    Error reconcileGroup(const std::string& deviceId, const std::string& deviceName);

    Store& store_;
    SecretStore& secrets_;
};

} // namespace vms::persist
