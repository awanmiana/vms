#pragma once

// P6-03/P6-07 — event rules & alarm lifecycle (native increment 27). Scope:
// ../events-alarms-P6-proposal.md.
//
// The pure alarm decision core, in the same style as HealthMonitor: normalized
// events flow in, enabled rules decide what deserves an alarm at what
// priority, and each alarm lives an accountable lifecycle — new →
// acknowledged (silenced, persists) → escalated (attention re-raised) →
// cleared (manual, or automatic on the rule's recovery event). A matching
// ACTIVE alarm deduplicates (count + last-seen bump, state untouched) instead
// of duplicating; a cleared alarm's re-occurrence is a fresh alarm. Device
// maintenance suppresses notification, never the state (P0-03 honesty).
//
// This engine NOTIFIES, it never actuates: automated linkage (camera pop-up,
// PTZ, outputs, dispatch) is the separately-gated P6-05. Durable storage,
// backpressure, and throughput profiles are P6-04. Pure C++17, Qt-free,
// clock-free (callers stamp times); unit-tested by vms_eventtest/CTest.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vms::events {

enum class Priority { Low, Medium, High };
const char* PriorityName(Priority p);

enum class AlarmState { New, Acknowledged, Escalated, Cleared };
const char* AlarmStateName(AlarmState s);

// A normalized occurrence from any source (device health now; motion/VCA/
// inputs are later gates). `timeSec` is the caller's clock (epoch seconds).
struct Event {
    std::string type;       // e.g. "device-offline"
    std::string deviceId;   // may be empty for system events
    std::string message;    // human detail
    long long timeSec = 0;
};

// P6-03 first pass: trigger = exact event-type match + optional device filter.
struct Rule {
    std::string id;             // stable, e.g. "rule-device-offline"
    std::string title;
    std::string eventType;      // trigger (exact match)
    std::string deviceId;       // empty = any device
    Priority priority = Priority::Medium;
    std::string clearEventType; // auto-clear trigger (empty = manual only)
    bool enabled = true;
};

struct Alarm {
    std::int64_t id = 0;
    std::string ruleId;
    std::string deviceId;
    Priority priority = Priority::Medium;
    AlarmState state = AlarmState::New;
    long long firstSec = 0;   // first occurrence
    long long lastSec = 0;    // most recent occurrence
    int count = 1;            // deduplicated occurrences
    std::string message;      // latest message
    bool suppressed = false;  // device in maintenance (notification only)
};

class AlarmEngine {
public:
    // Rules. add replaces an existing rule with the same id.
    void addRule(const Rule& rule);
    bool removeRule(const std::string& ruleId);
    bool setRuleEnabled(const std::string& ruleId, bool enabled);
    std::vector<Rule> rules() const;

    // Ingest one event: raises or deduplicates alarms per the enabled rules,
    // and auto-clears active alarms whose rule names this event type as its
    // recovery. Returns how many alarms were raised or updated (0 = the event
    // matched nothing — honest silence, not an implicit alarm).
    int ingest(const Event& e);

    // Lifecycle (operator or command envelope). All return false on an
    // unknown/cleared alarm id — honest, never a silent no-op.
    bool acknowledge(std::int64_t alarmId);
    bool escalate(std::int64_t alarmId);
    bool clearAlarm(std::int64_t alarmId, long long nowSec);

    // Maintenance suppresses NOTIFICATION for a device's alarms; the alarm
    // state stays visible and true (mirrors HealthMonitor semantics).
    void setDeviceMaintenance(const std::string& deviceId, bool inMaintenance);

    // Active alarms (newest first), optionally including cleared history.
    std::vector<Alarm> alarms(bool includeCleared = false) const;
    const Alarm* find(std::int64_t alarmId) const;

    // Honest attention surface: how many unsuppressed alarms are demanding an
    // operator (New or Escalated), and whether anything should notify now.
    int needsAttention() const;
    bool shouldNotify() const;

private:
    bool active(const Alarm& a) const { return a.state != AlarmState::Cleared; }

    std::vector<Rule> rules_;
    std::vector<Alarm> alarms_;   // append-only this session; Cleared = history
    std::map<std::string, bool> maintenance_;
    std::int64_t nextId_ = 1;
};

} // namespace vms::events
