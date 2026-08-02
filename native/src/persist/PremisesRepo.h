#pragma once

// P3-15 premises persistence (native increment 31). Qt-free canonical site /
// floor metadata and the active floor for a workspace. Plan images stay as
// external URIs; this repository never reads or copies them.

#include <string>

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

private:
    Store& store_;
};

} // namespace vms::persist
