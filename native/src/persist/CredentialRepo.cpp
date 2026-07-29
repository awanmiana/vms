#include "persist/CredentialRepo.h"

namespace vms::persist {

std::string CredentialRepo::mintRef(const std::string& deviceId) {
    // An opaque, stable, per-device handle. The scheme is irrelevant to callers;
    // it only has to be unique per device and never itself be a secret.
    return "cred://device/" + deviceId;
}

Error CredentialRepo::refFor(const std::string& deviceId, std::string& outRef) {
    outRef.clear();
    Result r;
    if (Error e = store_.query(
            "SELECT credential_ref FROM device_credentials WHERE device_id=?;",
            {deviceId}, r);
        !e)
        return e;
    if (r.rows.empty())
        return {Status::NotFound, "no credential for device " + deviceId};
    outRef = std::get<std::string>(r.rows[0][0]);
    return Error::success();
}

Error CredentialRepo::put(const std::string& deviceId,
                          const std::string& secret) {
    if (deviceId.empty())
        return {Status::Misuse, "empty deviceId"};

    // Reuse an existing ref (stable handle) or mint a new one. Whether the ref
    // pre-existed decides how we compensate a late failure below.
    std::string ref;
    bool hadRef = false;
    if (Error e = refFor(deviceId, ref); e) {
        hadRef = true;                 // device already had a credential_ref
    } else if (e.status == Status::NotFound) {
        ref = mintRef(deviceId);       // first provision for this device
    } else {
        return e;                      // a real query error
    }

    // Order for both-or-neither on a fresh provision:
    //   1. begin tx and write the ref row (uncommitted)
    //   2. write the encrypted secret; if that fails, the tx rolls back on scope
    //      exit -> no ref row is left behind
    //   3. commit; if the commit itself fails, remove the just-written secret so
    //      a fresh provision leaves no orphaned secret either
    Store::Tx tx(store_);
    if (Error e = tx.begin(); !e) return e;

    if (Error e = store_.exec(
            "INSERT INTO device_credentials(device_id, credential_ref) "
            "VALUES(?, ?) ON CONFLICT(device_id) DO UPDATE SET "
            "credential_ref=excluded.credential_ref;",
            {deviceId, ref});
        !e)
        return e;   // e.g. FK violation (unknown device) -> Constraint; no secret written

    if (Error e = secrets_.put(ref, secret); !e)
        return e;   // tx destructor rolls back the ref row

    if (Error e = tx.commit(); !e) {
        // The ref row is rolled back by the failed commit path; undo the secret
        // too, but only if this was a fresh provision (otherwise we would erase a
        // previously valid secret that the rolled-back row still points at).
        if (!hadRef) secrets_.remove(ref);
        return e;
    }
    return Error::success();
}

Error CredentialRepo::getSecret(const std::string& deviceId,
                                std::string& outSecret) {
    outSecret.clear();
    std::string ref;
    if (Error e = refFor(deviceId, ref); !e) return e;  // NotFound propagates
    return secrets_.get(ref, outSecret);                // Crypto/NotFound propagate
}

Error CredentialRepo::remove(const std::string& deviceId) {
    std::string ref;
    Error found = refFor(deviceId, ref);
    if (found.status == Status::NotFound)
        return Error::success();     // idempotent: nothing to remove
    if (!found) return found;

    // Delete the ref row first (committed), then drop the secret. If the secret
    // removal fails, at most an inaccessible orphan secret remains — never
    // exposed plaintext, and a later put/remove for the same device cleans it up.
    {
        Store::Tx tx(store_);
        if (Error e = tx.begin(); !e) return e;
        if (Error e = store_.exec(
                "DELETE FROM device_credentials WHERE device_id=?;", {deviceId});
            !e)
            return e;
        if (Error e = tx.commit(); !e) return e;
    }
    return secrets_.remove(ref);
}

} // namespace vms::persist
