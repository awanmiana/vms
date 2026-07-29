#include "health/HealthMonitor.h"

#include <chrono>

namespace vms::health {

const char* StateName(State s) {
    switch (s) {
        case State::Unknown:     return "unknown";
        case State::Online:      return "online";
        case State::Degraded:    return "degraded";
        case State::Offline:     return "offline";
        case State::Unsupported: return "unsupported";
    }
    return "?";
}

namespace {
std::int64_t steadyNowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Derive the overall state from the separate dimensions (honest: no evidence ->
// Unknown; a subsystem failure on an online device is Degraded, not "online").
State deriveState(const DeviceHealth& d) {
    if (!d.supported) return State::Unsupported;
    if (d.reach == Reach::Offline) return State::Offline;
    if (d.reach == Reach::Unknown) return State::Unknown;
    // reach == Online:
    if (d.stream == Stream::Failed || d.storage == Storage::Failed ||
        d.stream == Stream::Degraded || d.storage == Storage::Low)
        return State::Degraded;
    return State::Online;
}

// The worst-dimension reason for the current bad state (for the exception).
std::string reasonFor(const DeviceHealth& d) {
    if (!d.supported) return "device model not supported";
    if (d.reach == Reach::Offline) return "device offline";
    if (d.stream == Stream::Failed) return "stream failed";
    if (d.storage == Storage::Failed) return "storage failed";
    if (d.stream == Stream::Degraded) return "stream degraded";
    if (d.storage == Storage::Low) return "storage low";
    return "";
}
}  // namespace

HealthMonitor::HealthMonitor(NowMsFn now) : now_(std::move(now)) {
    if (!now_) now_ = &steadyNowMs;
}

DeviceHealth* HealthMonitor::find(const std::string& id) {
    auto it = devices_.find(id);
    return it == devices_.end() ? nullptr : &it->second;
}
const DeviceHealth* HealthMonitor::find(const std::string& id) const {
    auto it = devices_.find(id);
    return it == devices_.end() ? nullptr : &it->second;
}

void HealthMonitor::add(const std::string& id) {
    if (id.empty() || devices_.count(id)) return;
    DeviceHealth d;
    d.deviceId = id;
    devices_.emplace(id, d);   // all dimensions Unknown, state Unknown
}

void HealthMonitor::reevaluate(DeviceHealth& d) {
    d.state = deriveState(d);
    // Offline and Degraded (and an Unsupported device) are exception-worthy;
    // Unknown and Online are not.
    const bool bad = d.state == State::Offline || d.state == State::Degraded ||
                     d.state == State::Unsupported;
    if (bad) {
        if (!d.exceptionActive) {
            d.exceptionActive = true;
            d.exceptionAcknowledged = false;
            d.exceptionSinceMs = now_();
        }
        d.exceptionReason = reasonFor(d);   // keep the reason current
    } else if (d.exceptionActive) {
        // Recovered: auto-clear the exception (and its acknowledgement).
        d.exceptionActive = false;
        d.exceptionAcknowledged = false;
        d.exceptionReason.clear();
        d.exceptionSinceMs = 0;
    }
}

void HealthMonitor::setSupported(const std::string& id, bool supported) {
    if (DeviceHealth* d = find(id)) { d->supported = supported; reevaluate(*d); }
}
void HealthMonitor::setFirmware(const std::string& id, const std::string& fw) {
    if (DeviceHealth* d = find(id)) d->firmware = fw;   // inventory only; no state change
}
void HealthMonitor::setMaintenance(const std::string& id, bool m) {
    if (DeviceHealth* d = find(id)) d->inMaintenance = m;   // suppresses notify only
}
void HealthMonitor::reportReachable(const std::string& id, bool online) {
    if (DeviceHealth* d = find(id)) {
        d->reach = online ? Reach::Online : Reach::Offline;
        reevaluate(*d);
    }
}
void HealthMonitor::reportStream(const std::string& id, Stream s) {
    if (DeviceHealth* d = find(id)) { d->stream = s; reevaluate(*d); }
}
void HealthMonitor::reportStorage(const std::string& id, Storage s) {
    if (DeviceHealth* d = find(id)) { d->storage = s; reevaluate(*d); }
}

void HealthMonitor::acknowledge(const std::string& id) {
    if (DeviceHealth* d = find(id))
        if (d->exceptionActive) d->exceptionAcknowledged = true;
}

DeviceHealth HealthMonitor::get(const std::string& id) const {
    const DeviceHealth* d = find(id);
    return d ? *d : DeviceHealth{};
}
State HealthMonitor::state(const std::string& id) const {
    const DeviceHealth* d = find(id);
    return d ? d->state : State::Unknown;
}

bool HealthMonitor::shouldNotify(const std::string& id) const {
    const DeviceHealth* d = find(id);
    return d && d->exceptionActive && !d->exceptionAcknowledged && !d->inMaintenance;
}

std::vector<std::string> HealthMonitor::needsAttention() const {
    std::vector<std::string> out;
    for (const auto& [id, d] : devices_)
        if (d.exceptionActive && !d.exceptionAcknowledged && !d.inMaintenance)
            out.push_back(id);   // std::map iterates sorted by id
    return out;
}

} // namespace vms::health
