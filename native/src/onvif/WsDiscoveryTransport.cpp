#include "onvif/WsDiscoveryTransport.h"

#include "onvif/WsDiscovery.h"

#include <algorithm>

#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QUdpSocket>
#include <QUuid>

namespace vms::onvif {

std::vector<DiscoveredDevice> Discover(int timeoutMs, std::string& error) {
    error.clear();
    std::vector<DiscoveredDevice> out;
    if (timeoutMs < 1) timeoutMs = 1;

    QUdpSocket sock;
    // Bind to an ephemeral port on all IPv4 interfaces so unicast replies (ONVIF
    // devices answer directly to the source port) are received.
    if (!sock.bind(QHostAddress(QHostAddress::AnyIPv4), 0,
                   QUdpSocket::ShareAddress)) {
        error = "bind failed: " + sock.errorString().toStdString();
        return out;
    }

    const QString messageId =
        "urn:uuid:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QByteArray datagram =
        QByteArray::fromStdString(BuildProbe(messageId.toStdString()));
    const QHostAddress group(QStringLiteral("239.255.255.250"));
    if (sock.writeDatagram(datagram, group, 3702) < 0) {
        error = "send failed: " + sock.errorString().toStdString();
        return out;
    }

    std::vector<ProbeMatch> all;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (sock.waitForReadyRead(std::max(1, remaining))) {
            while (sock.hasPendingDatagrams()) {
                QByteArray buf;
                buf.resize(static_cast<int>(sock.pendingDatagramSize()));
                sock.readDatagram(buf.data(), buf.size());
                const auto ms = ParseProbeMatches(buf.toStdString());
                all.insert(all.end(), ms.begin(), ms.end());
            }
        }
    }

    for (const ProbeMatch& m : DedupByEndpoint(all)) {
        DiscoveredDevice d;
        d.endpointRef = m.endpointRef;
        d.name = ScopeValue(m, "name");
        d.hardware = ScopeValue(m, "hardware");
        d.xaddr = m.xaddrs.empty() ? std::string() : m.xaddrs.front();
        out.push_back(d);
    }
    return out;
}

} // namespace vms::onvif
