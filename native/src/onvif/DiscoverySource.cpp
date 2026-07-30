#include "onvif/DiscoverySource.h"

#include <QString>
#include <QUrl>

#include "onvif/WsDiscoveryTransport.h"

namespace vms::onvif {

std::string HostFromXAddr(const std::string& xaddr) {
    if (xaddr.empty()) return {};
    const QUrl u = QUrl::fromUserInput(QString::fromStdString(xaddr));
    return u.isValid() ? u.host().toStdString() : std::string{};
}

#ifdef VMS_ONVIF_TRANSPORT
namespace {

// The live source: WS-Discovery multicast for candidates, the ONVIF media SOAP
// client (over HTTP) for one device's profiles + stream URIs. Real network I/O;
// verified on hardware against the test camera.
class LiveDiscoverySource : public DiscoverySource {
public:
    std::vector<DiscoveryCandidate> discover(int timeoutMs,
                                             std::string& error) override {
        std::vector<DiscoveryCandidate> out;
        for (const DiscoveredDevice& d : Discover(timeoutMs, error)) {
            DiscoveryCandidate c;
            c.endpointRef = d.endpointRef;
            c.name = d.name;
            c.hardware = d.hardware;
            c.xaddr = d.xaddr;
            c.host = HostFromXAddr(d.xaddr);
            out.push_back(c);
        }
        return out;
    }

    bool fetch(const std::string& serviceUrl, const std::string& user,
               const std::string& password, OnvifDevice& out,
               std::string& error) override {
        return FetchDevice(serviceUrl, user, password, out, error);
    }
};

}  // namespace

std::unique_ptr<DiscoverySource> MakeLiveDiscoverySource() {
    return std::make_unique<LiveDiscoverySource>();
}
#else
std::unique_ptr<DiscoverySource> MakeLiveDiscoverySource() {
    // Transports were not compiled in (no Qt6::Network): discovery is honestly
    // unavailable rather than a fake pretending to scan.
    return nullptr;
}
#endif  // VMS_ONVIF_TRANSPORT

}  // namespace vms::onvif
