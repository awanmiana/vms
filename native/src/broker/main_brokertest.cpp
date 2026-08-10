// Connection broker self-check — native increment 6c. No Qt, no GStreamer, no
// display, no real socket. Exercises the pure ConnectionBroker against an
// in-memory SQLite store + in-memory secret store: connect-by-camera_id,
// tier->stream selection, in-memory credentialed-URL materialization (with
// userinfo percent-encoding), per-device connection pooling, the P0-03
// backoff/circuit-breaker (deterministic via an injected clock), and honest
// typed failures. Registered with CTest as `broker_selfcheck`.
// Scope: ../credential-broker-P1-03-proposal.md.

#include "broker/ConnectionBroker.h"
#include "media/BrokerGridLiveSource.h"
#include "persist/CredentialRepo.h"
#include "persist/Schema.h"
#include "persist/SecretStore.h"
#include "persist/Store.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

using namespace vms::persist;
using namespace vms::broker;
using namespace vms::media;

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}

// A secret store whose get() always fails with Crypto, to exercise the honest
// undecryptable-secret path through the broker.
class CryptoSecretStore : public InMemorySecretStore {
public:
    Error get(const std::string&, std::string&) override {
        return {Status::Crypto, "injected undecryptable secret"};
    }
};

}  // namespace

int main() {
    std::cout << "vms_brokertest — connection broker self-check\n";

    // ---- URL materialization (static, pure) ----
    {
        std::string url;
        check(static_cast<bool>(ConnectionBroker::materializeUrl(
                  "rtsp://192.168.0.254:554/Streaming/Channels/101",
                  "admin:p@ss/w0rd", url)) &&
                  url == "rtsp://admin:p%40ss%2Fw0rd@192.168.0.254:554/Streaming/Channels/101",
              "materialize: userinfo is injected and percent-encoded");
        check(ConnectionBroker::materializeUrl(
                  "rtsp://u:p@192.168.0.254:554/x", "admin:pw", url).status ==
                  Status::Misuse,
              "materialize: a template that already embeds credentials is Misuse");
        check(ConnectionBroker::materializeUrl("192.168.0.254/x", "a:b", url).status ==
                  Status::Misuse,
              "materialize: a template with no scheme:// is Misuse");
    }

    // ---- Set up a store with a provisioned device + cameras ----
    Store store;
    check(static_cast<bool>(store.open(":memory:")), "open in-memory store");
    check(static_cast<bool>(store.migrate(coreMigrations())), "migrate schema");
    check(static_cast<bool>(store.exec(
              "INSERT INTO devices(id, name) VALUES('dev-1','Cam Device 1');")),
          "insert device dev-1");
    check(static_cast<bool>(store.exec(
              "INSERT INTO devices(id, name) VALUES('dev-2','Cam Device 2');")),
          "insert device dev-2 (will have no secret)");
    check(static_cast<bool>(store.exec(
              "INSERT INTO cameras(id, device_id, name, main_url, sub_url) VALUES("
              "'cam-1','dev-1','Front Door',"
              "'rtsp://192.168.0.254:554/Streaming/Channels/101',"
              "'rtsp://192.168.0.254:554/Streaming/Channels/102');")),
          "insert cam-1 (main+sub URLs, credential-free)");
    check(static_cast<bool>(store.exec(
              "INSERT INTO cameras(id, device_id, name, main_url, sub_url) VALUES("
              "'cam-2','dev-1','Lobby',"
              "'rtsp://192.168.0.254:554/Streaming/Channels/201', NULL);")),
          "insert cam-2 (no sub URL)");
    check(static_cast<bool>(store.exec(
              "INSERT INTO cameras(id, device_id, name, main_url, sub_url) VALUES("
              "'cam-3','dev-2','Gate',"
              "'rtsp://192.168.0.254:554/Streaming/Channels/301', NULL);")),
          "insert cam-3 on the secret-less device");

    // Sanity: the stored URL templates carry no credentials.
    {
        Result r;
        store.query("SELECT main_url FROM cameras WHERE id='cam-1';", {}, r);
        check(!r.rows.empty() &&
                  std::get<std::string>(r.rows[0][0]).find('@') == std::string::npos,
              "stored URL template is credential-free (no '@')");
    }

    InMemorySecretStore secrets;
    CredentialRepo creds(store, secrets);
    check(static_cast<bool>(creds.put("dev-1", "admin:p@ss/w0rd")),
          "provision dev-1 credential (user:password)");

    // Deterministic clock for the circuit-breaker.
    std::int64_t clockMs = 100000;
    NowMsFn fakeClock = [&clockMs]() { return clockMs; };
    BrokerConfig cfg;
    cfg.maxSessionsPerDevice = 2;
    cfg.failureThreshold = 3;
    cfg.openDurationMs = 1000;
    ConnectionBroker broker(store, creds, cfg, fakeClock);

    // ---- connect + tier/stream selection ----
    {
        Session s;
        check(static_cast<bool>(broker.connect("cam-1", StreamKind::Main, s)) && s.valid &&
                  s.url == "rtsp://admin:p%40ss%2Fw0rd@192.168.0.254:554/Streaming/Channels/101",
              "connect(cam-1, Main) materializes the main-stream credentialed URL");
        broker.release(s);
        Session sub;
        check(static_cast<bool>(broker.connect("cam-1", StreamKind::Sub, sub)) && sub.valid &&
                  sub.url.find("/Streaming/Channels/102") != std::string::npos,
              "connect(cam-1, Sub) selects the sub-stream URL");
        broker.release(sub);
    }

    // ---- honest typed failures ----
    {
        Session s;
        check(broker.connect("nope", StreamKind::Main, s).status == Status::NotFound,
              "connect(unknown camera) is typed NotFound");
        check(broker.connect("cam-2", StreamKind::Sub, s).status == Status::NotFound,
              "connect to a stream with no URL is typed NotFound");
        check(broker.connect("cam-3", StreamKind::Main, s).status == Status::NotFound,
              "connect with no provisioned secret is typed NotFound");
        // Undecryptable secret -> Crypto (separate broker over a crypto store).
        CryptoSecretStore cryptoStore;
        CredentialRepo cryptoCreds(store, cryptoStore);
        cryptoCreds.put("dev-1", "whatever");   // writes ref; get() will fail Crypto
        ConnectionBroker cryptoBroker(store, cryptoCreds, cfg, fakeClock);
        check(cryptoBroker.connect("cam-1", StreamKind::Main, s).status == Status::Crypto,
              "connect with an undecryptable secret is typed Crypto (honest failure)");
        // restore the good secret for the remaining tests
        creds.put("dev-1", "admin:p@ss/w0rd");
    }

    // ---- production workspace live-source lease boundary ----
    {
        BrokerGridLiveSource live(broker, {"cam-1", "cam-2"});
        check(live.cameraCount() == 2,
              "live source preserves the explicit tile-to-camera order");

        GridStreamLease main;
        check(static_cast<bool>(live.acquire(0, vms::Tier::Main, main)) &&
                  main.valid && main.cameraId == "cam-1" &&
                  main.uri.find("/Streaming/Channels/101") != std::string::npos,
              "live source maps Main tier to the broker main stream");
        check(broker.activeSessions("dev-1") == 1,
              "live source lease holds exactly one broker pool slot");
        scrubLeaseUri(main);
        check(main.valid && main.uri.empty(),
              "credential-bearing lease URI can be scrubbed before release");
        live.release(main, StreamOutcome::Success);
        check(!main.valid && broker.activeSessions("dev-1") == 0,
              "successful live-source release clears lease and pool slot");

        GridStreamLease sub;
        check(static_cast<bool>(live.acquire(0, vms::Tier::Sub, sub)) &&
                  sub.uri.find("/Streaming/Channels/102") != std::string::npos,
              "live source maps Sub tier to the broker sub stream");
        live.release(sub, StreamOutcome::Unknown);

        GridStreamLease absent;
        check(live.acquire(1, vms::Tier::Thumb, absent).status ==
                  Status::NotFound && !absent.valid,
              "Thumb maps to Sub and never falls back to an absent Main stream");
        check(live.acquire(0, vms::Tier::Paused, absent).status ==
                  Status::Misuse && broker.activeSessions("dev-1") == 0,
              "Paused tier opens no broker session");
        check(live.acquire(2, vms::Tier::Main, absent).status ==
                  Status::NotFound,
              "tile without an assigned camera fails honestly");

        GridStreamLease failed;
        check(static_cast<bool>(live.acquire(0, vms::Tier::Main, failed)),
              "failure-feedback lease acquired");
        live.release(failed, StreamOutcome::Failure);
        check(broker.activeSessions("dev-1") == 0,
              "failed transport still releases its broker pool slot");
        GridStreamLease recovered;
        check(static_cast<bool>(live.acquire(0, vms::Tier::Main, recovered)),
              "one failed transport remains below the breaker threshold");
        live.release(recovered, StreamOutcome::Success);
    }

    // ---- pooling: never exceed maxSessionsPerDevice ----
    {
        Session a, b, c;
        check(static_cast<bool>(broker.connect("cam-1", StreamKind::Main, a)),
              "pool: 1st session granted");
        check(static_cast<bool>(broker.connect("cam-2", StreamKind::Main, b)),
              "pool: 2nd session granted (same device)");
        check(broker.activeSessions("dev-1") == 2, "pool: 2 active sessions on dev-1");
        check(broker.connect("cam-1", StreamKind::Main, c).status == Status::Unavailable &&
                  !broker.circuitOpen("dev-1"),
              "pool: 3rd session refused (Unavailable, breaker still closed)");
        broker.release(a);
        check(broker.activeSessions("dev-1") == 1, "pool: release frees a slot");
        check(static_cast<bool>(broker.connect("cam-1", StreamKind::Main, c)),
              "pool: session granted again after release");
        broker.release(b);
        broker.release(c);
        check(broker.activeSessions("dev-1") == 0, "pool: all sessions released");
    }

    // ---- circuit-breaker: back off after repeated failures, then recover ----
    {
        check(!broker.circuitOpen("dev-1"), "breaker: initially closed");
        broker.reportFailure("cam-1");
        broker.reportFailure("cam-1");
        check(!broker.circuitOpen("dev-1"), "breaker: still closed below threshold");
        broker.reportFailure("cam-1");
        check(broker.circuitOpen("dev-1"), "breaker: opens at the failure threshold");
        Session s;
        check(broker.connect("cam-1", StreamKind::Main, s).status == Status::Unavailable,
              "breaker: connect refused (Unavailable) while open");
        clockMs += cfg.openDurationMs + 1;   // let the cooldown elapse
        check(!broker.circuitOpen("dev-1"), "breaker: closes after the cooldown");
        check(static_cast<bool>(broker.connect("cam-1", StreamKind::Main, s)),
              "breaker: connect allowed again after cooldown (half-open)");
        broker.release(s);
        broker.reportSuccess("cam-1");        // a good connect resets the count
        broker.reportFailure("cam-1");
        broker.reportFailure("cam-1");
        check(!broker.circuitOpen("dev-1"),
              "breaker: reportSuccess reset the failure count (2 more != open)");
    }

    if (failures == 0) {
        std::cout << "PASS: broker resolution, materialization, pooling, backoff, "
                     "and honest typed failures all verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
