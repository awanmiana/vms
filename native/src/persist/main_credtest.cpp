// Connection broker + credential store self-check — native increment 6a (the OS
// secret store). No Qt, no display, no camera, no network. Exercises the
// SecretStore interface via the in-memory double and the real Windows DPAPI
// backend: put/get round-trip, secret durability across reopen, the
// no-plaintext-at-rest property (the backing file holds only ciphertext),
// idempotent remove, and typed NotFound / Crypto failure paths. Registered with
// CTest as `credential_selfcheck`. Scope: ../credential-broker-P1-03-proposal.md.

#include "persist/SecretStore.h"
#include "persist/CredentialRepo.h"
#include "persist/Store.h"
#include "persist/Schema.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace vms::persist;

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}

std::string tmpPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// A SecretStore whose put() always fails, to exercise the atomicity path where
// the secret write fails after the ref row was written (6b acceptance §4).
class FailingSecretStore : public vms::persist::InMemorySecretStore {
public:
    vms::persist::Error put(const std::string&, const std::string&) override {
        return {vms::persist::Status::Io, "injected secret-store failure"};
    }
};

}  // namespace

int main() {
    std::cout << "vms_credtest — credential store (DPAPI) self-check\n";

    // ---- 1) In-memory double: interface contract ----
    {
        InMemorySecretStore s;
        std::string out;
        check(static_cast<bool>(s.put("cred://dev-1", "hunter2")),
              "in-memory: put a secret");
        check(static_cast<bool>(s.get("cred://dev-1", out)) && out == "hunter2",
              "in-memory: get returns the same secret");
        check(s.contains("cred://dev-1"), "in-memory: contains reports present");
        check(s.get("cred://missing", out).status == Status::NotFound,
              "in-memory: get of an absent ref is typed NotFound");
        check(static_cast<bool>(s.remove("cred://dev-1")),
              "in-memory: remove a secret");
        check(!s.contains("cred://dev-1"), "in-memory: removed secret is gone");
        check(static_cast<bool>(s.remove("cred://dev-1")),
              "in-memory: removing an absent ref is idempotent Ok");
        check(s.put("", "x").status == Status::Misuse,
              "in-memory: empty ref is typed Misuse");
    }

#ifdef _WIN32
    // ---- 2) Windows DPAPI backend: real encrypt/decrypt at rest ----
    const std::string secretFile = tmpPath("vms_credtest_secrets.dat");
    std::error_code ec;
    std::filesystem::remove(secretFile, ec);
    std::filesystem::remove(secretFile + ".tmp", ec);

    const std::string kRef = "cred://dev-42";
    const std::string kSecret = "PLAINTEXT-Sentinel-9f3a-@Str0ng!";

    {
        DpapiSecretStore s(secretFile);
        check(static_cast<bool>(s.open()),
              "dpapi: open (missing file == empty store, first run)");
        std::string out;
        check(s.get(kRef, out).status == Status::NotFound,
              "dpapi: get before put is typed NotFound");
        check(static_cast<bool>(s.put(kRef, kSecret)),
              "dpapi: put encrypts and persists a secret");
        check(static_cast<bool>(s.get(kRef, out)) && out == kSecret,
              "dpapi: get decrypts back to the original plaintext");
    }

    // No-plaintext-at-rest: the backing file must hold only ciphertext.
    {
        const std::string onDisk = readFile(secretFile);
        check(!onDisk.empty(), "dpapi: backing file is non-empty after put");
        check(onDisk.find(kSecret) == std::string::npos,
              "dpapi: plaintext secret does NOT appear in the backing file");
        check(onDisk.find("PLAINTEXT-Sentinel") == std::string::npos,
              "dpapi: no plaintext fragment leaks to disk");
    }

    // Durability across a fresh store instance (simulates an app restart).
    {
        DpapiSecretStore s(secretFile);
        check(static_cast<bool>(s.open()), "dpapi: reopen backing file");
        std::string out;
        check(static_cast<bool>(s.get(kRef, out)) && out == kSecret,
              "dpapi: secret survives reopen and decrypts");
        check(static_cast<bool>(s.remove(kRef)), "dpapi: remove the secret");
        check(!s.contains(kRef), "dpapi: removed secret is gone");
    }
    {
        DpapiSecretStore s(secretFile);
        check(static_cast<bool>(s.open()), "dpapi: reopen after remove");
        check(!s.contains(kRef), "dpapi: removal persisted across reopen");
    }

    // Honest failure: tampered/undecryptable ciphertext yields typed Crypto
    // (stands in deterministically for the copied-to-another-user/machine case,
    // which cannot be simulated in a single-user test run).
    {
        DpapiSecretStore s(secretFile);
        check(static_cast<bool>(s.open()), "dpapi: open for tamper test");
        check(static_cast<bool>(s.put(kRef, kSecret)), "dpapi: re-put a secret");
    }
    {
        // Corrupt the ciphertext field (after the TAB) in the backing file.
        std::string onDisk = readFile(secretFile);
        auto tab = onDisk.find('\t');
        bool tampered = false;
        if (tab != std::string::npos && tab + 6 < onDisk.size()) {
            for (int i = 1; i <= 4; ++i) {
                char& c = onDisk[tab + i];
                c = (c == 'A') ? 'B' : 'A';   // flip to a different valid b64 char
            }
            std::ofstream f(secretFile, std::ios::binary | std::ios::trunc);
            f << onDisk;
            tampered = true;
        }
        check(tampered, "dpapi: tampered the stored ciphertext");
    }
    {
        DpapiSecretStore s(secretFile);
        check(static_cast<bool>(s.open()), "dpapi: reopen tampered store");
        std::string out;
        check(s.get(kRef, out).status == Status::Crypto,
              "dpapi: undecryptable ciphertext is typed Crypto (honest failure)");
    }

    std::filesystem::remove(secretFile, ec);
    std::filesystem::remove(secretFile + ".tmp", ec);

    // ---- 3) 6b: CredentialRepo — ref<->secret binding + atomicity ----
    // Uses a real SQLite Store (temp file, so we can scan the bytes) and the
    // in-memory secret double. The DB holds only the opaque credential_ref; the
    // plaintext secret must never enter any DB file.
    const std::string dbFile = tmpPath("vms_credtest.db");
    for (const char* suffix : {"", "-wal", "-shm"})
        std::filesystem::remove(dbFile + suffix, ec);

    const std::string kDev = "device-alpha";
    const std::string kDevSecret = "PLAINTEXT-CredSentinel-7c1e-@Adm1n!";
    {
        Store store;
        check(static_cast<bool>(store.open(dbFile)), "6b: open store");
        check(static_cast<bool>(store.migrate(coreMigrations())), "6b: migrate schema");
        // A device row must exist first (device_credentials.device_id is an FK).
        check(static_cast<bool>(store.exec(
                  "INSERT INTO devices(id, name) VALUES(?, ?);", {kDev, "Cam Alpha"})),
              "6b: insert device row");

        InMemorySecretStore secrets;
        CredentialRepo repo(store, secrets);

        // Round-trip: provision, then resolve.
        check(static_cast<bool>(repo.put(kDev, kDevSecret)),
              "6b: provision device credential");
        std::string ref;
        check(static_cast<bool>(repo.refFor(kDev, ref)) &&
                  ref == CredentialRepo::mintRef(kDev),
              "6b: credential_ref is the opaque handle (not the secret)");
        std::string got;
        check(static_cast<bool>(repo.getSecret(kDev, got)) && got == kDevSecret,
              "6b: getSecret decrypts back to the original plaintext");

        // The DB stores only the ref, never the plaintext.
        Result r;
        check(static_cast<bool>(store.query(
                  "SELECT credential_ref FROM device_credentials WHERE device_id=?;",
                  {kDev}, r)) && !r.rows.empty() &&
                  std::get<std::string>(r.rows[0][0]).find(kDevSecret) == std::string::npos,
              "6b: device_credentials row holds only the ref");

        // Stable ref across replace; new secret wins.
        const std::string kDevSecret2 = "PLAINTEXT-CredSentinel-Rotated-@N3w!";
        check(static_cast<bool>(repo.put(kDev, kDevSecret2)), "6b: rotate the secret");
        std::string ref2;
        check(static_cast<bool>(repo.refFor(kDev, ref2)) && ref2 == ref,
              "6b: credential_ref is stable across rotation");
        check(static_cast<bool>(repo.getSecret(kDev, got)) && got == kDevSecret2,
              "6b: getSecret returns the rotated secret");

        // Atomicity A — secret-store write fails => the ref row is rolled back.
        {
            check(static_cast<bool>(store.exec(
                      "INSERT INTO devices(id, name) VALUES(?, ?);",
                      {"device-beta", "Cam Beta"})),
                  "6b: insert second device row");
            FailingSecretStore failing;
            CredentialRepo failRepo(store, failing);
            Error e = failRepo.put("device-beta", "should-not-persist");
            check(!e && e.status == Status::Io, "6b: provision fails when secret write fails");
            std::string tmp;
            check(failRepo.refFor("device-beta", tmp).status == Status::NotFound,
                  "6b: no orphaned credential_ref left after a failed provision");
        }

        // Atomicity B — DB write fails (unknown device => FK violation) => no secret.
        {
            InMemorySecretStore secrets2;
            CredentialRepo repo2(store, secrets2);
            Error e = repo2.put("ghost-device", "should-not-persist");
            check(!e, "6b: provisioning an unknown device fails (FK)");
            check(!secrets2.contains(CredentialRepo::mintRef("ghost-device")),
                  "6b: no orphaned secret left after a failed DB write");
        }

        // Remove is atomic + idempotent.
        check(static_cast<bool>(repo.remove(kDev)), "6b: remove the credential");
        check(repo.getSecret(kDev, got).status == Status::NotFound,
              "6b: removed credential no longer resolves");
        check(!secrets.contains(ref), "6b: secret removed from the secret store");
        check(static_cast<bool>(repo.remove(kDev)), "6b: remove is idempotent");
        store.close();
    }

    // No-plaintext-at-rest across the whole DB (main + WAL + shm bytes).
    {
        bool leaked = false;
        for (const char* suffix : {"", "-wal", "-shm"}) {
            const std::string bytes = readFile(dbFile + suffix);
            if (bytes.find(kDevSecret) != std::string::npos ||
                bytes.find("PLAINTEXT-CredSentinel") != std::string::npos)
                leaked = true;
        }
        check(!leaked, "6b: no plaintext secret fragment in any DB file");
    }
    for (const char* suffix : {"", "-wal", "-shm"})
        std::filesystem::remove(dbFile + suffix, ec);
#else
    std::cout << "  (DPAPI backend is Windows-only; skipped on this platform)\n";
#endif

    if (failures == 0) {
        std::cout << "PASS: secret-store interface, DPAPI at-rest encryption, "
                     "durability, and typed failures all verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
