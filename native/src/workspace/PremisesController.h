#pragma once

// P3-15 premises bridge (native increment 31). Read-only QML properties over
// the canonical repository; all mutations enter through CommandController.

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

#include "persist/PremisesRepo.h"

class PremisesController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString siteId READ siteId NOTIFY changed)
    Q_PROPERTY(QString siteName READ siteName NOTIFY changed)
    Q_PROPERTY(QString timezone READ timezone NOTIFY changed)
    Q_PROPERTY(QString floorId READ floorId NOTIFY changed)
    Q_PROPERTY(QString floorName READ floorName NOTIFY changed)
    Q_PROPERTY(QString planUri READ planUri NOTIFY changed)
    Q_PROPERTY(double worldWidth READ worldWidth NOTIFY changed)
    Q_PROPERTY(double worldHeight READ worldHeight NOTIFY changed)

public:
    PremisesController(vms::persist::PremisesRepo* repo,
                       QString workspace = QStringLiteral("live"),
                       QObject* parent = nullptr);

    QString siteId() const { return siteId_; }
    QString siteName() const { return siteName_; }
    QString timezone() const { return timezone_; }
    QString floorId() const { return floorId_; }
    QString floorName() const { return floorName_; }
    QString planUri() const { return planUri_; }
    double worldWidth() const { return worldWidth_; }
    double worldHeight() const { return worldHeight_; }

    QString configureSite(const QString& id, const QString& name,
                          const QString& timezone);
    QString configureFloor(const QString& id, const QString& siteId,
                           const QString& name, const QString& planUri,
                           double width, double height);
    QString addOperatingWindow(const QString& weekday, const QString& start,
                               const QString& end);
    QString clearOperatingDay(const QString& weekday);
    QString setSpecialHours(const QString& localDate, const QString& start,
                            const QString& end, const QString& label);
    QString setHolidayClosed(const QString& localDate, const QString& label);
    QString clearDateException(const QString& localDate);

    // Deterministic UTC input keeps DST behavior directly self-testable. The
    // front-layer aggregate supplies currentDateTimeUtc().
    QVariantMap operatingHoursAtUtc(const QDateTime& utc) const;

signals:
    void changed();

private:
    QString reload();

    vms::persist::PremisesRepo* repo_ = nullptr;
    QString workspace_;
    QString siteId_, siteName_, timezone_, floorId_, floorName_, planUri_;
    double worldWidth_ = 1600.0;
    double worldHeight_ = 900.0;
    vms::persist::OperatingSchedule operatingSchedule_;
};
