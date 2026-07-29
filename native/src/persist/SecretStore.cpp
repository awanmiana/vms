#include "persist/SecretStore.h"

#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>   // DATA_BLOB
#include <dpapi.h>      // CryptProtectData / CryptUnprotectData
#endif

namespace vms::persist {

namespace {

// --- base64 (no external dependency) -------------------------------------
const char* kB64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64encode(const std::string& in) {
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= in.size()) {
        unsigned n = (static_cast<unsigned char>(in[i]) << 16) |
                     (static_cast<unsigned char>(in[i + 1]) << 8) |
                     static_cast<unsigned char>(in[i + 2]);
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back(kB64[(n >> 6) & 63]);
        out.push_back(kB64[n & 63]);
        i += 3;
    }
    if (i + 1 == in.size()) {
        unsigned n = static_cast<unsigned char>(in[i]) << 16;
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    } else if (i + 2 == in.size()) {
        unsigned n = (static_cast<unsigned char>(in[i]) << 16) |
                     (static_cast<unsigned char>(in[i + 1]) << 8);
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back(kB64[(n >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

bool b64decode(const std::string& in, std::string& out) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    out.clear();
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        int v = val(c);
        if (v < 0) return false;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return true;
}

}  // namespace

// --- InMemorySecretStore --------------------------------------------------

Error InMemorySecretStore::put(const std::string& ref, const std::string& secret) {
    if (ref.empty())
        return {Status::Misuse, "empty credential_ref", 0};
    secrets_[ref] = secret;
    return Error::success();
}

Error InMemorySecretStore::get(const std::string& ref, std::string& out) {
    auto it = secrets_.find(ref);
    if (it == secrets_.end())
        return {Status::NotFound, "no secret for ref", 0};
    out = it->second;
    return Error::success();
}

Error InMemorySecretStore::remove(const std::string& ref) {
    secrets_.erase(ref);   // absent ref -> still Ok (idempotent)
    return Error::success();
}

bool InMemorySecretStore::contains(const std::string& ref) const {
    return secrets_.find(ref) != secrets_.end();
}

#ifdef _WIN32

// --- DpapiSecretStore -----------------------------------------------------

namespace {

// DPAPI encrypt/decrypt over std::string bytes. `machineScope` selects
// CRYPTPROTECT_LOCAL_MACHINE. Returns false on failure (caller maps to Crypto),
// setting `win32err` to GetLastError().
bool dpapiProtect(const std::string& plain, bool machineScope,
                  std::string& cipher, DWORD& win32err) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
    in.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB out{};
    DWORD flags = machineScope ? CRYPTPROTECT_LOCAL_MACHINE : 0;
    if (!CryptProtectData(&in, L"vms device credential", nullptr, nullptr,
                          nullptr, flags, &out)) {
        win32err = GetLastError();
        return false;
    }
    cipher.assign(reinterpret_cast<char*>(out.pbData), out.cbData);
    if (out.pbData) LocalFree(out.pbData);
    return true;
}

bool dpapiUnprotect(const std::string& cipher, std::string& plain,
                    DWORD& win32err) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(cipher.data()));
    in.cbData = static_cast<DWORD>(cipher.size());
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        win32err = GetLastError();
        return false;
    }
    plain.assign(reinterpret_cast<char*>(out.pbData), out.cbData);
    if (out.pbData) {
        SecureZeroMemory(out.pbData, out.cbData);
        LocalFree(out.pbData);
    }
    return true;
}

}  // namespace

DpapiSecretStore::DpapiSecretStore(std::string backingFile, bool machineScope)
    : path_(std::move(backingFile)), machineScope_(machineScope) {}

Error DpapiSecretStore::open() {
    cipher_.clear();
    std::ifstream f(path_, std::ios::binary);
    if (!f) {
        loaded_ = true;   // missing file == empty store (first run)
        return Error::success();
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string refB64 = line.substr(0, tab);
        std::string cipherB64 = line.substr(tab + 1);
        std::string ref;
        if (!b64decode(refB64, ref)) continue;
        cipher_[ref] = cipherB64;
    }
    loaded_ = true;
    return Error::success();
}

Error DpapiSecretStore::flush() {
    const std::string tmp = path_ + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f)
            return {Status::Io, "cannot open secret store for write", 0};
        for (const auto& [ref, cipherB64] : cipher_)
            f << b64encode(ref) << '\t' << cipherB64 << '\n';
        f.flush();
        if (!f)
            return {Status::Io, "write to secret store failed", 0};
    }
    // Atomic replace: rename temp over the real file.
    std::wstring wtmp(tmp.begin(), tmp.end());
    std::wstring wdst(path_.begin(), path_.end());
    if (!MoveFileExW(wtmp.c_str(), wdst.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD e = GetLastError();
        return {Status::Io, "atomic replace of secret store failed",
                static_cast<int>(e)};
    }
    return Error::success();
}

Error DpapiSecretStore::put(const std::string& ref, const std::string& secret) {
    if (!loaded_)
        return {Status::Misuse, "secret store not opened", 0};
    if (ref.empty())
        return {Status::Misuse, "empty credential_ref", 0};
    std::string cipher;
    DWORD e = 0;
    if (!dpapiProtect(secret, machineScope_, cipher, e))
        return {Status::Crypto, "CryptProtectData failed", static_cast<int>(e)};
    cipher_[ref] = b64encode(cipher);
    return flush();
}

Error DpapiSecretStore::get(const std::string& ref, std::string& out) {
    if (!loaded_)
        return {Status::Misuse, "secret store not opened", 0};
    auto it = cipher_.find(ref);
    if (it == cipher_.end())
        return {Status::NotFound, "no secret for ref", 0};
    std::string cipher;
    if (!b64decode(it->second, cipher))
        return {Status::Crypto, "corrupt ciphertext (base64)", 0};
    DWORD e = 0;
    if (!dpapiUnprotect(cipher, out, e))
        return {Status::Crypto, "CryptUnprotectData failed", static_cast<int>(e)};
    return Error::success();
}

Error DpapiSecretStore::remove(const std::string& ref) {
    if (!loaded_)
        return {Status::Misuse, "secret store not opened", 0};
    if (cipher_.erase(ref) == 0)
        return Error::success();   // absent -> idempotent Ok, no rewrite needed
    return flush();
}

bool DpapiSecretStore::contains(const std::string& ref) const {
    return cipher_.find(ref) != cipher_.end();
}

#endif  // _WIN32

} // namespace vms::persist
