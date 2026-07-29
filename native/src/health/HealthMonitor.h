#pragma once

// Device health & lifecycle — native increment 13 (P2-13). Scope:
// ../Development_plan.md (Approval Log "Device health & lifecycle").
//
// A pure, OS-free decision core that tracks each managed device's health and
// turns it into an honest operator-facing state + exception. Following the P0-03
// rule, the dimensions are kept SEPARATE (reachability / stream / storage) and
// only combined into an overall state at the edge — a device is never claimed
// healthy without evidence (Unknown until observed). Exceptions have a first-seen
// time, can be acknowledged (silences the alert but the exception persists until
// the condition recovers), auto-clear on recovery, and are suppressed while the
// device is in a maintenance window. Fed by the ConnectionBroker's connect
// outcomes now and by ONVIF / live polling later; injectable clock for tests.

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace vms::health {

enum class Reach { Unknown, Online, Offline };
enum class Stream { Unknown, Ok, Degraded, Failed };
enum class Storage { Unknown, NotApplicable, Ok, Low, Failed };

// The overall operator-facing state derived from the dimensions above.
enum class State { Unknown, Online, Degraded, Offline, Unsupported };
const char* StateName(State s);

struct DeviceHealth {
    std::string deviceId;
    Reach reach = Reach::Unknown;
    Stream stream = Stream::Unknown;
    Storage storage = Storage::Unknown;
    bool supported = true;
    std::string firmware;

    State state = State::Unknown;          // derived
    bool exceptionActive = false;
    bool exceptionAcknowledged = false;
    std::string exceptionReason;
    std::int64_t exceptionSinceMs = 0;
    bool inMaintenance = false;
};

using NowMsFn = std::function<std::int64_t()>;

class HealthMonitor {
public:
    explicit HealthMonitor(NowMsFn now = {});

    void add(const std::string& deviceId);            // Unknown until observed
    void setSupported(const std::string& id, bool supported);
    void setFirmware(const std::string& id, const std::string& firmware);
    void setMaintenance(const std::string& id, bool inMaintenance);

    void reportReachable(const std::string& id, bool online);
    void reportStream(const std::string& id, Stream s);
    void reportStorage(const std::string& id, Storage s);

    // Acknowledge the active exception on a device: silences its alert but keeps
    // the exception until the underlying condition recovers.
    void acknowledge(const std::string& id);

    DeviceHealth get(const std::string& id) const;
    State state(const std::string& id) const;
    std::size_t size() const { return devices_.size(); }

    // True when the device has an active, unacknowledged exception and is not in
    // a maintenance window — i.e. the operator should be alerted.
    bool shouldNotify(const std::string& id) const;

    // Devices that shouldNotify(), sorted by id — the alerting work-list.
    std::vector<std::string> needsAttention() const;

private:
    DeviceHealth* find(const std::string& id);
    const DeviceHealth* find(const std::string& id) const;
    void reevaluate(DeviceHealth& d);   // recompute state + exception lifecycle

    NowMsFn now_;
    std::map<std::string, DeviceHealth> devices_;
};

} // namespace vms::health
