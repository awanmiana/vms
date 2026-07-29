#pragma once

// Connection broker + credential store — native increment 6a (the OS secret
// store). Scope + rationale: ../credential-broker-P1-03-proposal.md.
//
// The store keeps device secrets keyed by an opaque `credential_ref` (the same
// handle the SQLite `device_credentials.credential_ref` column holds). Secrets
// are CIPHERTEXT at rest and plaintext only in memory for the lifetime of a
// call; the caller is expected to use and discard the plaintext promptly.
//
// `SecretStore` is a technology-neutral interface so the backend is swappable
// (Windows DPAPI now; libsecret/keyring on Linux later) and unit-testable in
// isolation via an in-memory double — exactly as `Store` is for SQLite. It is
// Qt-free and SQLite-free; it shares only the typed `Error` vocabulary.

#include <map>
#include <string>

#include "persist/Error.h"

namespace vms::persist {

// The secret-store abstraction. Implementations must never persist plaintext.
class SecretStore {
public:
    virtual ~SecretStore() = default;

    // Encrypt and store `secret` under `ref`, replacing any existing secret for
    // that ref. `secret` is the caller's plaintext and is never written in clear.
    virtual Error put(const std::string& ref, const std::string& secret) = 0;

    // Decrypt and return the secret for `ref`. Status::NotFound if no secret is
    // stored for ref; Status::Crypto if one exists but cannot be decrypted (e.g.
    // a DPAPI blob copied to a different user/machine, or tampered ciphertext).
    virtual Error get(const std::string& ref, std::string& out) = 0;

    // Remove the secret for `ref`. Removing an absent ref succeeds (idempotent).
    virtual Error remove(const std::string& ref) = 0;

    // Whether a secret is stored for `ref` (no decryption performed).
    virtual bool contains(const std::string& ref) const = 0;
};

// In-memory test double: holds secrets in a map for the object's lifetime and
// never touches disk or the OS. Used by the self-check and by any test that
// needs a SecretStore without the real OS store. It performs no encryption
// because it never persists — there is no "at rest" to protect.
class InMemorySecretStore : public SecretStore {
public:
    Error put(const std::string& ref, const std::string& secret) override;
    Error get(const std::string& ref, std::string& out) override;
    Error remove(const std::string& ref) override;
    bool contains(const std::string& ref) const override;

private:
    std::map<std::string, std::string> secrets_;
};

#ifdef _WIN32
// Windows DPAPI implementation. Secrets are encrypted with CryptProtectData and
// stored as base64 ciphertext in a small backing file (one `ref<TAB>cipher` line
// per secret); the plaintext never reaches the file. By default the blob is
// bound to the current Windows USER (only that user can decrypt); pass
// machineScope=true (CRYPTPROTECT_LOCAL_MACHINE) for a service that must decrypt
// regardless of the logged-in user — a deployment choice, documented here.
class DpapiSecretStore : public SecretStore {
public:
    // `backingFile` is where the ciphertext key/value lines live (created on
    // first put). Call open() before use.
    explicit DpapiSecretStore(std::string backingFile, bool machineScope = false);

    // Load any existing ciphertext entries from the backing file. A missing file
    // is not an error (first run). Returns Status::Io on an unreadable file.
    Error open();

    Error put(const std::string& ref, const std::string& secret) override;
    Error get(const std::string& ref, std::string& out) override;
    Error remove(const std::string& ref) override;
    bool contains(const std::string& ref) const override;

private:
    Error flush();   // atomically rewrite the backing file (temp + rename)

    std::string path_;
    bool machineScope_ = false;
    std::map<std::string, std::string> cipher_;   // ref -> base64(ciphertext)
    bool loaded_ = false;
};
#endif  // _WIN32

} // namespace vms::persist
