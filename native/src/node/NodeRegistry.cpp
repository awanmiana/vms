#include "node/NodeRegistry.h"

#include <chrono>
#include <tuple>

namespace vms::node {

const char* ScopeName(Scope s) {
    switch (s) {
        case Scope::Local:    return "local";
        case Scope::Lan:      return "lan";
        case Scope::Internet: return "internet";
    }
    return "?";
}

const char* HealthName(Health h) {
    switch (h) {
        case Health::Unknown:  return "unknown";
        case Health::Healthy:  return "healthy";
        case Health::Degraded: return "degraded";
        case Health::Down:     return "down";
    }
    return "?";
}

namespace {
std::int64_t steadyNowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

NodeRegistry::NodeRegistry(FailoverConfig cfg, NowMsFn now)
    : cfg_(cfg), now_(std::move(now)) {
    if (!now_) now_ = &steadyNowMs;
}

NodeRegistry::Node* NodeRegistry::find(const std::string& id) {
    for (auto& n : nodes_) if (n.ep.id == id) return &n;
    return nullptr;
}
const NodeRegistry::Node* NodeRegistry::find(const std::string& id) const {
    for (const auto& n : nodes_) if (n.ep.id == id) return &n;
    return nullptr;
}

bool NodeRegistry::add(const Endpoint& e) {
    if (e.id.empty() || find(e.id)) return false;
    Node n;
    n.ep = e;
    n.order = nextOrder_++;
    nodes_.push_back(n);
    return true;
}

SelectResult NodeRegistry::select() const {
    SelectResult r;
    if (nodes_.empty()) {
        r.message = "no endpoints registered";
        return r;
    }
    const std::int64_t now = now_();
    const Node* best = nullptr;
    for (const auto& n : nodes_) {
        if (n.openUntilMs > now) continue;   // breaker open -> not usable
        // Prefer lower scope (local < lan < internet), then registration order.
        if (!best ||
            std::make_tuple(static_cast<int>(n.ep.scope), n.order) <
                std::make_tuple(static_cast<int>(best->ep.scope), best->order))
            best = &n;
    }
    if (!best) {
        r.message = "all endpoints are down (circuit open)";
        return r;
    }
    r.ok = true;
    r.endpoint = best->ep;
    return r;
}

void NodeRegistry::reportSuccess(const std::string& id) {
    Node* n = find(id);
    if (!n) return;
    n->failures = 0;
    n->openUntilMs = 0;
    n->sampled = true;
}

void NodeRegistry::reportFailure(const std::string& id) {
    Node* n = find(id);
    if (!n) return;
    n->sampled = true;
    n->failures += 1;
    if (n->failures >= cfg_.failureThreshold)
        n->openUntilMs = now_() + cfg_.openDurationMs;
}

Health NodeRegistry::health(const std::string& id) const {
    const Node* n = find(id);
    if (!n) return Health::Unknown;
    if (n->openUntilMs > now_()) return Health::Down;
    if (!n->sampled) return Health::Unknown;
    return n->failures > 0 ? Health::Degraded : Health::Healthy;
}

bool NodeRegistry::circuitOpen(const std::string& id) const {
    const Node* n = find(id);
    return n && n->openUntilMs > now_();
}

} // namespace vms::node
