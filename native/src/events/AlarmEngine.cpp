#include "events/AlarmEngine.h"

#include <algorithm>

namespace vms::events {

const char* PriorityName(Priority p) {
    switch (p) {
        case Priority::Low:    return "low";
        case Priority::Medium: return "medium";
        case Priority::High:   return "high";
    }
    return "?";
}

const char* AlarmStateName(AlarmState s) {
    switch (s) {
        case AlarmState::New:          return "new";
        case AlarmState::Acknowledged: return "acknowledged";
        case AlarmState::Escalated:    return "escalated";
        case AlarmState::Cleared:      return "cleared";
    }
    return "?";
}

void AlarmEngine::addRule(const Rule& rule) {
    for (Rule& r : rules_) {
        if (r.id == rule.id) {
            r = rule;
            return;
        }
    }
    rules_.push_back(rule);
}

bool AlarmEngine::removeRule(const std::string& ruleId) {
    const auto it = std::find_if(rules_.begin(), rules_.end(),
                                 [&](const Rule& r) { return r.id == ruleId; });
    if (it == rules_.end()) return false;
    rules_.erase(it);
    return true;
}

bool AlarmEngine::setRuleEnabled(const std::string& ruleId, bool enabled) {
    for (Rule& r : rules_) {
        if (r.id == ruleId) {
            r.enabled = enabled;
            return true;
        }
    }
    return false;
}

std::vector<Rule> AlarmEngine::rules() const { return rules_; }

int AlarmEngine::ingest(const Event& e) {
    int touched = 0;
    for (const Rule& r : rules_) {
        if (!r.enabled) continue;
        const bool deviceMatches = r.deviceId.empty() || r.deviceId == e.deviceId;

        // Auto-clear: this event is the rule's recovery signal.
        if (!r.clearEventType.empty() && r.clearEventType == e.type &&
            deviceMatches) {
            for (Alarm& a : alarms_) {
                if (a.ruleId == r.id && a.deviceId == e.deviceId && active(a)) {
                    a.state = AlarmState::Cleared;
                    a.lastSec = e.timeSec;
                    ++touched;
                }
            }
        }

        // Trigger: raise or deduplicate.
        if (r.eventType == e.type && deviceMatches) {
            Alarm* existing = nullptr;
            for (Alarm& a : alarms_) {
                if (a.ruleId == r.id && a.deviceId == e.deviceId && active(a)) {
                    existing = &a;
                    break;
                }
            }
            if (existing) {
                // Dedup: the occurrence count and freshness move; the state
                // does NOT (an acknowledged alarm stays acknowledged — the
                // operator already knows; a NEW occurrence after clear is a
                // fresh alarm below).
                existing->count += 1;
                existing->lastSec = e.timeSec;
                existing->message = e.message;
            } else {
                Alarm a;
                a.id = nextId_++;
                a.ruleId = r.id;
                a.deviceId = e.deviceId;
                a.priority = r.priority;
                a.state = AlarmState::New;
                a.firstSec = a.lastSec = e.timeSec;
                a.count = 1;
                a.message = e.message;
                const auto m = maintenance_.find(e.deviceId);
                a.suppressed = (m != maintenance_.end() && m->second);
                alarms_.push_back(a);
            }
            ++touched;
        }
    }
    return touched;
}

bool AlarmEngine::acknowledge(std::int64_t alarmId) {
    for (Alarm& a : alarms_) {
        if (a.id == alarmId && active(a)) {
            if (a.state == AlarmState::Acknowledged) return true;  // idempotent
            a.state = AlarmState::Acknowledged;
            return true;
        }
    }
    return false;
}

bool AlarmEngine::escalate(std::int64_t alarmId) {
    for (Alarm& a : alarms_) {
        if (a.id == alarmId && active(a)) {
            a.state = AlarmState::Escalated;
            return true;
        }
    }
    return false;
}

bool AlarmEngine::clearAlarm(std::int64_t alarmId, long long nowSec) {
    for (Alarm& a : alarms_) {
        if (a.id == alarmId && active(a)) {
            a.state = AlarmState::Cleared;
            a.lastSec = nowSec;
            return true;
        }
    }
    return false;
}

void AlarmEngine::setDeviceMaintenance(const std::string& deviceId,
                                       bool inMaintenance) {
    maintenance_[deviceId] = inMaintenance;
    // Suppression follows the device, including alarms already raised.
    for (Alarm& a : alarms_)
        if (a.deviceId == deviceId && active(a)) a.suppressed = inMaintenance;
}

std::vector<Alarm> AlarmEngine::alarms(bool includeCleared) const {
    std::vector<Alarm> out;
    for (const Alarm& a : alarms_)
        if (includeCleared || a.state != AlarmState::Cleared) out.push_back(a);
    // Newest first: the operator's working order.
    std::sort(out.begin(), out.end(),
              [](const Alarm& x, const Alarm& y) { return x.id > y.id; });
    return out;
}

const Alarm* AlarmEngine::find(std::int64_t alarmId) const {
    for (const Alarm& a : alarms_)
        if (a.id == alarmId) return &a;
    return nullptr;
}

int AlarmEngine::needsAttention() const {
    int n = 0;
    for (const Alarm& a : alarms_)
        if ((a.state == AlarmState::New || a.state == AlarmState::Escalated) &&
            !a.suppressed)
            ++n;
    return n;
}

bool AlarmEngine::shouldNotify() const { return needsAttention() > 0; }

} // namespace vms::events
