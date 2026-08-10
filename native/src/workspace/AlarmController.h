#pragma once

// P6-03/P6-07 — the alarm surface's Qt bridge (native increment 27). Scope:
// ../events-alarms-P6-proposal.md.
//
// Binds the pure AlarmEngine to QML: the Alarms panel lists the deduplicated,
// accountable alarms with their honest states, the toolbar chip shows the
// needs-attention count, and the lifecycle verbs (acknowledge / escalate /
// clear) are invoked here — normally THROUGH the command envelope (alarm.ack /
// alarm.escalate / alarm.clear, inc 25), so a palette line, an API call, and a
// panel button are all the same audited action. Device health transitions are
// the first event source (wired from DeviceController's live feed under three
// default rules). It notifies, never actuates (P6-05 is its own gate).

#include <QObject>
#include <QString>
#include <QVariantList>

#include "events/AlarmEngine.h"

class AlarmController : public QObject {
    Q_OBJECT
    // Active alarms, newest first: { id, device, priority, state, stateText,
    // count, message, time, suppressed }.
    Q_PROPERTY(QVariantList alarms READ alarms NOTIFY changed)
    // Unsuppressed alarms demanding an operator (New or Escalated).
    Q_PROPERTY(int needsAttention READ needsAttention NOTIFY changed)

public:
    explicit AlarmController(QObject* parent = nullptr);

    QVariantList alarms() const { return model_; }
    int needsAttention() const { return engine_.needsAttention(); }

    // First event source: device health transitions (offline / degraded /
    // recovered) from DeviceController's live feed. Stamps the current time.
    Q_INVOKABLE void onHealthEvent(const QString& deviceId,
                                   const QString& eventType,
                                   const QString& message);

    // Lifecycle. Return false on an unknown/cleared alarm id (honest refusal;
    // the command envelope maps that to a failed result).
    Q_INVOKABLE bool acknowledge(double alarmId);
    Q_INVOKABLE bool escalate(double alarmId);
    Q_INVOKABLE bool clearAlarm(double alarmId);
    Q_INVOKABLE bool assign(double alarmId, const QString& operatorId);

    // Maintenance suppression follows the Devices tab's maintenance toggle.
    Q_INVOKABLE void setDeviceMaintenance(const QString& deviceId, bool on);

    vms::events::AlarmEngine& engine() { return engine_; }

signals:
    void changed();

private:
    void rebuild();

    vms::events::AlarmEngine engine_;
    QVariantList model_;
};
