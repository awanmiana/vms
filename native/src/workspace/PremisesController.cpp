#include "PremisesController.h"

#include <QDate>
#include <QTimeZone>
#include <QVariantList>

namespace {

QVariantMap unavailableHours(const QString& reason) {
    return {{QStringLiteral("available"), false},
            {QStringLiteral("stateText"), QStringLiteral("Unavailable")},
            {QStringLiteral("reason"), reason}};
}

int weekdayNumber(const QString& value) {
    const QString day = value.trimmed().toLower();
    static const QStringList names = {
        QStringLiteral("monday"), QStringLiteral("tuesday"),
        QStringLiteral("wednesday"), QStringLiteral("thursday"),
        QStringLiteral("friday"), QStringLiteral("saturday"),
        QStringLiteral("sunday")};
    const int longIndex = names.indexOf(day);
    if (longIndex >= 0) return longIndex + 1;
    static const QStringList shortNames = {
        QStringLiteral("mon"), QStringLiteral("tue"),
        QStringLiteral("wed"), QStringLiteral("thu"),
        QStringLiteral("fri"), QStringLiteral("sat"),
        QStringLiteral("sun")};
    const int shortIndex = shortNames.indexOf(day);
    return shortIndex >= 0 ? shortIndex + 1 : 0;
}

int clockMinute(const QString& value, bool allow24) {
    const QStringList parts = value.trimmed().split(QLatin1Char(':'));
    if (parts.size() != 2 || parts[0].size() != 2 || parts[1].size() != 2)
        return -1;
    bool hourOk = false, minuteOk = false;
    const int hour = parts[0].toInt(&hourOk);
    const int minute = parts[1].toInt(&minuteOk);
    if (!hourOk || !minuteOk || minute < 0 || minute > 59) return -1;
    if (hour == 24 && allow24 && minute == 0) return 1440;
    if (hour < 0 || hour > 23) return -1;
    return hour * 60 + minute;
}

QString minuteText(int minute) {
    if (minute == 1440) return QStringLiteral("24:00");
    return QStringLiteral("%1:%2")
        .arg(minute / 60, 2, 10, QLatin1Char('0'))
        .arg(minute % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

PremisesController::PremisesController(vms::persist::PremisesRepo* repo,
                                       QString workspace, QObject* parent)
    : QObject(parent), repo_(repo), workspace_(std::move(workspace)) {
    if (repo_) {
        repo_->ensureDefault(workspace_.toStdString());
        reload();
    }
}

QString PremisesController::reload() {
    if (!repo_) return QStringLiteral("no premises store attached");
    vms::persist::PremisesState state;
    bool found = false;
    const vms::persist::Error e =
        repo_->loadActive(workspace_.toStdString(), state, found);
    if (!e) return QString::fromStdString(e.message);
    if (!found) return QStringLiteral("no active premises");
    siteId_ = QString::fromStdString(state.siteId);
    siteName_ = QString::fromStdString(state.siteName);
    timezone_ = QString::fromStdString(state.timezone);
    floorId_ = QString::fromStdString(state.floorId);
    floorName_ = QString::fromStdString(state.floorName);
    planUri_ = QString::fromStdString(state.planUri);
    worldWidth_ = state.worldWidth;
    worldHeight_ = state.worldHeight;
    vms::persist::OperatingSchedule schedule;
    if (const vms::persist::Error e =
            repo_->loadOperatingSchedule(state.siteId, schedule); !e)
        return QString::fromStdString(e.message);
    operatingSchedule_ = std::move(schedule);
    emit changed();
    return {};
}

QString PremisesController::configureSite(const QString& id,
                                          const QString& name,
                                          const QString& timezone) {
    if (!repo_) return QStringLiteral("no premises store attached");
    const vms::persist::Error e = repo_->configureSite(
        workspace_.toStdString(), id.toStdString(), name.toStdString(),
        timezone.toStdString());
    if (!e) return QString::fromStdString(e.message);
    return reload();
}

QString PremisesController::configureFloor(const QString& id,
                                           const QString& siteId,
                                           const QString& name,
                                           const QString& planUri,
                                           double width, double height) {
    if (!repo_) return QStringLiteral("no premises store attached");
    const vms::persist::Error e = repo_->configureFloor(
        workspace_.toStdString(), id.toStdString(), siteId.toStdString(),
        name.toStdString(), planUri.toStdString(), width, height);
    if (!e) return QString::fromStdString(e.message);
    return reload();
}

QString PremisesController::addOperatingWindow(const QString& weekday,
                                               const QString& start,
                                               const QString& end) {
    if (!repo_) return QStringLiteral("no premises store attached");
    if (siteId_.isEmpty()) return QStringLiteral("no active premises");
    const int day = weekdayNumber(weekday);
    const int startMinute = clockMinute(start, false);
    const int endMinute = clockMinute(end, true);
    if (day == 0) return QStringLiteral("weekday must be mon..sun");
    if (startMinute < 0 || endMinute < 0)
        return QStringLiteral("hours must use HH:MM (24:00 only as end)");
    const vms::persist::Error e = repo_->addWeeklyWindow(
        siteId_.toStdString(), day, startMinute, endMinute);
    if (!e) return QString::fromStdString(e.message);
    return reload();
}

QString PremisesController::clearOperatingDay(const QString& weekday) {
    if (!repo_) return QStringLiteral("no premises store attached");
    if (siteId_.isEmpty()) return QStringLiteral("no active premises");
    const int day = weekdayNumber(weekday);
    if (day == 0) return QStringLiteral("weekday must be mon..sun");
    const vms::persist::Error e =
        repo_->clearWeeklyDay(siteId_.toStdString(), day);
    if (!e) return QString::fromStdString(e.message);
    return reload();
}

QString PremisesController::setSpecialHours(const QString& localDate,
                                            const QString& start,
                                            const QString& end,
                                            const QString& label) {
    if (!repo_) return QStringLiteral("no premises store attached");
    if (siteId_.isEmpty()) return QStringLiteral("no active premises");
    const int startMinute = clockMinute(start, false);
    const int endMinute = clockMinute(end, true);
    if (startMinute < 0 || endMinute < 0)
        return QStringLiteral("hours must use HH:MM (24:00 only as end)");
    const vms::persist::Error e = repo_->setDateException(
        siteId_.toStdString(), localDate.toStdString(), false, startMinute,
        endMinute, label.toStdString());
    if (!e) return QString::fromStdString(e.message);
    return reload();
}

QString PremisesController::setHolidayClosed(const QString& localDate,
                                             const QString& label) {
    if (!repo_) return QStringLiteral("no premises store attached");
    if (siteId_.isEmpty()) return QStringLiteral("no active premises");
    const vms::persist::Error e = repo_->setDateException(
        siteId_.toStdString(), localDate.toStdString(), true, 0, 0,
        label.toStdString());
    if (!e) return QString::fromStdString(e.message);
    return reload();
}

QString PremisesController::clearDateException(const QString& localDate) {
    if (!repo_) return QStringLiteral("no premises store attached");
    if (siteId_.isEmpty()) return QStringLiteral("no active premises");
    const vms::persist::Error e = repo_->clearDateException(
        siteId_.toStdString(), localDate.toStdString());
    if (!e) return QString::fromStdString(e.message);
    return reload();
}

QVariantMap PremisesController::operatingHoursAtUtc(const QDateTime& utc) const {
    if (siteId_.isEmpty())
        return unavailableHours(QStringLiteral("No active premises"));
    if (!operatingSchedule_.configured)
        return unavailableHours(QStringLiteral(
            "Operating hours and holiday schedules are not configured"));
    const QTimeZone zone(timezone_.toUtf8());
    if (!zone.isValid())
        return unavailableHours(QStringLiteral("Configured timezone is invalid"));
    if (!utc.isValid())
        return unavailableHours(QStringLiteral("Evaluation instant is invalid"));

    const QDateTime local = utc.toUTC().toTimeZone(zone);
    const QString date = local.date().toString(Qt::ISODate);
    const int minute = local.time().hour() * 60 + local.time().minute();
    const int weekday = local.date().dayOfWeek();
    std::vector<vms::persist::OperatingWindow> windows;
    QString source = QStringLiteral("Weekly schedule");
    QString label;

    for (const auto& exception : operatingSchedule_.exceptions) {
        if (QString::fromStdString(exception.localDate) != date) continue;
        source = exception.closed ? QStringLiteral("Date closed")
                                  : QStringLiteral("Special hours");
        label = QString::fromStdString(exception.label);
        if (!exception.closed) windows = exception.windows;
        break;
    }
    if (source == QLatin1String("Weekly schedule")) {
        for (const auto& window : operatingSchedule_.weekly)
            if (window.weekday == weekday) windows.push_back(window);
    }

    bool open = false;
    QStringList ranges;
    QVariantList windowList;
    for (const auto& window : windows) {
        const QString range = minuteText(window.startMinute) +
                              QStringLiteral("–") +
                              minuteText(window.endMinute);
        ranges.push_back(range);
        windowList.push_back(QVariantMap{
            {QStringLiteral("startMinute"), window.startMinute},
            {QStringLiteral("endMinute"), window.endMinute},
            {QStringLiteral("text"), range}});
        if (minute >= window.startMinute && minute < window.endMinute)
            open = true;
    }
    const QString todayHours = ranges.isEmpty()
        ? QStringLiteral("Closed") : ranges.join(QStringLiteral(", "));
    return {{QStringLiteral("available"), true},
            {QStringLiteral("open"), open},
            {QStringLiteral("stateText"),
             open ? QStringLiteral("Open") : QStringLiteral("Closed")},
            {QStringLiteral("source"), source},
            {QStringLiteral("label"), label},
            {QStringLiteral("todayHours"), todayHours},
            {QStringLiteral("localDate"), date},
            {QStringLiteral("timezone"), timezone_},
            {QStringLiteral("windows"), windowList}};
}
