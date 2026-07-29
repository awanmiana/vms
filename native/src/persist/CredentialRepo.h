#pragma once

// Connection broker + credential store — native increment 6b (credential
// wiring + atomicity). Scope + rationale: ../credential-broker-P1-03-proposal.md.
//
// Binds the SQLite `device_credentials.credential_ref` handle (Schema.cpp) to
// the actual secret held by a `SecretStore`. The DB stores ONLY the opaque
// `credential_ref`; the plaintext secret never enters SQLite. The repository is
// the one place that mints/looks up the ref and pairs the two stores so a
// provision is atomic — both the ref row and the encrypted secret are written,
// or neither is (acceptance §4). Qt-free; depends only on Store + SecretStore.

#include <string>

#include "persist/Store.h"
#include "persist/SecretStore.h"

namespace vms::persist {

// Pairs the relational store (the credential_ref row) with the secret store (the
// encrypted secret) behind one atomic interface. A device is identified by its
// `devices.id`; its credential is reachable only through this repository.
class CredentialRepo {
public:
    CredentialRepo(Store& store, SecretStore& secrets)
        : store_(store), secrets_(secrets) {}

    // Provision (or replace) a device's credential. Reuses the device's existing
    // credential_ref if it has one, otherwise mints a fresh opaque ref. Writes
    // the device_credentials row and the encrypted secret together: on a FRESH
    // provision, a failure of either leaves neither (the DB transaction rolls
    // back the ref row; a failed commit removes the just-written secret). The
    // device row must already exist (device_credentials.device_id is an FK).
    Error put(const std::string& deviceId, const std::string& secret);

    // The opaque credential_ref stored for a device. Status::NotFound if the
    // device has no credential row. No decryption is performed.
    Error refFor(const std::string& deviceId, std::string& outRef);

    // Resolve a device's secret: look up its credential_ref, then decrypt via the
    // SecretStore. Status::NotFound if the device has no credential; Status::Crypto
    // if the secret exists but cannot be decrypted (honest failure, P0-03C/E).
    Error getSecret(const std::string& deviceId, std::string& outSecret);

    // Remove a device's credential: delete the ref row and the secret. Idempotent
    // (removing an absent credential succeeds). Atomic for the common path; a
    // partial failure leaves at most an inaccessible secret, never exposed plaintext.
    Error remove(const std::string& deviceId);

    // Deterministic opaque handle for a device. Exposed for tests/diagnostics;
    // callers treat the ref as opaque and never build one themselves.
    static std::string mintRef(const std::string& deviceId);

private:
    Store& store_;
    SecretStore& secrets_;
};

} // namespace vms::persist
