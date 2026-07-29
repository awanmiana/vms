#include "onvif/OnvifClient.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QString>
#include <QXmlStreamReader>

namespace vms::onvif {

namespace {
const char* kSoapOpen =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
    "xmlns:tt=\"http://www.onvif.org/ver10/schema\">";
}  // namespace

std::string PasswordDigest(const std::string& password,
                           const std::string& nonceBase64,
                           const std::string& createdUtc) {
    const QByteArray nonce =
        QByteArray::fromBase64(QByteArray::fromStdString(nonceBase64));
    QByteArray material = nonce;
    material.append(QByteArray::fromStdString(createdUtc));
    material.append(QByteArray::fromStdString(password));
    const QByteArray sha =
        QCryptographicHash::hash(material, QCryptographicHash::Sha1);
    return sha.toBase64().toStdString();
}

std::string BuildSecurityHeader(const std::string& user, const std::string& password,
                                const std::string& nonceBase64,
                                const std::string& createdUtc) {
    const std::string digest = PasswordDigest(password, nonceBase64, createdUtc);
    std::string h;
    h += "<s:Header>";
    h += "<wsse:Security s:mustUnderstand=\"1\" "
         "xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/"
         "oasis-200401-wss-wssecurity-secext-1.0.xsd\" "
         "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/"
         "oasis-200401-wss-wssecurity-utility-1.0.xsd\">";
    h += "<wsse:UsernameToken>";
    h += "<wsse:Username>" + user + "</wsse:Username>";
    h += "<wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/"
         "oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">" +
         digest + "</wsse:Password>";
    h += "<wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/"
         "oasis-200401-wss-soap-message-security-1.0#Base64Binary\">" +
         nonceBase64 + "</wsse:Nonce>";
    h += "<wsu:Created>" + createdUtc + "</wsu:Created>";
    h += "</wsse:UsernameToken></wsse:Security></s:Header>";
    return h;
}

std::string BuildGetDeviceInformationRequest(const std::string& user,
                                             const std::string& password,
                                             const std::string& nonceBase64,
                                             const std::string& createdUtc) {
    return std::string(kSoapOpen) +
           BuildSecurityHeader(user, password, nonceBase64, createdUtc) +
           "<s:Body><tds:GetDeviceInformation/></s:Body></s:Envelope>";
}

std::string BuildGetProfilesRequest(const std::string& user, const std::string& password,
                                    const std::string& nonceBase64,
                                    const std::string& createdUtc) {
    return std::string(kSoapOpen) +
           BuildSecurityHeader(user, password, nonceBase64, createdUtc) +
           "<s:Body><trt:GetProfiles/></s:Body></s:Envelope>";
}

std::string BuildGetStreamUriRequest(const std::string& profileToken,
                                     const std::string& user, const std::string& password,
                                     const std::string& nonceBase64,
                                     const std::string& createdUtc) {
    return std::string(kSoapOpen) +
           BuildSecurityHeader(user, password, nonceBase64, createdUtc) +
           "<s:Body><trt:GetStreamUri>"
           "<trt:StreamSetup><tt:Stream>RTP-Unicast</tt:Stream>"
           "<tt:Transport><tt:Protocol>RTSP</tt:Protocol></tt:Transport>"
           "</trt:StreamSetup>"
           "<trt:ProfileToken>" + profileToken + "</trt:ProfileToken>"
           "</trt:GetStreamUri></s:Body></s:Envelope>";
}

std::vector<MediaProfile> ParseProfiles(const std::string& xml) {
    std::vector<MediaProfile> out;
    QXmlStreamReader r(QString::fromStdString(xml));
    bool inProfile = false;
    MediaProfile cur;
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType t = r.readNext();
        if (t == QXmlStreamReader::StartElement) {
            const QString name = r.name().toString();
            if (name == "Profiles") {
                cur = MediaProfile{};
                const auto attrs = r.attributes();
                cur.token = attrs.value("token").toString().toStdString();
                inProfile = true;
            } else if (!inProfile) {
                continue;
            } else if (name == "Name" && cur.name.empty()) {
                cur.name = r.readElementText().trimmed().toStdString();
            } else if (name == "Encoding" && cur.encoding.empty()) {
                cur.encoding = r.readElementText().trimmed().toStdString();
            } else if (name == "Width" && cur.width == 0) {
                cur.width = r.readElementText().toInt();
            } else if (name == "Height" && cur.height == 0) {
                cur.height = r.readElementText().toInt();
            }
        } else if (t == QXmlStreamReader::EndElement) {
            if (r.name().toString() == "Profiles") {
                if (!cur.token.empty()) out.push_back(cur);
                inProfile = false;
            }
        }
    }
    if (r.hasError()) return {};
    return out;
}

std::string ParseStreamUri(const std::string& xml) {
    QXmlStreamReader r(QString::fromStdString(xml));
    while (!r.atEnd()) {
        if (r.readNext() == QXmlStreamReader::StartElement &&
            r.name().toString() == "Uri")
            return r.readElementText().trimmed().toStdString();
    }
    return {};
}

DeviceInformation ParseDeviceInformation(const std::string& xml) {
    DeviceInformation d;
    QXmlStreamReader r(QString::fromStdString(xml));
    while (!r.atEnd()) {
        if (r.readNext() != QXmlStreamReader::StartElement) continue;
        const QString name = r.name().toString();
        if (name == "Manufacturer")        d.manufacturer = r.readElementText().toStdString();
        else if (name == "Model")          d.model = r.readElementText().toStdString();
        else if (name == "FirmwareVersion") d.firmware = r.readElementText().toStdString();
        else if (name == "SerialNumber")   d.serial = r.readElementText().toStdString();
        else if (name == "HardwareId")     d.hardwareId = r.readElementText().toStdString();
    }
    return d;
}

} // namespace vms::onvif
