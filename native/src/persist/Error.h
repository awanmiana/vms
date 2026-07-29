#pragma once

// Shared typed-error vocabulary for the native persistence layer (the C0-03
// "typed persistence errors" requirement). Extracted from Store.h so the SQLite
// store (Store) and the OS secret store (SecretStore, native increment 6) share
// one status/error type without the secret store depending on SQLite.

#include <string>

namespace vms::persist {

// Typed persistence status.
enum class Status {
    Ok,
    NotFound,     // an expected row/secret was absent
    Constraint,   // uniqueness / FK / check violation
    Busy,         // database locked
    Io,           // disk / file error
    Migration,    // a migration step failed
    Misuse,       // API used incorrectly (e.g. store not open)
    Crypto,       // encrypt/decrypt failed (secret store; e.g. wrong user/machine)
    Unavailable,  // temporarily refused: circuit open / device locked out / pool full
    Error,        // any other failure
};

inline const char* StatusName(Status s) {
    switch (s) {
        case Status::Ok:         return "ok";
        case Status::NotFound:   return "not-found";
        case Status::Constraint: return "constraint";
        case Status::Busy:       return "busy";
        case Status::Io:         return "io";
        case Status::Migration:  return "migration";
        case Status::Misuse:     return "misuse";
        case Status::Crypto:     return "crypto";
        case Status::Unavailable: return "unavailable";
        case Status::Error:      return "error";
    }
    return "?";
}

struct Error {
    Status status = Status::Ok;
    std::string message;
    int sqliteCode = 0;   // native code from the backend (SQLite code / Win32 error)
    bool ok() const { return status == Status::Ok; }
    explicit operator bool() const { return ok(); }  // `if (err)` == success
    static Error success() { return {}; }
};

} // namespace vms::persist
