#pragma once

// Connection broker — native increment 6c (the single device-connection
// identity for the standalone runtime). Scope: ../credential-broker-P1-03-proposal.md.
//
// Given a camera_id + which stream is wanted, the broker is the ONE component
// that: (1) resolves the camera's device and credential-free URL template from
// the SQLite store, (2) resolves the device secret through the CredentialRepo /
// SecretStore, (3) materializes the credentialed connection URL IN MEMORY, and
// (4) hands the media layer a ready Session. Callers pass a camera_id and never
// a URL-with-password (acceptance §3). The broker also owns per-device
// connection pooling and the P0-03 login backoff / circuit-breaker, so a device
// is never hammered into a lockout (acceptance §6) and every failure is a typed
// error, never a silent retry or a false "connected" (acceptance §5).
//
// Pure and isolated like Store / Governor / SecretStore: no Qt, no GStreamer,
// no real socket. It emits a materialized URL string; the media layer builds the
// actual GStreamer pipeline from it. That keeps the broker unit-testable in
// isolation (vms_brokertest) while remaining the only URL-materializing path.

#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "persist/CredentialRepo.h"
#include "persist/Store.h"

namespace vms::broker {

using vms::persist::CredentialRepo;
using vms::persist::Error;
using vms::persist::Status;
using vms::persist::Store;

// Which physical stream a tile wants. The governor's Tier maps here at the call
// site (Main -> Main; Sub/Thumb -> Sub; Paused -> no connect), so the broker
// stays independent of the governor.
enum class StreamKind { Main, Sub };

const char* StreamKindName(StreamKind k);

// A ready connection. `url` is credential-bearing and lives only in memory for
// the lifetime of the session; it is NEVER logged and never persisted. `id` is
// the pool handle passed back to release().
struct Session {
    std::uint64_t id = 0;
    std::string cameraId;
    std::string deviceId;
    StreamKind kind = StreamKind::Main;
    std::string url;          // materialized in memory; do not log
    bool valid = false;
    explicit operator bool() const { return valid; }
};

struct BrokerConfig {
    int maxSessionsPerDevice = 4;   // pooling: refuse beyond the device's capacity
    int failureThreshold = 3;       // consecutive failures that trip the breaker
    std::int64_t openDurationMs = 30000;  // how long the breaker stays open
};

// Injectable monotonic clock (milliseconds) so the circuit-breaker is
// deterministic under test. Defaults to steady_clock.
using NowMsFn = std::function<std::int64_t()>;

class ConnectionBroker {
public:
    ConnectionBroker(Store& store, CredentialRepo& creds,
                     BrokerConfig cfg = {}, NowMsFn now = {});

    // Resolve + materialize a connection for `cameraId`'s `kind` stream.
    //  - Status::NotFound       : unknown camera, or no URL for that stream
    //  - Status::NotFound/Crypto: missing / undecryptable device secret
    //  - Status::Unavailable    : circuit open (device locked out) or pool full
    //  - Status::Misuse         : the stored URL already embeds credentials
    // On success `out` is a valid Session holding the in-memory credentialed URL.
    Error connect(const std::string& cameraId, StreamKind kind, Session& out);

    // Return a session to the pool (decrements the device's active count).
    void release(const Session& s);

    // The media layer reports the outcome of using a session so the breaker can
    // learn. Success closes the breaker; repeated failures open it.
    void reportSuccess(const std::string& cameraId);
    void reportFailure(const std::string& cameraId);

    // Introspection for tests / diagnostics.
    int activeSessions(const std::string& deviceId) const;
    bool circuitOpen(const std::string& deviceId) const;

    // Materialize a credentialed URL from a credential-free template and a
    // "user:password" secret, percent-encoding the userinfo. Exposed (static) so
    // the self-check can assert it directly. Returns Misuse if `templateUrl`
    // already contains an '@' userinfo or has no "scheme://".
    static Error materializeUrl(const std::string& templateUrl,
                                const std::string& secret, std::string& out);

private:
    struct DeviceState {
        int active = 0;
        int failures = 0;
        std::int64_t openUntilMs = 0;
    };

    // camera_id -> (device_id, url template for `kind`). Typed NotFound if the
    // camera is unknown or has no URL for that stream.
    Error resolveCamera(const std::string& cameraId, StreamKind kind,
                        std::string& deviceId, std::string& templateUrl);
    Error deviceForCamera(const std::string& cameraId, std::string& deviceId);

    Store& store_;
    CredentialRepo& creds_;
    BrokerConfig cfg_;
    NowMsFn now_;
    std::map<std::string, DeviceState> devices_;  // keyed by device_id
    std::uint64_t nextSessionId_ = 1;
};

} // namespace vms::broker
