#pragma once

// P0-04 persistence — the canonical native schema (increment 5b).
//
// One source of truth for the forward migrations, used by both the application
// (`vms_workspace`) and the self-check (`vms_dbtest`), so the tested schema is
// exactly the shipped schema. Only the entities the native client needs now
// (proposal §5); the rest of the reference `docs/sqlite-schema.sql` is deferred
// until a feature needs it. Credential *secrets* are the next gate (DPAPI); this
// schema stores a `credential_ref` only.

#include <vector>

#include "persist/Store.h"

namespace vms::persist {

// The ordered forward migrations that define the current native schema.
std::vector<Migration> coreMigrations();

} // namespace vms::persist
