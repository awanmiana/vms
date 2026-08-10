// P6-03/P6-07 alarm-engine self-check (native increment 27). No Qt, no
// display. Exercises rule matching, deduplication, the accountable alarm
// lifecycle (new -> acknowledged -> escalated -> cleared, manual + auto-clear
// on the recovery event), fresh-alarm-after-clear, maintenance suppression
// (notification, never state), and the honest attention surface. Registered
// with CTest as `event_selfcheck`.

#include "events/AlarmEngine.h"

#include <iostream>
#include <string>

using namespace vms::events;

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}

Event ev(const char* type, const char* dev, long long t,
         const char* msg = "") {
    return Event{type, dev, msg, t};
}

} // namespace

int main() {
    std::cout << "vms_eventtest — P6-03/P6-07 alarm-engine self-check\n";

    AlarmEngine eng;
    eng.addRule({"r-offline", "Device offline", "device-offline", "",
                 Priority::High, "device-recovered", true});
    eng.addRule({"r-degraded", "Device degraded", "device-degraded", "",
                 Priority::Medium, "device-recovered", true});
    eng.addRule({"r-cam9", "Cam-9 storage", "storage-fault", "cam-9",
                 Priority::High, "", true});
    eng.addRule({"r-disabled", "Disabled rule", "device-offline", "",
                 Priority::Low, "", false});
    check(eng.rules().size() == 4, "rules registered");

    // --- 1) Matching ---------------------------------------------------------
    check(eng.ingest(ev("no-such-type", "cam-1", 100)) == 0,
          "an unmatched event raises nothing (honest silence)");
    check(eng.ingest(ev("storage-fault", "cam-1", 101)) == 0,
          "a device-filtered rule ignores other devices");
    check(eng.ingest(ev("device-offline", "cam-1", 102)) == 1,
          "matching event raises exactly one alarm (disabled rule silent)");
    {
        const auto active = eng.alarms();
        check(active.size() == 1 && active[0].priority == Priority::High &&
                  active[0].state == AlarmState::New &&
                  active[0].deviceId == "cam-1" && active[0].count == 1,
              "raised alarm carries rule priority, New state, device, count 1");
    }

    // --- 2) Dedup ------------------------------------------------------------
    check(eng.ingest(ev("device-offline", "cam-1", 110, "still down")) == 1,
          "re-ingest touches the existing alarm");
    {
        const auto active = eng.alarms();
        check(active.size() == 1 && active[0].count == 2 &&
                  active[0].lastSec == 110 && active[0].firstSec == 102 &&
                  active[0].message == "still down",
              "dedup bumps count/last-seen/message, no duplicate alarm");
    }
    check(eng.ingest(ev("device-offline", "cam-2", 111)) == 1 &&
              eng.alarms().size() == 2,
          "a different device is a separate alarm");

    // --- 3) Lifecycle --------------------------------------------------------
    // cam-1's alarm (the listing is newest-first, so select by device).
    const std::int64_t a1 = [&] {
        for (const Alarm& a : eng.alarms())
            if (a.deviceId == "cam-1") return a.id;
        return std::int64_t{-1};
    }();
    check(eng.acknowledge(a1), "acknowledge succeeds");
    check(eng.find(a1)->state == AlarmState::Acknowledged,
          "acknowledged silences but persists");
    check(eng.needsAttention() == 1,
          "attention count drops to the unacknowledged alarm only");
    eng.ingest(ev("device-offline", "cam-1", 120));
    check(eng.find(a1)->state == AlarmState::Acknowledged &&
              eng.find(a1)->count == 3,
          "re-ingest after acknowledge stays acknowledged (operator knows)");
    check(eng.escalate(a1) && eng.find(a1)->state == AlarmState::Escalated,
          "escalate re-raises attention");
    check(eng.needsAttention() == 2, "escalated counts as needing attention");

    // Auto-clear on the recovery event.
    check(eng.ingest(ev("device-recovered", "cam-1", 130)) == 1,
          "recovery event auto-clears the matching device's alarm");
    check(eng.find(a1)->state == AlarmState::Cleared &&
              eng.alarms().size() == 1,
          "cleared alarm leaves the active list (cam-2 remains)");

    // A re-occurrence after clear is a FRESH alarm.
    eng.ingest(ev("device-offline", "cam-1", 140));
    {
        const auto active = eng.alarms();
        const Alarm* fresh = nullptr;
        for (const Alarm& a : active)
            if (a.deviceId == "cam-1") fresh = &a;
        check(fresh && fresh->id != a1 && fresh->count == 1 &&
                  fresh->state == AlarmState::New,
              "re-occurrence after clear raises a fresh alarm (new id, count 1)");
    }

    // Manual clear + honest unknown ids.
    const std::int64_t cam2Id = [&] {
        for (const Alarm& a : eng.alarms())
            if (a.deviceId == "cam-2") return a.id;
        return std::int64_t{-1};
    }();
    check(eng.clearAlarm(cam2Id, 150), "manual clear succeeds");
    check(!eng.acknowledge(cam2Id), "lifecycle on a cleared alarm refuses");
    check(!eng.acknowledge(9999), "unknown alarm id refuses (never silent)");

    // --- 4) Maintenance suppression -----------------------------------------
    eng.setDeviceMaintenance("cam-1", true);
    check(eng.needsAttention() == 0 && !eng.shouldNotify(),
          "maintenance suppresses notification");
    {
        const auto active = eng.alarms();
        check(active.size() == 1 && active[0].suppressed &&
                  active[0].state == AlarmState::New,
              "the alarm itself stays visible and New (state never hidden)");
    }
    eng.setDeviceMaintenance("cam-1", false);
    check(eng.needsAttention() == 1 && eng.shouldNotify(),
          "leaving maintenance restores notification");
    eng.setDeviceMaintenance("cam-3", true);
    eng.ingest(ev("device-offline", "cam-3", 160));
    check(eng.needsAttention() == 1,
          "an alarm raised DURING maintenance is born suppressed");

    // --- 5) Rule management + listing ----------------------------------------
    check(eng.setRuleEnabled("r-offline", false), "disable a rule");
    check(eng.ingest(ev("device-offline", "cam-4", 170)) == 0,
          "a disabled rule no longer raises");
    check(!eng.setRuleEnabled("no-such", true) && !eng.removeRule("no-such"),
          "unknown rule ids refuse honestly");
    check(eng.removeRule("r-disabled") && eng.rules().size() == 3,
          "remove a rule");
    {
        const auto all = eng.alarms(true);
        const auto active = eng.alarms(false);
        check(all.size() > active.size(),
              "history (cleared) listed only on request");
        check(all.size() >= 2 && all[0].id > all[1].id,
              "alarms listed newest first");
    }

    // --- 6) P6-03 completion: versioning, schedules, conditions, simulation -
    AlarmEngine advanced;
    Rule scheduled{"r-scheduled", "Scheduled access alarm", "door-forced",
                   "", Priority::High, "door-secured", true};
    scheduled.version = 1;
    // Monday 09:00-17:00 UTC (2026-08-10 is a Monday).
    scheduled.schedules.push_back({0x01, 9 * 60, 17 * 60});
    scheduled.requiredAttributes["zone"] = "secure";
    check(advanced.addRule(scheduled), "valid versioned scheduled rule admitted");
    check(!advanced.addRule(scheduled), "same rule version refused (monotonic rollout)");
    Rule older = scheduled;
    older.version = 0;
    check(!advanced.addRule(older), "invalid/older rule version refused");

    // 2026-08-10 10:00:00 UTC = 1786356000.
    Event door = ev("door-forced", "door-1", 1786356000, "forced open");
    door.attributes["zone"] = "public";
    const auto beforeSimulationStats = advanced.stats();
    auto simulation = advanced.simulate(door);
    check(simulation.size() == 1 && !simulation[0].wouldRaise &&
              simulation[0].reason == "condition" &&
              advanced.alarms().empty() &&
              advanced.stats().events == beforeSimulationStats.events,
          "simulation explains refusal and is side-effect-free");
    door.attributes["zone"] = "secure";
    simulation = advanced.simulate(door);
    check(simulation[0].wouldRaise && simulation[0].reason == "match",
          "simulation reports a standards-aligned schedule/condition match");
    check(advanced.ingest(door) == 1, "inside-schedule conditioned event raises");
    Event afterHours = door;
    afterHours.deviceId = "door-2";
    afterHours.timeSec = 1786388400; // Monday 19:00 UTC.
    check(advanced.ingest(afterHours) == 0,
          "outside ISO-weekday UTC schedule is refused");

    // Version 2 atomically replaces version 1 and adds an overnight window.
    Rule scheduledV2 = scheduled;
    scheduledV2.version = 2;
    scheduledV2.schedules = {{0x01, 23 * 60, 60}}; // Monday -> Tuesday 01:00.
    check(advanced.addRule(scheduledV2) &&
              advanced.rules()[0].version == 2,
          "higher rule version replaces the active definition");
    Event overnight = door;
    overnight.deviceId = "door-3";
    overnight.timeSec = 1786408200; // 2026-08-11 00:30 UTC.
    check(advanced.ingest(overnight) == 1,
          "overnight schedule carries Monday mask into Tuesday");

    // --- 7) Correlation + dependencies --------------------------------------
    Rule correlated{"r-correlated", "Correlated intrusion", "motion", "",
                    Priority::High, "", true};
    correlated.correlationEventTypes = {"door-forced"};
    correlated.correlationWindowSec = 30;
    check(advanced.addRule(correlated), "correlation rule admitted");
    check(advanced.ingest(ev("motion", "cam-c", 200)) == 0,
          "correlation refuses without prerequisite history");
    advanced.ingest(ev("door-forced", "cam-c", 210));
    check(advanced.ingest(ev("motion", "cam-c", 220)) == 1,
          "same-device prerequisite inside correlation window matches");
    check(advanced.ingest(ev("motion", "cam-other", 220)) == 0,
          "correlation never crosses device identity");
    check(advanced.ingest(ev("motion", "cam-c", 250)) == 0,
          "expired correlation window refuses");

    Rule dependent{"r-dependent", "Dependent rule", "tamper", "",
                   Priority::Medium, "", true};
    dependent.dependsOnRuleIds = {"r-correlated"};
    check(advanced.addRule(dependent), "dependency rule admitted");
    check(advanced.ingest(ev("tamper", "cam-c", 251)) == 1,
          "dependency matches an active same-device alarm");
    check(advanced.ingest(ev("tamper", "cam-none", 251)) == 0,
          "missing same-device dependency refuses");

    // --- 8) P6-07 assignment, SLA/SLO, connector retry, audit ----------------
    AlarmEngine lifecycle;
    std::vector<AlarmAuditRecord> audit;
    lifecycle.setAuditSink([&](const AlarmAuditRecord& r) { audit.push_back(r); });
    Rule deliver{"r-deliver", "Delivered alarm", "critical", "",
                 Priority::High, "recovered", true};
    deliver.deliveryChannels = {"webhook", "smtp"};
    deliver.maxDeliveryAttempts = 3;
    deliver.initialRetrySec = 5;
    deliver.acknowledgeWithinSec = 10;
    deliver.resolveWithinSec = 20;
    check(lifecycle.addRule(deliver), "delivery/SLA rule admitted");
    check(lifecycle.ingest(ev("critical", "cam-8", 1000, "down")) == 1,
          "delivery rule raises an alarm");
    const std::int64_t deliveryAlarm = lifecycle.alarms()[0].id;
    check(lifecycle.assign(deliveryAlarm, "operator-42", 1001) &&
              lifecycle.find(deliveryAlarm)->assignedTo == "operator-42",
          "active alarm assignment is durable in the decision model");
    check(!lifecycle.assign(deliveryAlarm, "", 1001),
          "empty assignment identity is refused");
    auto jobs = lifecycle.notifications();
    check(jobs.size() == 2 && jobs[0].idempotencyKey != jobs[1].idempotencyKey,
          "one stable idempotent delivery job queued per configured channel");

    int connectorCalls = 0;
    auto flaky = [&](const Notification&, std::string& error) {
        ++connectorCalls;
        if (connectorCalls <= 2) {
            error = "temporary";
            return false;
        }
        return true;
    };
    check(lifecycle.deliverDue(1000, flaky) == 2,
          "due connector jobs attempted once each");
    jobs = lifecycle.notifications();
    check(jobs[0].state == DeliveryState::Pending && jobs[0].attempts == 1 &&
              jobs[0].nextAttemptSec == 1005,
          "failure schedules bounded exponential retry");
    check(lifecycle.deliverDue(1004, flaky) == 0,
          "retry is not attempted before its deadline");
    check(lifecycle.deliverDue(1005, flaky) == 2,
          "due retries are dispatched through injected connectors");
    jobs = lifecycle.notifications();
    check(jobs[0].state == DeliveryState::Delivered &&
              jobs[1].state == DeliveryState::Delivered,
          "successful retries reach delivered state");

    check(lifecycle.evaluateDeadlines(1010) == 1 &&
              lifecycle.find(deliveryAlarm)->acknowledgeBreached &&
              lifecycle.find(deliveryAlarm)->state == AlarmState::Escalated,
          "missed acknowledgement SLA escalates and records breach");
    check(lifecycle.evaluateDeadlines(1020) == 1 &&
              lifecycle.find(deliveryAlarm)->resolveBreached,
          "missed resolution SLO is marked without false auto-clear");
    check(lifecycle.acknowledge(deliveryAlarm, 1021) &&
              lifecycle.find(deliveryAlarm)->state == AlarmState::Acknowledged,
          "operator may acknowledge an SLA-escalated alarm");
    check(!audit.empty() && audit.front().action == "raise" &&
              audit.back().action == "acknowledge",
          "raise, assignment, delivery, SLA, and lifecycle emit audit records");

    AlarmEngine exhausted;
    Rule oneTry = deliver;
    oneTry.id = "r-one-try";
    oneTry.deliveryChannels = {"sms"};
    oneTry.maxDeliveryAttempts = 1;
    exhausted.addRule(oneTry);
    exhausted.ingest(ev("critical", "cam-9", 2000));
    check(exhausted.deliverDue(2000, [](const Notification&, std::string& e) {
              e = "permanent";
              return false;
          }) == 1 &&
              exhausted.notifications()[0].state == DeliveryState::Exhausted,
          "delivery becomes exhausted after the configured bounded attempts");

    AlarmEngine suppressedDelivery;
    suppressedDelivery.addRule(deliver);
    suppressedDelivery.setDeviceMaintenance("cam-10", true);
    suppressedDelivery.ingest(ev("critical", "cam-10", 3000));
    check(suppressedDelivery.deliverDue(
              3000, [](const Notification&, std::string&) { return true; }) == 0,
          "maintenance suppresses connector delivery without hiding the alarm");
    suppressedDelivery.setDeviceMaintenance("cam-10", false);
    check(suppressedDelivery.deliverDue(
              3000, [](const Notification&, std::string&) { return true; }) == 2,
          "leaving maintenance releases still-pending delivery jobs");

    const EvaluationStats perf = advanced.stats();
    check(perf.events >= 10 && perf.rulesExamined >= perf.events &&
              perf.rulesMatched > 0 && perf.elapsedNanoseconds > 0,
          "active-rule evaluation exposes independently measurable counters");

    AlarmEngine profile;
    for (int i = 0; i < 1000; ++i) {
        Rule r{"profile-" + std::to_string(i), "Profile rule",
               "event-" + std::to_string(i), "", Priority::Low, "", true};
        profile.addRule(r);
    }
    for (int i = 0; i < 1000; ++i)
        profile.ingest(ev("unmatched-profile-event", "profile-device", i));
    const EvaluationStats measured = profile.stats();
    const double nsPerRule = measured.rulesExamined == 0
                                 ? 0.0
                                 : static_cast<double>(measured.elapsedNanoseconds) /
                                       static_cast<double>(measured.rulesExamined);
    std::cout << "  info active-rule profile: " << measured.rulesExamined
              << " evaluations, " << nsPerRule << " ns/rule\n";
    check(measured.events == 1000 && measured.rulesExamined == 1000000 &&
              nsPerRule > 0.0 && nsPerRule < 100000.0,
          "1,000-active-rule profile completes inside the documented safety bound");

    if (failures == 0) {
        std::cout << "PASS: rule matching, versioning, UTC schedules, conditions, "
                     "correlation, dependencies, side-effect-free simulation, "
                     "performance counters, assignment, SLA/SLO, bounded delivery "
                     "retry, audit hooks, and accountable lifecycle verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
