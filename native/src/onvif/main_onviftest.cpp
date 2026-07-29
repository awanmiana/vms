// ONVIF WS-Discovery self-check — native increment 11 (P2-03). No sockets. Proves
// the standards-exact Probe build and ProbeMatch parsing against real device
// response fixtures (including prefix variation), dedup by endpoint, scope
// decoding, and honest empty on malformed input. Registered with CTest as
// `onvif_selfcheck`. Needs Qt6::Core DLLs on PATH to run (QXmlStreamReader/QUrl).

#include "onvif/WsDiscovery.h"
#include "onvif/OnvifClient.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <QByteArray>
#include <QCryptographicHash>
#include <QString>
#include <QXmlStreamReader>

using namespace vms::onvif;

namespace {
int failures = 0;
void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}
bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
bool hasType(const ProbeMatch& m, const std::string& t) {
    return std::find(m.types.begin(), m.types.end(), t) != m.types.end();
}
bool wellFormed(const std::string& xml) {
    QXmlStreamReader r(QString::fromStdString(xml));
    while (!r.atEnd()) r.readNext();
    return !r.hasError();
}

// A real-shape Hikvision-style ProbeMatches (prefixes: env/wsa/d/dn).
const char* kHik = R"(<?xml version="1.0" encoding="UTF-8"?>
<env:Envelope xmlns:env="http://www.w3.org/2003/05/soap-envelope"
 xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing"
 xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"
 xmlns:dn="http://www.onvif.org/ver10/network/wsdl">
 <env:Header>
  <wsa:MessageID>urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee</wsa:MessageID>
  <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action>
 </env:Header>
 <env:Body>
  <d:ProbeMatches>
   <d:ProbeMatch>
    <wsa:EndpointReference>
     <wsa:Address>urn:uuid:2419d68a-2dd2-21b2-a205-0123456789ab</wsa:Address>
    </wsa:EndpointReference>
    <d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>
    <d:Scopes>onvif://www.onvif.org/type/video_encoder onvif://www.onvif.org/name/HIKVISION%20DS-2CD2042 onvif://www.onvif.org/hardware/DS-2CD2042WD-I onvif://www.onvif.org/location/city/hangzhou</d:Scopes>
    <d:XAddrs>http://192.168.0.254/onvif/device_service</d:XAddrs>
    <d:MetadataVersion>1</d:MetadataVersion>
   </d:ProbeMatch>
  </d:ProbeMatches>
 </env:Body>
</env:Envelope>)";

// A different device, DIFFERENT prefixes (default soap ns, wsdd:) to prove the
// parser is prefix-agnostic. Same content shape.
const char* kAxisDiffPrefix = R"(<?xml version="1.0"?>
<Envelope xmlns="http://www.w3.org/2003/05/soap-envelope"
 xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing"
 xmlns:wsdd="http://schemas.xmlsoap.org/ws/2005/04/discovery">
 <Body>
  <wsdd:ProbeMatches>
   <wsdd:ProbeMatch>
    <a:EndpointReference><a:Address>urn:uuid:11112222-3333-4444-5555-666677778888</a:Address></a:EndpointReference>
    <wsdd:Types>dn:NetworkVideoTransmitter</wsdd:Types>
    <wsdd:Scopes>onvif://www.onvif.org/name/AXIS%20M3045</wsdd:Scopes>
    <wsdd:XAddrs>http://10.0.0.20/onvif/device_service</wsdd:XAddrs>
   </wsdd:ProbeMatch>
  </wsdd:ProbeMatches>
 </Body>
</Envelope>)";

// Two ProbeMatch entries with the SAME endpoint (a device answering twice).
const char* kDup = R"(<d:ProbeMatches xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery">
 <d:ProbeMatch><wsa:EndpointReference><wsa:Address>urn:uuid:dup-1</wsa:Address></wsa:EndpointReference><d:XAddrs>http://1.1.1.1/onvif/device_service</d:XAddrs></d:ProbeMatch>
 <d:ProbeMatch><wsa:EndpointReference><wsa:Address>urn:uuid:dup-1</wsa:Address></wsa:EndpointReference><d:XAddrs>http://1.1.1.1/onvif/device_service</d:XAddrs></d:ProbeMatch>
</d:ProbeMatches>)";
}  // namespace

int main() {
    std::cout << "vms_onviftest — ONVIF WS-Discovery self-check\n";

    // ---- Probe build (standards-exact) ----
    const std::string probe = BuildProbe("urn:uuid:12345678-1234-1234-1234-123456789abc");
    check(wellFormed(probe), "Probe is well-formed XML");
    check(contains(probe, "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"),
          "Probe carries the WS-Discovery Probe Action");
    check(contains(probe, "urn:schemas-xmlsoap-org:ws:2005:04:discovery"),
          "Probe To is the WS-Discovery target");
    check(contains(probe, "dn:NetworkVideoTransmitter"),
          "Probe Types selects ONVIF network video transmitters");
    check(contains(probe, "urn:uuid:12345678-1234-1234-1234-123456789abc"),
          "Probe carries the caller's MessageID");

    // ---- Parse a real-shape ProbeMatches ----
    {
        const auto m = ParseProbeMatches(kHik);
        check(m.size() == 1, "Hikvision fixture: one ProbeMatch parsed");
        if (m.size() == 1) {
            check(m[0].endpointRef == "urn:uuid:2419d68a-2dd2-21b2-a205-0123456789ab",
                  "endpoint reference extracted");
            check(hasType(m[0], "dn:NetworkVideoTransmitter"), "Types extracted");
            check(!m[0].xaddrs.empty() &&
                      m[0].xaddrs[0] == "http://192.168.0.254/onvif/device_service",
                  "XAddrs (device-service URL) extracted");
            check(ScopeValue(m[0], "name") == "HIKVISION DS-2CD2042",
                  "scope 'name' decoded (percent-decoded)");
            check(ScopeValue(m[0], "hardware") == "DS-2CD2042WD-I", "scope 'hardware' decoded");
        }
    }

    // ---- Prefix-agnostic parse (different namespaces/prefixes) ----
    {
        const auto m = ParseProbeMatches(kAxisDiffPrefix);
        check(m.size() == 1 && m[0].xaddrs.size() == 1 &&
                  m[0].xaddrs[0] == "http://10.0.0.20/onvif/device_service" &&
                  ScopeValue(m[0], "name") == "AXIS M3045",
              "parses a device that uses different XML prefixes (prefix-agnostic)");
    }

    // ---- Dedup by endpoint ----
    {
        const auto raw = ParseProbeMatches(kDup);
        check(raw.size() == 2, "duplicate fixture parses both entries");
        check(DedupByEndpoint(raw).size() == 1, "DedupByEndpoint collapses same-EPR matches");
    }

    // ---- Honest on malformed input ----
    check(ParseProbeMatches("not xml <<<").empty(), "malformed input -> empty (no crash)");
    check(ParseProbeMatches("").empty(), "empty input -> empty");

    // ================= ONVIF device/media SOAP client (inc 12) =================
    // WS-Security PasswordDigest: verify the standard formula independently.
    {
        const std::string nonce = "LKqI9FA/hyj1qLRV+Rce7g==";
        const std::string created = "2010-01-01T00:00:00Z";
        const std::string pass = "password";
        // Independent recompute: Base64(SHA1( base64decode(nonce) + created + pass )).
        QByteArray material = QByteArray::fromBase64(QByteArray::fromStdString(nonce));
        material.append(QByteArray::fromStdString(created));
        material.append(QByteArray::fromStdString(pass));
        const std::string ref =
            QCryptographicHash::hash(material, QCryptographicHash::Sha1)
                .toBase64().toStdString();
        check(PasswordDigest(pass, nonce, created) == ref,
              "WSSE PasswordDigest matches Base64(SHA1(nonce+created+password))");
        check(PasswordDigest("other", nonce, created) != ref,
              "PasswordDigest changes with the password");

        const std::string hdr = BuildSecurityHeader("admin", pass, nonce, created);
        check(contains(hdr, "<wsse:Username>admin</wsse:Username>") &&
                  contains(hdr, ref) && contains(hdr, nonce) && contains(hdr, created) &&
                  contains(hdr, "#PasswordDigest"),
              "security header carries username, digest, nonce, created (PasswordDigest type)");
    }

    // Request envelopes are well-formed and carry the right operations.
    {
        const std::string prof = BuildGetProfilesRequest("u", "p", "AAAA", "2010-01-01T00:00:00Z");
        check(wellFormed(prof) && contains(prof, "GetProfiles") &&
                  contains(prof, "www.onvif.org/ver10/media/wsdl"),
              "GetProfiles request is well-formed ONVIF media SOAP");
        const std::string uri = BuildGetStreamUriRequest("Profile_1", "u", "p", "AAAA",
                                                          "2010-01-01T00:00:00Z");
        check(wellFormed(uri) && contains(uri, "GetStreamUri") &&
                  contains(uri, "<trt:ProfileToken>Profile_1</trt:ProfileToken>") &&
                  contains(uri, "RTSP"),
              "GetStreamUri request carries the profile token and RTSP transport");
    }

    // Parse a real-shape GetProfilesResponse.
    {
        const char* kProfiles = R"(<trt:GetProfilesResponse
 xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
 <trt:Profiles token="Profile_1"><tt:Name>mainStream</tt:Name>
  <tt:VideoEncoderConfiguration><tt:Name>venc1</tt:Name><tt:Encoding>H264</tt:Encoding>
   <tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution></tt:VideoEncoderConfiguration>
 </trt:Profiles>
 <trt:Profiles token="Profile_2"><tt:Name>subStream</tt:Name>
  <tt:VideoEncoderConfiguration><tt:Name>venc2</tt:Name><tt:Encoding>H264</tt:Encoding>
   <tt:Resolution><tt:Width>640</tt:Width><tt:Height>480</tt:Height></tt:Resolution></tt:VideoEncoderConfiguration>
 </trt:Profiles>
</trt:GetProfilesResponse>)";
        const auto p = ParseProfiles(kProfiles);
        check(p.size() == 2, "GetProfilesResponse: 2 profiles parsed");
        if (p.size() == 2) {
            check(p[0].token == "Profile_1" && p[0].name == "mainStream" &&
                      p[0].encoding == "H264" && p[0].width == 1920 && p[0].height == 1080,
                  "main profile token/name/encoding/resolution extracted");
            check(p[1].token == "Profile_2" && p[1].width == 640 && p[1].height == 480,
                  "sub profile resolution extracted");
        }
    }

    // Parse GetStreamUriResponse + GetDeviceInformationResponse.
    {
        const char* kUri = R"(<trt:GetStreamUriResponse
 xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
 <trt:MediaUri><tt:Uri>rtsp://192.168.0.254:554/Streaming/Channels/101</tt:Uri>
  <tt:InvalidAfterConnect>false</tt:InvalidAfterConnect></trt:MediaUri></trt:GetStreamUriResponse>)";
        check(ParseStreamUri(kUri) == "rtsp://192.168.0.254:554/Streaming/Channels/101",
              "GetStreamUriResponse: RTSP URI extracted");

        const char* kInfo = R"(<tds:GetDeviceInformationResponse
 xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
 <tds:Manufacturer>HIKVISION</tds:Manufacturer><tds:Model>DS-2CD2042</tds:Model>
 <tds:FirmwareVersion>V5.4.5</tds:FirmwareVersion><tds:SerialNumber>SN123</tds:SerialNumber>
 <tds:HardwareId>88</tds:HardwareId></tds:GetDeviceInformationResponse>)";
        const auto info = ParseDeviceInformation(kInfo);
        check(info.manufacturer == "HIKVISION" && info.model == "DS-2CD2042" &&
                  info.firmware == "V5.4.5" && info.serial == "SN123",
              "GetDeviceInformationResponse fields extracted");
    }

    if (failures == 0) {
        std::cout << "PASS: standards-exact Probe, ProbeMatch parsing across prefix "
                     "variation, dedup, scope decoding, and honest failure verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
