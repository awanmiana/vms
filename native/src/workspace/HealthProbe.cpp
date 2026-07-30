#include "HealthProbe.h"

#include <QString>
#include <QTcpSocket>

namespace vms::health {

namespace {

// A blocking TCP-connect reachability probe: the device answers on its media
// port -> reachable. Any refusal/timeout/unresolved-host -> not reachable. This
// is honest about reachability only (a completed TCP handshake is not a decoding
// stream); DeviceController leaves stream health Unknown.
class TcpHealthProbe : public DeviceHealthProbe {
public:
    explicit TcpHealthProbe(int timeoutMs) : timeoutMs_(timeoutMs) {}

    ProbeResult probe(const ProbeTarget& t) override {
        ProbeResult r;
        r.deviceId = t.deviceId;
        r.reachable = false;
        if (t.host.empty() || t.port <= 0 || t.port > 65535) return r;
        QTcpSocket sock;
        sock.connectToHost(QString::fromStdString(t.host),
                           static_cast<quint16>(t.port));
        r.reachable = sock.waitForConnected(timeoutMs_);
        sock.abort();
        return r;
    }

private:
    int timeoutMs_;
};

}  // namespace

std::unique_ptr<DeviceHealthProbe> MakeTcpHealthProbe(int timeoutMs) {
    return std::make_unique<TcpHealthProbe>(timeoutMs);
}

}  // namespace vms::health
