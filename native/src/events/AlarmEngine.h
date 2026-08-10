#pragma once

// P6-03/P6-07 - deterministic event rules, alarm lifecycle, and delivery.
// Pure C++17, Qt-free, clock-free (callers provide UTC epoch seconds).

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace vms::events {

enum class Priority { Low, Medium, High };
const char* PriorityName(Priority p);

enum class AlarmState { New, Acknowledged, Escalated, Cleared };
const char* AlarmStateName(AlarmState s);

struct Event {
    std::string type;
    std::string deviceId;
    std::string message;
    long long timeSec = 0;
    // Vendor-neutral normalized attributes. Rules use exact, typed-as-text
    // comparisons; adapters own vendor payload normalization.
    std::map<std::string, std::string> attributes;
};

// ISO-8601 weekday mask: bit 0 Monday ... bit 6 Sunday. Times are explicitly
// UTC minutes, avoiding host-local/DST ambiguity. start > end spans midnight.
struct ScheduleWindow {
    std::uint8_t isoWeekdayMask = 0x7f;
    int startMinuteUtc = 0;
    int endMinuteUtc = 1440;
};

struct Rule {
    std::string id;
    std::string title;
    std::string eventType;
    std::string deviceId;
    Priority priority = Priority::Medium;
    std::string clearEventType;
    bool enabled = true;

    // P6-03 completion fields. Empty schedule means always active. Every
    // correlation type must have occurred for the same device inside the
    // window. Every dependency must have an active same-device alarm.
    int version = 1;
    std::vector<ScheduleWindow> schedules;
    std::map<std::string, std::string> requiredAttributes;
    std::vector<std::string> correlationEventTypes;
    long long correlationWindowSec = 0;
    std::vector<std::string> dependsOnRuleIds;

    // P6-07 policy. Delivery channel names identify injected connectors;
    // AlarmEngine never performs network I/O itself.
    std::vector<std::string> deliveryChannels;
    int maxDeliveryAttempts = 3;
    long long initialRetrySec = 5;
    long long acknowledgeWithinSec = 0;
    long long resolveWithinSec = 0;
};

struct Alarm {
    std::int64_t id = 0;
    std::string ruleId;
    std::string deviceId;
    Priority priority = Priority::Medium;
    AlarmState state = AlarmState::New;
    long long firstSec = 0;
    long long lastSec = 0;
    int count = 1;
    std::string message;
    bool suppressed = false;
    std::string assignedTo;
    long long acknowledgeBySec = 0;
    long long resolveBySec = 0;
    bool acknowledgeBreached = false;
    bool resolveBreached = false;
};

struct RuleEvaluation {
    std::string ruleId;
    bool wouldRaise = false;
    bool wouldClear = false;
    std::string reason;
};

struct EvaluationStats {
    std::uint64_t events = 0;
    std::uint64_t rulesExamined = 0;
    std::uint64_t rulesMatched = 0;
    std::uint64_t alarmsRaised = 0;
    std::uint64_t alarmsDeduplicated = 0;
    std::uint64_t elapsedNanoseconds = 0;
};

enum class DeliveryState { Pending, Delivered, Exhausted, Cancelled };
const char* DeliveryStateName(DeliveryState s);

struct Notification {
    std::int64_t id = 0;
    std::int64_t alarmId = 0;
    std::string channel;
    std::string idempotencyKey;
    DeliveryState state = DeliveryState::Pending;
    int attempts = 0;
    int maxAttempts = 3;
    long long nextAttemptSec = 0;
    long long initialRetrySec = 5;
    std::string lastError;
};

using DeliveryFn = std::function<bool(const Notification&, std::string& error)>;

struct AlarmAuditRecord {
    std::string action;
    std::int64_t alarmId = 0;
    std::string ruleId;
    std::string deviceId;
    std::string detail;
    long long timeSec = 0;
};
using AlarmAuditSink = std::function<void(const AlarmAuditRecord&)>;

class AlarmEngine {
public:
    // New versions replace older versions. Same/older versions are rejected,
    // making rule rollout monotonic and auditable. Invalid rules are refused.
    bool addRule(const Rule& rule);
    bool removeRule(const std::string& ruleId);
    bool setRuleEnabled(const std::string& ruleId, bool enabled);
    std::vector<Rule> rules() const;

    int ingest(const Event& e);
    // Read-only decision trace: no event history, alarm, delivery, audit, or
    // metrics mutation. Used for safe rule simulation before activation.
    std::vector<RuleEvaluation> simulate(const Event& e) const;

    bool acknowledge(std::int64_t alarmId, long long nowSec = 0);
    bool escalate(std::int64_t alarmId, long long nowSec = 0);
    bool clearAlarm(std::int64_t alarmId, long long nowSec);
    bool assign(std::int64_t alarmId, const std::string& operatorId,
                long long nowSec = 0);

    void setDeviceMaintenance(const std::string& deviceId, bool inMaintenance);

    // Evaluates SLA deadlines. A missed acknowledgement deadline escalates a
    // New alarm; a missed resolution deadline is marked, never auto-cleared.
    int evaluateDeadlines(long long nowSec);

    std::vector<Alarm> alarms(bool includeCleared = false) const;
    const Alarm* find(std::int64_t alarmId) const;
    int needsAttention() const;
    bool shouldNotify() const;

    // Attempt due notifications through an injected connector dispatcher.
    // Failures use bounded exponential retry; the stable idempotency key lets
    // SMTP/SMS/webhook adapters suppress duplicates across ambiguous retries.
    int deliverDue(long long nowSec, const DeliveryFn& deliver);
    std::vector<Notification> notifications(bool includeFinal = true) const;

    EvaluationStats stats() const { return stats_; }
    void resetStats() { stats_ = {}; }
    void setAuditSink(AlarmAuditSink sink) { audit_ = std::move(sink); }

private:
    bool active(const Alarm& a) const { return a.state != AlarmState::Cleared; }
    std::vector<RuleEvaluation> evaluate(const Event& e) const;
    bool scheduleAllows(const Rule& rule, long long timeSec) const;
    bool attributesAllow(const Rule& rule, const Event& e) const;
    bool correlationsAllow(const Rule& rule, const Event& e) const;
    bool dependenciesAllow(const Rule& rule, const Event& e) const;
    void queueNotifications(const Rule& rule, const Alarm& alarm,
                            long long nowSec);
    void cancelPending(std::int64_t alarmId);
    void audit(const char* action, const Alarm& alarm,
               const std::string& detail, long long timeSec) const;

    std::vector<Rule> rules_;
    std::vector<Alarm> alarms_;
    std::vector<Notification> notifications_;
    std::map<std::string, bool> maintenance_;
    std::map<std::string, long long> recentEvents_;
    std::int64_t nextId_ = 1;
    std::int64_t nextNotificationId_ = 1;
    EvaluationStats stats_;
    AlarmAuditSink audit_;
};

} // namespace vms::events
