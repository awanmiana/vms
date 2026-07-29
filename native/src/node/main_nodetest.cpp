// Node-connectivity resilience self-check — native increment 9. No sockets, no
// Qt. Drives the NodeRegistry with a deterministic fake clock to prove failover
// across the software's own peer/AI-agent endpoints: preference ordering, fail
// over on circuit-open, recover on success and after the cooldown, honest health
// states, and an honest Unavailable when everything is down. Registered with
// CTest as `node_selfcheck`. NOT camera connectivity (that is the broker).

#include "node/NodeRegistry.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

using namespace vms::node;

namespace {
int failures = 0;
void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}
}  // namespace

int main() {
    std::cout << "vms_nodetest — node-connectivity resilience self-check\n";

    std::int64_t clockMs = 100000;
    NowMsFn clock = [&clockMs]() { return clockMs; };
    FailoverConfig cfg;
    cfg.failureThreshold = 3;
    cfg.openDurationMs = 1000;
    NodeRegistry reg(cfg, clock);

    // Registry empty -> honest Unavailable.
    check(!reg.select() && !reg.select().message.empty(),
          "empty registry selects nothing (honest Unavailable)");

    // Three endpoints for one logical service, preference local -> LAN -> internet.
    check(reg.add({"local", "127.0.0.1", 9000, Scope::Local}), "add local endpoint");
    check(reg.add({"lan", "192.168.1.10", 9000, Scope::Lan}), "add LAN endpoint");
    check(reg.add({"cloud", "node.example.net", 443, Scope::Internet}), "add internet endpoint");
    check(!reg.add({"local", "x", 1, Scope::Local}), "duplicate id is rejected");
    check(!reg.add({"", "x", 1, Scope::Local}), "empty id is rejected");
    check(reg.size() == 3, "three endpoints registered");

    // Fresh: prefer the local endpoint.
    check(reg.select() && reg.select().endpoint.id == "local",
          "select prefers the local endpoint");
    check(reg.health("local") == Health::Unknown, "health is Unknown before any report");

    // Local starts failing: 1 failure degrades but is still usable.
    reg.reportFailure("local");
    check(reg.health("local") == Health::Degraded, "one failure -> Degraded");
    check(reg.select().endpoint.id == "local", "a Degraded endpoint is still selected");

    // Local trips its breaker -> fail over to LAN.
    reg.reportFailure("local");
    reg.reportFailure("local");
    check(reg.circuitOpen("local") && reg.health("local") == Health::Down,
          "local trips the breaker at the threshold (Down)");
    check(reg.select() && reg.select().endpoint.id == "lan",
          "failover: select moves to the LAN endpoint");

    // LAN also down -> fail over to internet.
    reg.reportFailure("lan");
    reg.reportFailure("lan");
    reg.reportFailure("lan");
    check(reg.select() && reg.select().endpoint.id == "cloud",
          "failover: select moves to the internet endpoint");

    // Everything down -> honest Unavailable.
    reg.reportFailure("cloud");
    reg.reportFailure("cloud");
    reg.reportFailure("cloud");
    check(!reg.select(), "all endpoints down -> honest Unavailable");

    // A success on the local endpoint recovers it and it is preferred again.
    reg.reportSuccess("local");
    check(reg.health("local") == Health::Healthy && !reg.circuitOpen("local"),
          "reportSuccess clears the breaker (Healthy)");
    check(reg.select().endpoint.id == "local",
          "recovered local endpoint is preferred again");

    // Cooldown: after the open duration elapses, a Down endpoint becomes usable
    // again (half-open) even without an explicit success.
    reg.reportFailure("local");
    reg.reportFailure("local");
    reg.reportFailure("local");   // local Down again (lan + cloud still open too)
    check(!reg.select(), "local down again -> every endpoint down, honest Unavailable");
    check(reg.circuitOpen("cloud"), "cloud still within its cooldown window");
    clockMs += cfg.openDurationMs + 1;   // let every breaker cool down
    check(!reg.circuitOpen("cloud") && !reg.circuitOpen("local"),
          "breakers close after the cooldown (half-open)");
    check(reg.select().endpoint.id == "local",
          "after cooldown, the preferred (local) endpoint is retried");

    if (failures == 0) {
        std::cout << "PASS: preference ordering, failover on breaker, recovery on "
                     "success and after cooldown, and honest Unavailable all verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
