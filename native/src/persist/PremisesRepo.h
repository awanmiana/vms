#pragma once

// P3-15 premises persistence (native increment 31). Qt-free canonical site /
// floor metadata and the active floor for a workspace. Plan images stay as
// external URIs; this repository never reads or copies them.

#include <string>
#include <vector>

#include "persist/Store.h"

namespace vms::persist {

struct PremisesState {
    std::string siteId;
    std::string siteName;
    std::string timezone;
    std::string floorId;
    std::string floorName;
    std::string planUri;
    double worldWidth = 1600.0;
    double worldHeight = 900.0;
};

struct OperatingWindow {
    int weekday = 0;       // ISO Monday=1 ... Sunday=7; 0 for date exceptions
    int startMinute = 0;   // local wall time, inclusive
    int endMinute = 0;     // local wall time, exclusive; 1440 means 24:00
};

struct OperatingException {
    std::string localDate; // YYYY-MM-DD in the site's timezone
    bool closed = true;
    std::string label;
    std::vector<OperatingWindow> windows;
};

struct OperatingSchedule {
    bool configured = false;
    std::vector<OperatingWindow> weekly;
    std::vector<OperatingException> exceptions;
};

class PremisesRepo {
public:
    explicit PremisesRepo(Store& store) : store_(store) {}

    Error ensureDefault(const std::string& workspace);
    Error loadActive(const std::string& workspace, PremisesState& out,
                     bool& found);
    Error configureSite(const std::string& workspace, const std::string& id,
                        const std::string& name, const std::string& timezone);
    Error configureFloor(const std::string& workspace, const std::string& id,
                         const std::string& siteId, const std::string& name,
                         const std::string& planUri, double worldWidth,
                         double worldHeight);
    Error loadOperatingSchedule(const std::string& siteId,
                                OperatingSchedule& out);
    Error addWeeklyWindow(const std::string& siteId, int weekday,
                          int startMinute, int endMinute);
    Error clearWeeklyDay(const std::string& siteId, int weekday);
    Error setDateException(const std::string& siteId,
                           const std::string& localDate, bool closed,
                           int startMinute, int endMinute,
                           const std::string& label);
    Error clearDateException(const std::string& siteId,
                             const std::string& localDate);

private:
    Store& store_;
};

} // namespace vms::persist
