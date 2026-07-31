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

    if (failures == 0) {
        std::cout << "PASS: rule matching, dedup, the accountable lifecycle "
                     "(ack/escalate/clear + auto-clear), fresh-after-clear, "
                     "maintenance suppression, and honest attention verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
