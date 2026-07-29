#pragma once

// Node-connectivity resilience — native increment 9. Scope: ../Development_plan.md
// (Approval Log "Node-connectivity resilience"), realizing the EndpointHealth
// slice deferred from optimization-service-P12-03-proposal.md §4 Option C.
//
// The single place that decides, for THIS software's own peer / AI-agent
// endpoints, which endpoint to talk to and when to fail over to another. It is
// explicitly NOT about camera/NVR connectivity — that is the ConnectionBroker.
//
// A `NodeRegistry` holds a preference-ordered set of endpoints for one logical
// service (a coordinator, a sibling node, an AI-agent connector), each with a
// health state tracked by the P0-03 login backoff / circuit-breaker (the same
// pattern the ConnectionBroker uses). `select()` returns the most-preferred
// currently-usable endpoint (local before LAN before internet, then registration
// order); if the primary trips its breaker, the next healthy endpoint is chosen,
// so a degraded peer link does not stall the software — it fails over.
//
// Pure and OS-free (no sockets): standalone has no peers yet, so the real
// transport, peer discovery, authentication, and the AI-agent connector are the
// P1-10 / A0 gates. They implement the transport behind this decision core
// unchanged. The injectable clock makes the breaker deterministic under test.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace vms::node {

// Where an endpoint lives, in failover-preference order (lower = preferred).
enum class Scope { Local = 0, Lan = 1, Internet = 2 };
const char* ScopeName(Scope s);

// Honest per-endpoint health (never claims healthy without evidence).
enum class Health { Unknown, Healthy, Degraded, Down };
const char* HealthName(Health h);

struct Endpoint {
    std::string id;              // logical id, unique within the registry
    std::string host;
    int port = 0;
    Scope scope = Scope::Local;
};

struct FailoverConfig {
    int failureThreshold = 3;          // consecutive failures -> Down + open breaker
    std::int64_t openDurationMs = 30000;  // breaker cooldown before a retry is allowed
};

// Result of selecting an endpoint to use.
struct SelectResult {
    bool ok = false;
    Endpoint endpoint;
    std::string message;   // set on failure (all endpoints down / registry empty)
    explicit operator bool() const { return ok; }
};

// Injectable monotonic clock in milliseconds (deterministic under test).
using NowMsFn = std::function<std::int64_t()>;

class NodeRegistry {
public:
    explicit NodeRegistry(FailoverConfig cfg = {}, NowMsFn now = {});

    // Register an endpoint. Ignored (returns false) if the id is empty or already
    // present. Preference is (scope, registration order), so add local first.
    bool add(const Endpoint& e);

    // The most-preferred endpoint whose breaker is not open. Fails (Unavailable)
    // when the registry is empty or every endpoint is currently down.
    SelectResult select() const;

    // Health feedback from the transport (P1-10). A success clears the breaker; a
    // run of failures degrades then opens it (fails over to the next endpoint).
    void reportSuccess(const std::string& id);
    void reportFailure(const std::string& id);

    Health health(const std::string& id) const;
    bool circuitOpen(const std::string& id) const;
    std::size_t size() const { return nodes_.size(); }

private:
    struct Node {
        Endpoint ep;
        int order = 0;              // registration order (tie-break within a scope)
        int failures = 0;
        std::int64_t openUntilMs = 0;
        bool sampled = false;       // has any success/failure been reported?
    };

    Node* find(const std::string& id);
    const Node* find(const std::string& id) const;

    FailoverConfig cfg_;
    NowMsFn now_;
    std::vector<Node> nodes_;
    int nextOrder_ = 0;
};

} // namespace vms::node
