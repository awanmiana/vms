#include "events/AlarmEngine.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <set>

namespace vms::events {
namespace {

long long floorDiv(long long a, long long b) {
    long long q = a / b;
    const long long r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

int isoWeekday(long long day) {
    // 1970-01-01 was Thursday (ISO weekday 4).
    int d = static_cast<int>((day + 3) % 7);
    if (d < 0) d += 7;
    return d + 1;
}

bool maskHas(std::uint8_t mask, int isoDay) {
    return (mask & static_cast<std::uint8_t>(1u << (isoDay - 1))) != 0;
}

std::string historyKey(const std::string& device, const std::string& type) {
    return device + std::string(1, '\x1f') + type;
}

} // namespace

const char* PriorityName(Priority p) {
    switch (p) {
        case Priority::Low: return "low";
        case Priority::Medium: return "medium";
        case Priority::High: return "high";
    }
    return "?";
}

const char* AlarmStateName(AlarmState s) {
    switch (s) {
        case AlarmState::New: return "new";
        case AlarmState::Acknowledged: return "acknowledged";
        case AlarmState::Escalated: return "escalated";
        case AlarmState::Cleared: return "cleared";
    }
    return "?";
}

const char* DeliveryStateName(DeliveryState s) {
    switch (s) {
        case DeliveryState::Pending: return "pending";
        case DeliveryState::Delivered: return "delivered";
        case DeliveryState::Exhausted: return "exhausted";
        case DeliveryState::Cancelled: return "cancelled";
    }
    return "?";
}

bool AlarmEngine::addRule(const Rule& rule) {
    if (rule.id.empty() || rule.eventType.empty() || rule.version < 1 ||
        rule.maxDeliveryAttempts < 1 || rule.initialRetrySec < 0 ||
        rule.correlationWindowSec < 0 || rule.acknowledgeWithinSec < 0 ||
        rule.resolveWithinSec < 0)
        return false;
    for (const ScheduleWindow& s : rule.schedules)
        if (s.isoWeekdayMask == 0 || s.startMinuteUtc < 0 ||
            s.startMinuteUtc > 1439 || s.endMinuteUtc < 0 ||
            s.endMinuteUtc > 1440 || s.startMinuteUtc == s.endMinuteUtc)
            return false;
    std::set<std::string> channels;
    for (const std::string& channel : rule.deliveryChannels)
        if (channel.empty() || !channels.insert(channel).second) return false;
    for (Rule& current : rules_) {
        if (current.id != rule.id) continue;
        if (rule.version <= current.version) return false;
        current = rule;
        return true;
    }
    rules_.push_back(rule);
    return true;
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

bool AlarmEngine::scheduleAllows(const Rule& rule, long long timeSec) const {
    if (rule.schedules.empty()) return true;
    const long long day = floorDiv(timeSec, 86400);
    const long long secondOfDay = timeSec - day * 86400;
    const int minute = static_cast<int>(secondOfDay / 60);
    const int today = isoWeekday(day);
    const int yesterday = today == 1 ? 7 : today - 1;
    for (const ScheduleWindow& s : rule.schedules) {
        if (s.startMinuteUtc < s.endMinuteUtc) {
            if (maskHas(s.isoWeekdayMask, today) && minute >= s.startMinuteUtc &&
                minute < s.endMinuteUtc)
                return true;
        } else {
            if ((maskHas(s.isoWeekdayMask, today) && minute >= s.startMinuteUtc) ||
                (maskHas(s.isoWeekdayMask, yesterday) && minute < s.endMinuteUtc))
                return true;
        }
    }
    return false;
}

bool AlarmEngine::attributesAllow(const Rule& rule, const Event& e) const {
    for (const auto& wanted : rule.requiredAttributes) {
        const auto it = e.attributes.find(wanted.first);
        if (it == e.attributes.end() || it->second != wanted.second) return false;
    }
    return true;
}

bool AlarmEngine::correlationsAllow(const Rule& rule, const Event& e) const {
    if (rule.correlationEventTypes.empty()) return true;
    if (rule.correlationWindowSec <= 0) return false;
    for (const std::string& type : rule.correlationEventTypes) {
        const auto it = recentEvents_.find(historyKey(e.deviceId, type));
        if (it == recentEvents_.end() || it->second > e.timeSec ||
            e.timeSec - it->second > rule.correlationWindowSec)
            return false;
    }
    return true;
}

bool AlarmEngine::dependenciesAllow(const Rule& rule, const Event& e) const {
    for (const std::string& dependency : rule.dependsOnRuleIds) {
        bool found = false;
        for (const Alarm& a : alarms_)
            if (active(a) && a.ruleId == dependency &&
                a.deviceId == e.deviceId) {
                found = true;
                break;
            }
        if (!found) return false;
    }
    return true;
}

std::vector<RuleEvaluation> AlarmEngine::evaluate(const Event& e) const {
    std::vector<RuleEvaluation> out;
    out.reserve(rules_.size());
    for (const Rule& r : rules_) {
        RuleEvaluation x;
        x.ruleId = r.id;
        const bool deviceMatches = r.deviceId.empty() || r.deviceId == e.deviceId;
        if (!r.enabled) x.reason = "disabled";
        else if (!deviceMatches) x.reason = "device-filter";
        else if (!r.clearEventType.empty() && r.clearEventType == e.type) {
            x.wouldClear = true;
            x.reason = "recovery-match";
        } else if (r.eventType != e.type) x.reason = "event-type";
        else if (!scheduleAllows(r, e.timeSec)) x.reason = "outside-schedule";
        else if (!attributesAllow(r, e)) x.reason = "condition";
        else if (!correlationsAllow(r, e)) x.reason = "correlation";
        else if (!dependenciesAllow(r, e)) x.reason = "dependency";
        else {
            x.wouldRaise = true;
            x.reason = "match";
        }
        out.push_back(std::move(x));
    }
    return out;
}

std::vector<RuleEvaluation> AlarmEngine::simulate(const Event& e) const {
    return evaluate(e);
}

void AlarmEngine::queueNotifications(const Rule& rule, const Alarm& alarm,
                                     long long nowSec) {
    for (const std::string& channel : rule.deliveryChannels) {
        Notification n;
        n.id = nextNotificationId_++;
        n.alarmId = alarm.id;
        n.channel = channel;
        n.idempotencyKey = "alarm-" + std::to_string(alarm.id) + "-" + channel;
        n.maxAttempts = rule.maxDeliveryAttempts;
        n.initialRetrySec = rule.initialRetrySec;
        n.nextAttemptSec = nowSec;
        notifications_.push_back(std::move(n));
    }
}

void AlarmEngine::audit(const char* action, const Alarm& alarm,
                        const std::string& detail, long long timeSec) const {
    if (audit_)
        audit_({action, alarm.id, alarm.ruleId, alarm.deviceId, detail, timeSec});
}

int AlarmEngine::ingest(const Event& e) {
    const auto started = std::chrono::steady_clock::now();
    const std::vector<RuleEvaluation> decisions = evaluate(e);
    int touched = 0;
    ++stats_.events;
    stats_.rulesExamined += rules_.size();
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        const Rule& r = rules_[i];
        const RuleEvaluation& decision = decisions[i];
        if (decision.wouldClear) {
            for (Alarm& a : alarms_) {
                if (a.ruleId == r.id && a.deviceId == e.deviceId && active(a)) {
                    a.state = AlarmState::Cleared;
                    a.lastSec = e.timeSec;
                    cancelPending(a.id);
                    audit("auto-clear", a, e.message, e.timeSec);
                    ++touched;
                    ++stats_.rulesMatched;
                }
            }
        }
        if (!decision.wouldRaise) continue;
        ++stats_.rulesMatched;
        Alarm* existing = nullptr;
        for (Alarm& a : alarms_)
            if (a.ruleId == r.id && a.deviceId == e.deviceId && active(a)) {
                existing = &a;
                break;
            }
        if (existing) {
            ++existing->count;
            existing->lastSec = e.timeSec;
            existing->message = e.message;
            audit("deduplicate", *existing, e.message, e.timeSec);
            ++stats_.alarmsDeduplicated;
        } else {
            Alarm a;
            a.id = nextId_++;
            a.ruleId = r.id;
            a.deviceId = e.deviceId;
            a.priority = r.priority;
            a.firstSec = a.lastSec = e.timeSec;
            a.message = e.message;
            const auto m = maintenance_.find(e.deviceId);
            a.suppressed = m != maintenance_.end() && m->second;
            if (r.acknowledgeWithinSec > 0)
                a.acknowledgeBySec = e.timeSec + r.acknowledgeWithinSec;
            if (r.resolveWithinSec > 0)
                a.resolveBySec = e.timeSec + r.resolveWithinSec;
            alarms_.push_back(a);
            queueNotifications(r, alarms_.back(), e.timeSec);
            audit("raise", alarms_.back(), e.message, e.timeSec);
            ++stats_.alarmsRaised;
        }
        ++touched;
    }
    recentEvents_[historyKey(e.deviceId, e.type)] = e.timeSec;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started);
    stats_.elapsedNanoseconds += static_cast<std::uint64_t>(elapsed.count());
    return touched;
}

bool AlarmEngine::acknowledge(std::int64_t alarmId, long long nowSec) {
    for (Alarm& a : alarms_) {
        if (a.id == alarmId && active(a)) {
            if (!a.acknowledgeBreached && nowSec > 0 &&
                a.acknowledgeBySec > 0 && nowSec >= a.acknowledgeBySec) {
                a.acknowledgeBreached = true;
                audit("sla-ack-breach", a, "late acknowledgement", nowSec);
            }
            a.state = AlarmState::Acknowledged;
            cancelPending(a.id);
            audit("acknowledge", a, a.assignedTo, nowSec);
            return true;
        }
    }
    return false;
}

bool AlarmEngine::escalate(std::int64_t alarmId, long long nowSec) {
    for (Alarm& a : alarms_) {
        if (a.id == alarmId && active(a)) {
            a.state = AlarmState::Escalated;
            audit("escalate", a, a.assignedTo, nowSec);
            return true;
        }
    }
    return false;
}

bool AlarmEngine::clearAlarm(std::int64_t alarmId, long long nowSec) {
    for (Alarm& a : alarms_) {
        if (a.id == alarmId && active(a)) {
            if (!a.resolveBreached && a.resolveBySec > 0 &&
                nowSec >= a.resolveBySec) {
                a.resolveBreached = true;
                audit("sla-resolve-breach", a, "late resolution", nowSec);
            }
            a.state = AlarmState::Cleared;
            a.lastSec = nowSec;
            cancelPending(a.id);
            audit("clear", a, a.assignedTo, nowSec);
            return true;
        }
    }
    return false;
}

bool AlarmEngine::assign(std::int64_t alarmId, const std::string& operatorId,
                         long long nowSec) {
    if (operatorId.empty()) return false;
    for (Alarm& a : alarms_) {
        if (a.id == alarmId && active(a)) {
            a.assignedTo = operatorId;
            audit("assign", a, operatorId, nowSec);
            return true;
        }
    }
    return false;
}

void AlarmEngine::cancelPending(std::int64_t alarmId) {
    for (Notification& n : notifications_)
        if (n.alarmId == alarmId && n.state == DeliveryState::Pending)
            n.state = DeliveryState::Cancelled;
}

void AlarmEngine::setDeviceMaintenance(const std::string& deviceId,
                                       bool inMaintenance) {
    maintenance_[deviceId] = inMaintenance;
    for (Alarm& a : alarms_)
        if (a.deviceId == deviceId && active(a)) a.suppressed = inMaintenance;
}

int AlarmEngine::evaluateDeadlines(long long nowSec) {
    int changed = 0;
    for (Alarm& a : alarms_) {
        if (!active(a)) continue;
        if (!a.acknowledgeBreached && a.acknowledgeBySec > 0 &&
            nowSec >= a.acknowledgeBySec && a.state == AlarmState::New) {
            a.acknowledgeBreached = true;
            a.state = AlarmState::Escalated;
            audit("sla-ack-breach", a, "acknowledgement deadline", nowSec);
            ++changed;
        }
        if (!a.resolveBreached && a.resolveBySec > 0 &&
            nowSec >= a.resolveBySec) {
            a.resolveBreached = true;
            audit("sla-resolve-breach", a, "resolution deadline", nowSec);
            ++changed;
        }
    }
    return changed;
}

std::vector<Alarm> AlarmEngine::alarms(bool includeCleared) const {
    std::vector<Alarm> out;
    for (const Alarm& a : alarms_)
        if (includeCleared || active(a)) out.push_back(a);
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

int AlarmEngine::deliverDue(long long nowSec, const DeliveryFn& deliver) {
    if (!deliver) return 0;
    int attempted = 0;
    for (Notification& n : notifications_) {
        if (n.state != DeliveryState::Pending || n.nextAttemptSec > nowSec)
            continue;
        const Alarm* alarm = find(n.alarmId);
        if (!alarm || !active(*alarm)) {
            n.state = DeliveryState::Cancelled;
            continue;
        }
        if (alarm->suppressed || alarm->state == AlarmState::Acknowledged)
            continue;
        ++attempted;
        ++n.attempts;
        std::string error;
        if (deliver(n, error)) {
            n.state = DeliveryState::Delivered;
            n.lastError.clear();
            audit("delivery-ok", *alarm, n.channel, nowSec);
        } else {
            n.lastError = error.empty() ? "connector refused delivery" : error;
            if (n.attempts >= n.maxAttempts) {
                n.state = DeliveryState::Exhausted;
                audit("delivery-exhausted", *alarm,
                      n.channel + ": " + n.lastError, nowSec);
            } else {
                const int shift = std::min(n.attempts - 1, 30);
                const long long factor = 1LL << shift;
                const long long maxDelay = std::numeric_limits<long long>::max();
                const long long delay =
                    n.initialRetrySec > 0 && factor > maxDelay / n.initialRetrySec
                        ? maxDelay : n.initialRetrySec * factor;
                n.nextAttemptSec = nowSec > maxDelay - delay
                                       ? maxDelay : nowSec + delay;
                audit("delivery-retry", *alarm,
                      n.channel + ": " + n.lastError, nowSec);
            }
        }
    }
    return attempted;
}

std::vector<Notification> AlarmEngine::notifications(bool includeFinal) const {
    std::vector<Notification> out;
    for (const Notification& n : notifications_)
        if (includeFinal || n.state == DeliveryState::Pending) out.push_back(n);
    return out;
}

} // namespace vms::events
