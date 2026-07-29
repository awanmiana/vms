#pragma once

// System health probe — native increment 8 (optimization service). Scope:
// ../optimization-service-P12-03-proposal.md.
//
// Samples the LIVE machine headroom the governor's static startup profile cannot
// see: total/available RAM, logical cores, and current system CPU load. The
// Optimizer maps these onto the governor's capacity so the wall degrades BEFORE
// the machine is exhausted (not just at the static ceiling). Qt-free and
// swappable (Windows now; an injectable fake for tests, Linux later) exactly like
// SecretStore / Store. All sampling is cheap syscalls; it must never itself
// become the bottleneck.

#include <string>

namespace vms::optimize {

struct SystemSample {
    bool valid = false;
    double totalRamMb = 0.0;
    double availRamMb = 0.0;
    unsigned int logicalCores = 0;
    double systemCpuLoadPct = -1.0;   // -1 = unknown (e.g. first sample, no delta)
    // Estimated usable network link budget in kbps for streaming; -1 = unmetered /
    // unknown (operator-configured or measured later — not probed here yet).
    double estBandwidthKbps = -1.0;
};

class SystemHealthProbe {
public:
    virtual ~SystemHealthProbe() = default;
    virtual SystemSample sample() = 0;
};

// Deterministic test double: returns exactly what was set. Lets the self-check
// script headroom without touching the real OS.
class FakeSystemHealthProbe : public SystemHealthProbe {
public:
    void set(const SystemSample& s) { s_ = s; }
    SystemSample sample() override { return s_; }
private:
    SystemSample s_;
};

#ifdef _WIN32
// Windows implementation: GlobalMemoryStatusEx for RAM, GetSystemTimes deltas
// across successive calls for system CPU load. The first sample reports CPU load
// as unknown (-1) because a delta needs two readings.
class WindowsSystemHealthProbe : public SystemHealthProbe {
public:
    SystemSample sample() override;
private:
    unsigned long long lastIdle_ = 0, lastKernel_ = 0, lastUser_ = 0;
    bool havePrev_ = false;
};
#endif

} // namespace vms::optimize
