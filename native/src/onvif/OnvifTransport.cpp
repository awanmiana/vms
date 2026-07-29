#include "onvif/OnvifTransport.h"

#include <QByteArray>
#include <QDateTime>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrl>

namespace vms::onvif {

namespace {
// A fresh WS-Security nonce (base64 of 16 random bytes) and an ISO-8601 UTC
// timestamp, per WSSE UsernameToken.
std::string freshNonce() {
    QByteArray raw(16, 0);
    for (int i = 0; i < raw.size(); ++i)
        raw[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return raw.toBase64().toStdString();
}
std::string nowUtc() {
    return QDateTime::currentDateTimeUtc()
        .toString(Qt::ISODate)
        .toStdString();
}
}  // namespace

bool OnvifCall(const std::string& serviceUrl, const std::string& soap,
               std::string& responseXml, std::string& error, int timeoutMs) {
    responseXml.clear();
    error.clear();

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(QString::fromStdString(serviceUrl))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QByteArrayLiteral("application/soap+xml; charset=utf-8"));

    QEventLoop loop;
    QNetworkReply* reply = nam.post(req, QByteArray::fromStdString(soap));
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (reply->isRunning()) {
        reply->abort();
        error = "ONVIF request timed out";
        reply->deleteLater();
        return false;
    }
    const QNetworkReply::NetworkError ne = reply->error();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    // ONVIF returns 500 with a SOAP Fault on auth errors; surface the body either
    // way so the caller can see the fault reason, but treat transport errors with
    // no body as failures.
    if (ne != QNetworkReply::NoError && body.isEmpty()) {
        error = "ONVIF transport error";
        return false;
    }
    responseXml = body.toStdString();
    return true;
}

bool FetchDevice(const std::string& serviceUrl, const std::string& user,
                 const std::string& password, OnvifDevice& out, std::string& error) {
    out = OnvifDevice{};

    auto call = [&](const std::string& soap, std::string& resp) -> bool {
        return OnvifCall(serviceUrl, soap, resp, error);
    };

    std::string resp;
    if (!call(BuildGetDeviceInformationRequest(user, password, freshNonce(), nowUtc()), resp))
        return false;
    out.info = ParseDeviceInformation(resp);

    if (!call(BuildGetProfilesRequest(user, password, freshNonce(), nowUtc()), resp))
        return false;
    out.profiles = ParseProfiles(resp);
    if (out.profiles.empty()) {
        error = "no media profiles returned (check credentials / media service URL)";
        return false;
    }

    for (const MediaProfile& p : out.profiles) {
        std::string uriResp;
        if (!call(BuildGetStreamUriRequest(p.token, user, password, freshNonce(), nowUtc()),
                  uriResp))
            return false;
        out.streamUris.push_back(ParseStreamUri(uriResp));
    }
    return true;
}

} // namespace vms::onvif
