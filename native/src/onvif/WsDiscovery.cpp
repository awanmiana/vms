#include "onvif/WsDiscovery.h"

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamReader>

namespace vms::onvif {

std::string BuildProbe(const std::string& messageId) {
    // WS-Discovery 2005/04 namespaces, exactly as ONVIF requires. mustUnderstand
    // on To/Action per the spec. The Probe Types selects ONVIF transmitters.
    std::string s;
    s += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    s += "<soap:Envelope "
         "xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" "
         "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
         "xmlns:wsdd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
         "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">";
    s += "<soap:Header>";
    s += "<wsa:MessageID>" + messageId + "</wsa:MessageID>";
    s += "<wsa:To soap:mustUnderstand=\"1\">"
         "urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>";
    s += "<wsa:Action soap:mustUnderstand=\"1\">"
         "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>";
    s += "</soap:Header>";
    s += "<soap:Body>";
    s += "<wsdd:Probe><wsdd:Types>dn:NetworkVideoTransmitter</wsdd:Types></wsdd:Probe>";
    s += "</soap:Body></soap:Envelope>";
    return s;
}

namespace {
std::vector<std::string> splitWhitespace(const QString& text) {
    std::vector<std::string> out;
    const QStringList parts =
        text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString& p : parts) out.push_back(p.toStdString());
    return out;
}
}  // namespace

std::vector<ProbeMatch> ParseProbeMatches(const std::string& xml) {
    std::vector<ProbeMatch> matches;
    QXmlStreamReader r(QString::fromStdString(xml));

    ProbeMatch cur;
    bool inMatch = false;
    bool inEndpointRef = false;

    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType t = r.readNext();
        if (t == QXmlStreamReader::StartElement) {
            const QString name = r.name().toString();   // local name (prefix-agnostic)
            if (name == "ProbeMatch") {
                cur = ProbeMatch{};
                inMatch = true;
            } else if (!inMatch) {
                continue;
            } else if (name == "EndpointReference") {
                inEndpointRef = true;
            } else if (name == "Address" && inEndpointRef) {
                cur.endpointRef = r.readElementText().trimmed().toStdString();
            } else if (name == "Types") {
                cur.types = splitWhitespace(r.readElementText());
            } else if (name == "Scopes") {
                cur.scopes = splitWhitespace(r.readElementText());
            } else if (name == "XAddrs") {
                cur.xaddrs = splitWhitespace(r.readElementText());
            }
        } else if (t == QXmlStreamReader::EndElement) {
            const QString name = r.name().toString();
            if (name == "EndpointReference") {
                inEndpointRef = false;
            } else if (name == "ProbeMatch") {
                if (!cur.endpointRef.empty() || !cur.xaddrs.empty())
                    matches.push_back(cur);
                inMatch = false;
            }
        }
    }
    if (r.hasError()) return {};   // malformed datagram -> honest empty
    return matches;
}

std::vector<ProbeMatch> DedupByEndpoint(const std::vector<ProbeMatch>& matches) {
    std::vector<ProbeMatch> out;
    for (const ProbeMatch& m : matches) {
        bool seen = false;
        for (const ProbeMatch& kept : out)
            if (kept.endpointRef == m.endpointRef && !m.endpointRef.empty()) {
                seen = true;
                break;
            }
        if (!seen) out.push_back(m);
    }
    return out;
}

std::string ScopeValue(const ProbeMatch& match, const std::string& key) {
    const std::string prefix = "onvif://www.onvif.org/" + key + "/";
    for (const std::string& scope : match.scopes) {
        if (scope.rfind(prefix, 0) == 0) {
            const QString encoded = QString::fromStdString(scope.substr(prefix.size()));
            return QUrl::fromPercentEncoding(encoded.toUtf8()).toStdString();
        }
    }
    return {};
}

} // namespace vms::onvif
