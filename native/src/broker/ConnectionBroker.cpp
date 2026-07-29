#include "broker/ConnectionBroker.h"

#include <chrono>

namespace vms::broker {

const char* StreamKindName(StreamKind k) {
    switch (k) {
        case StreamKind::Main: return "main";
        case StreamKind::Sub:  return "sub";
    }
    return "?";
}

namespace {

std::int64_t steadyNowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Percent-encode a URL userinfo component: keep RFC 3986 unreserved characters,
// escape everything else (so an '@', ':', '/', or '%' in a password cannot break
// the authority). Deliberately conservative.
std::string encodeUserinfo(const std::string& s) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        const bool unreserved =
            (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

}  // namespace

ConnectionBroker::ConnectionBroker(Store& store, CredentialRepo& creds,
                                   BrokerConfig cfg, NowMsFn now)
    : store_(store), creds_(creds), cfg_(cfg), now_(std::move(now)) {
    if (!now_) now_ = &steadyNowMs;
}

Error ConnectionBroker::materializeUrl(const std::string& templateUrl,
                                       const std::string& secret,
                                       std::string& out) {
    out.clear();
    const std::size_t scheme = templateUrl.find("://");
    if (scheme == std::string::npos)
        return {Status::Misuse, "URL template has no scheme://"};
    const std::size_t authStart = scheme + 3;
    const std::size_t slash = templateUrl.find('/', authStart);
    const std::size_t authLen =
        (slash == std::string::npos) ? std::string::npos : slash - authStart;
    if (templateUrl.substr(authStart, authLen).find('@') != std::string::npos)
        return {Status::Misuse, "URL template already embeds credentials"};

    std::string user, pass;
    const std::size_t colon = secret.find(':');
    if (colon == std::string::npos) {
        pass = secret;                       // password-only credential
    } else {
        user = secret.substr(0, colon);
        pass = secret.substr(colon + 1);
    }
    const std::string userinfo =
        encodeUserinfo(user) + ":" + encodeUserinfo(pass) + "@";
    out = templateUrl.substr(0, authStart) + userinfo + templateUrl.substr(authStart);
    return Error::success();
}

Error ConnectionBroker::resolveCamera(const std::string& cameraId, StreamKind kind,
                                      std::string& deviceId, std::string& templateUrl) {
    deviceId.clear();
    templateUrl.clear();
    persist::Result r;
    if (Error e = store_.query(
            "SELECT device_id, main_url, sub_url FROM cameras WHERE id=?;",
            {cameraId}, r);
        !e)
        return e;
    if (r.rows.empty())
        return {Status::NotFound, "unknown camera " + cameraId};

    deviceId = std::get<std::string>(r.rows[0][0]);
    const persist::Value& urlv = (kind == StreamKind::Main) ? r.rows[0][1] : r.rows[0][2];
    if (std::holds_alternative<std::nullptr_t>(urlv) ||
        (std::holds_alternative<std::string>(urlv) && std::get<std::string>(urlv).empty()))
        return {Status::NotFound,
                "camera " + cameraId + " has no " + StreamKindName(kind) + " URL"};
    templateUrl = std::get<std::string>(urlv);
    return Error::success();
}

Error ConnectionBroker::deviceForCamera(const std::string& cameraId,
                                        std::string& deviceId) {
    deviceId.clear();
    persist::Result r;
    if (Error e = store_.query("SELECT device_id FROM cameras WHERE id=?;",
                               {cameraId}, r);
        !e)
        return e;
    if (r.rows.empty())
        return {Status::NotFound, "unknown camera " + cameraId};
    deviceId = std::get<std::string>(r.rows[0][0]);
    return Error::success();
}

Error ConnectionBroker::connect(const std::string& cameraId, StreamKind kind,
                                Session& out) {
    out = Session{};

    std::string deviceId, templateUrl;
    if (Error e = resolveCamera(cameraId, kind, deviceId, templateUrl); !e) return e;

    const std::int64_t now = now_();
    DeviceState& st = devices_[deviceId];

    // Circuit-breaker: while open, refuse honestly instead of hammering a device
    // that is likely locked out (P0-03).
    if (st.openUntilMs > now)
        return {Status::Unavailable,
                "device " + deviceId + " circuit open (backing off after repeated failures)"};

    // Pooling: never exceed the device's session capacity.
    if (st.active >= cfg_.maxSessionsPerDevice)
        return {Status::Unavailable,
                "device " + deviceId + " at max sessions (" +
                    std::to_string(cfg_.maxSessionsPerDevice) + ")"};

    // Resolve the secret through the credential repo; NotFound/Crypto propagate
    // as honest typed failures (never a plaintext fallback).
    std::string secret;
    if (Error e = creds_.getSecret(deviceId, secret); !e) return e;

    std::string url;
    Error m = materializeUrl(templateUrl, secret, url);
    secret.assign(secret.size(), '\0');   // zeroize the plaintext promptly
    if (!m) return m;

    st.active += 1;
    out.id = nextSessionId_++;
    out.cameraId = cameraId;
    out.deviceId = deviceId;
    out.kind = kind;
    out.url = url;
    out.valid = true;
    return Error::success();
}

void ConnectionBroker::release(const Session& s) {
    if (!s.valid) return;
    auto it = devices_.find(s.deviceId);
    if (it != devices_.end() && it->second.active > 0) it->second.active -= 1;
}

void ConnectionBroker::reportSuccess(const std::string& cameraId) {
    std::string deviceId;
    if (Error e = deviceForCamera(cameraId, deviceId); !e) return;
    DeviceState& st = devices_[deviceId];
    st.failures = 0;
    st.openUntilMs = 0;   // close the breaker
}

void ConnectionBroker::reportFailure(const std::string& cameraId) {
    std::string deviceId;
    if (Error e = deviceForCamera(cameraId, deviceId); !e) return;
    DeviceState& st = devices_[deviceId];
    st.failures += 1;
    if (st.failures >= cfg_.failureThreshold)
        st.openUntilMs = now_() + cfg_.openDurationMs;
}

int ConnectionBroker::activeSessions(const std::string& deviceId) const {
    auto it = devices_.find(deviceId);
    return it == devices_.end() ? 0 : it->second.active;
}

bool ConnectionBroker::circuitOpen(const std::string& deviceId) const {
    auto it = devices_.find(deviceId);
    if (it == devices_.end()) return false;
    return it->second.openUntilMs > now_();
}

} // namespace vms::broker
