#include "AlarmController.h"

#include <QDateTime>

using vms::events::Alarm;
using vms::events::AlarmStateName;
using vms::events::Event;
using vms::events::Priority;
using vms::events::PriorityName;
using vms::events::Rule;

namespace {

long long nowSec() { return QDateTime::currentSecsSinceEpoch(); }

} // namespace

AlarmController::AlarmController(QObject* parent) : QObject(parent) {
    // The default health rules (the first event source, P6-08 direction):
    // offline is a High alarm, degraded a Medium one, both auto-cleared by the
    // device's recovery. Rule schedules/correlation are later P6-03 slices.
    engine_.addRule({"rule-device-offline", "Device offline", "device-offline",
                     "", Priority::High, "device-recovered", true});
    engine_.addRule({"rule-device-degraded", "Device degraded",
                     "device-degraded", "", Priority::Medium,
                     "device-recovered", true});
    rebuild();
}

void AlarmController::onHealthEvent(const QString& deviceId,
                                    const QString& eventType,
                                    const QString& message) {
    Event e;
    e.type = eventType.toStdString();
    e.deviceId = deviceId.toStdString();
    e.message = message.toStdString();
    e.timeSec = nowSec();
    if (engine_.ingest(e) > 0) rebuild();
}

bool AlarmController::acknowledge(double alarmId) {
    const bool ok = engine_.acknowledge(static_cast<std::int64_t>(alarmId));
    if (ok) rebuild();
    return ok;
}

bool AlarmController::escalate(double alarmId) {
    const bool ok = engine_.escalate(static_cast<std::int64_t>(alarmId));
    if (ok) rebuild();
    return ok;
}

bool AlarmController::clearAlarm(double alarmId) {
    const bool ok =
        engine_.clearAlarm(static_cast<std::int64_t>(alarmId), nowSec());
    if (ok) rebuild();
    return ok;
}

void AlarmController::setDeviceMaintenance(const QString& deviceId, bool on) {
    engine_.setDeviceMaintenance(deviceId.toStdString(), on);
    rebuild();
}

void AlarmController::rebuild() {
    model_.clear();
    for (const Alarm& a : engine_.alarms()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), static_cast<double>(a.id));
        m.insert(QStringLiteral("device"), QString::fromStdString(a.deviceId));
        m.insert(QStringLiteral("priority"),
                 QString::fromLatin1(PriorityName(a.priority)));
        m.insert(QStringLiteral("state"),
                 QString::fromLatin1(AlarmStateName(a.state)));
        m.insert(QStringLiteral("count"), a.count);
        m.insert(QStringLiteral("message"), QString::fromStdString(a.message));
        m.insert(QStringLiteral("time"),
                 QDateTime::fromSecsSinceEpoch(a.firstSec)
                     .toUTC().toString(QStringLiteral("HH:mm:ss")));
        m.insert(QStringLiteral("suppressed"), a.suppressed);
        model_.push_back(m);
    }
    emit changed();
}
