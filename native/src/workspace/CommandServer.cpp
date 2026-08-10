#include "CommandServer.h"

#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <cmath>

#include "CommandController.h"

using vms::command::Args;
using vms::command::CommandResult;
using vms::command::Outcome;
using vms::command::OutcomeName;

namespace {

using StatusCode = QHttpServerResponder::StatusCode;

// Deterministic outcome -> HTTP status mapping (documented in the proposal).
StatusCode statusFor(Outcome o) {
    switch (o) {
        case Outcome::Ok:               return StatusCode::Ok;                 // 200
        case Outcome::UnknownCommand:   return StatusCode::NotFound;           // 404
        case Outcome::MissingParam:
        case Outcome::BadParam:         return StatusCode::BadRequest;         // 400
        case Outcome::CapabilityDenied: return StatusCode::Forbidden;          // 403
        case Outcome::NeedsConfirm:     return StatusCode::Conflict;           // 409
        case Outcome::Failed:           return StatusCode::InternalServerError;// 500
    }
    return StatusCode::InternalServerError;
}

QHttpServerResponse resultResponse(const CommandResult& r,
                                   const QString& identityId,
                                   const QString& correlationId) {
    QJsonObject body;
    body.insert(QStringLiteral("outcome"),
                QString::fromLatin1(OutcomeName(r.outcome)));
    body.insert(QStringLiteral("message"), QString::fromStdString(r.message));
    body.insert(QStringLiteral("identity"), identityId);
    body.insert(QStringLiteral("correlationId"), correlationId);
    return QHttpServerResponse(body, statusFor(r.outcome));
}

QHttpServerResponse errorResponse(StatusCode code, const QString& outcome,
                                  const QString& message) {
    QJsonObject body;
    body.insert(QStringLiteral("outcome"), outcome);
    body.insert(QStringLiteral("message"), message);
    return QHttpServerResponse(body, code);
}

QHttpServerResponse rateLimitResponse(int limit, int windowMs,
                                      int retryAfterMs) {
    QJsonObject body;
    body.insert(QStringLiteral("outcome"), QStringLiteral("rate-limited"));
    body.insert(QStringLiteral("message"),
                QStringLiteral("control API request budget exhausted"));
    body.insert(QStringLiteral("limit"), limit);
    body.insert(QStringLiteral("windowMs"), windowMs);
    body.insert(QStringLiteral("retryAfterMs"), retryAfterMs);
    return QHttpServerResponse(body, StatusCode::TooManyRequests);
}

// Constant shape: the bearer token, or empty when absent/malformed.
QString bearerOf(const QHttpServerRequest& req) {
    const QByteArray auth = req.value("Authorization");
    if (!auth.startsWith("Bearer ")) return QString();
    return QString::fromUtf8(auth.mid(7)).trimmed();
}

QString correlationOf(const QHttpServerRequest& req) {
    QString value = QString::fromUtf8(req.value("X-Correlation-ID")).trimmed();
    static const QRegularExpression safe(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
    if (!safe.match(value).hasMatch())
        value = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return value;
}

bool constantTimeEqual(const QByteArray& a, const QByteArray& b) {
    const int count = std::max(a.size(), b.size());
    unsigned int diff = static_cast<unsigned int>(a.size() ^ b.size());
    for (int i = 0; i < count; ++i) {
        const unsigned char ac = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char bc = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        diff |= static_cast<unsigned int>(ac ^ bc);
    }
    return diff == 0;
}

// JSON args -> the envelope's typed Args. Returns false + fills `why` on an
// arg the envelope could never validate (null/array/object values).
bool argsFromJson(const QJsonObject& obj, Args& out, QString& why) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QJsonValue v = it.value();
        const std::string key = it.key().toStdString();
        if (v.isBool()) {
            out[key] = v.toBool();
        } else if (v.isDouble()) {
            const double d = v.toDouble();
            if (d == std::floor(d) && std::abs(d) < 9.0e15)
                out[key] = static_cast<std::int64_t>(d);
            else
                out[key] = d;
        } else if (v.isString()) {
            out[key] = v.toString().toStdString();
        } else {
            why = QStringLiteral("argument '%1' must be a string, number, or "
                                 "boolean").arg(it.key());
            return false;
        }
    }
    return true;
}

} // namespace

CommandServer::CommandServer(CommandController* commander, QString token,
                             int requestLimit, int windowMs, QObject* parent)
    : CommandServer(
          commander,
          std::vector<ApiIdentity>{{QStringLiteral("session"), std::move(token),
                                    commander ? commander->capabilities()
                                              : std::set<std::string>{}}},
          requestLimit, windowMs, parent) {}

CommandServer::CommandServer(CommandController* commander,
                             std::vector<ApiIdentity> identities,
                             int requestLimit, int windowMs, QObject* parent)
    : QObject(parent), commander_(commander),
      requestLimit_(requestLimit > 0 ? requestLimit : 1),
      windowMs_(windowMs > 0 ? windowMs : 60000) {
    std::set<QString> ids;
    std::set<QString> tokens;
    for (ApiIdentity& identity : identities) {
        identity.id = identity.id.trimmed();
        identity.token = identity.token.trimmed();
        if (identity.id.isEmpty() || identity.token.isEmpty() ||
            !ids.insert(identity.id).second ||
            !tokens.insert(identity.token).second)
            continue;
        identities_.push_back(std::move(identity));
    }
    http_ = std::make_unique<QHttpServer>();
    tcp_ = std::make_unique<QTcpServer>();
    windowClock_.start();

    // GET /v1/commands — the machine-readable catalog (agent discovery).
    http_->route(QStringLiteral("/v1/commands"),
                 QHttpServerRequest::Method::Get,
                 [this](const QHttpServerRequest& req) {
        if (!admitRequest())
            return rateLimitResponse(requestLimit_, windowMs_, retryAfterMs());
        const ApiIdentity* identity = authenticate(bearerOf(req));
        if (!identity)
            return errorResponse(StatusCode::Unauthorized,
                                 QStringLiteral("unauthorized"),
                                 QStringLiteral("missing or invalid bearer token"));
        const QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray::fromStdString(
                commander_->registry().catalogJson(identity->capabilities)));
        return QHttpServerResponse(doc.array());
    });

    // POST /v1/invoke — one validated command through the SAME gate the
    // palette uses (validation, capability, dangerous confirm, audit).
    http_->route(QStringLiteral("/v1/invoke"),
                 QHttpServerRequest::Method::Post,
                 [this](const QHttpServerRequest& req) {
        if (!admitRequest())
            return rateLimitResponse(requestLimit_, windowMs_, retryAfterMs());
        const ApiIdentity* identity = authenticate(bearerOf(req));
        if (!identity)
            return errorResponse(StatusCode::Unauthorized,
                                 QStringLiteral("unauthorized"),
                                 QStringLiteral("missing or invalid bearer token"));

        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject())
            return errorResponse(StatusCode::BadRequest,
                                 QStringLiteral("bad-param"),
                                 QStringLiteral("body must be a JSON object"));
        const QJsonObject o = doc.object();
        const QString id = o.value(QStringLiteral("command")).toString();
        if (id.isEmpty())
            return errorResponse(StatusCode::BadRequest,
                                 QStringLiteral("bad-param"),
                                 QStringLiteral("'command' is required"));

        const QJsonValue argsValue = o.value(QStringLiteral("args"));
        if (!argsValue.isUndefined() && !argsValue.isObject())
            return errorResponse(StatusCode::BadRequest,
                                 QStringLiteral("bad-param"),
                                 QStringLiteral("'args' must be a JSON object"));
        const QJsonValue confirmValue = o.value(QStringLiteral("confirm"));
        if (!confirmValue.isUndefined() && !confirmValue.isBool())
            return errorResponse(StatusCode::BadRequest,
                                 QStringLiteral("bad-param"),
                                 QStringLiteral("'confirm' must be a boolean"));

        Args args;
        QString why;
        if (!argsFromJson(argsValue.toObject(), args, why))
            return errorResponse(StatusCode::BadRequest,
                                 QStringLiteral("bad-param"), why);
        const bool confirmed = confirmValue.toBool(false);
        const QString correlation = correlationOf(req);

        // Durable structured attribution: scoped identity + request correlation.
        commander_->setAuditContext(QStringLiteral("api"), identity->id,
                                    correlation);
        const CommandResult r = commander_->registry().invoke(
            id.toStdString(), args, identity->capabilities, confirmed);
        commander_->setAuditContext(QStringLiteral("ui"), QString(), QString());
        return resultResponse(r, identity->id, correlation);
    });

    // Anything else: an honest JSON 404 (not an empty body).
    http_->setMissingHandler(this, [this](const QHttpServerRequest& req,
                                         QHttpServerResponder& responder) {
        if (!admitRequest()) {
            responder.sendResponse(
                rateLimitResponse(requestLimit_, windowMs_, retryAfterMs()));
            return;
        }
        if (!authenticate(bearerOf(req))) {
            responder.sendResponse(errorResponse(
                StatusCode::Unauthorized, QStringLiteral("unauthorized"),
                QStringLiteral("missing or invalid bearer token")));
            return;
        }
        QJsonObject body;
        body.insert(QStringLiteral("outcome"), QStringLiteral("unknown-route"));
        body.insert(QStringLiteral("message"),
                    QStringLiteral("use GET /v1/commands or POST /v1/invoke"));
        responder.sendResponse(
            QHttpServerResponse(body, StatusCode::NotFound));
    });
}

CommandServer::~CommandServer() = default;

const ApiIdentity* CommandServer::authenticate(const QString& bearer) const {
    if (bearer.isEmpty()) return nullptr;
    const QByteArray supplied = bearer.toUtf8();
    const ApiIdentity* match = nullptr;
    for (const ApiIdentity& identity : identities_) {
        if (constantTimeEqual(supplied, identity.token.toUtf8())) match = &identity;
    }
    return match;
}

bool CommandServer::admitRequest() {
    if (!windowClock_.isValid() || windowClock_.elapsed() >= windowMs_) {
        windowClock_.restart();
        requestsInWindow_ = 0;
    }
    if (requestsInWindow_ >= requestLimit_) return false;
    ++requestsInWindow_;
    return true;
}

int CommandServer::retryAfterMs() const {
    if (!windowClock_.isValid()) return windowMs_;
    const qint64 remaining = windowMs_ - windowClock_.elapsed();
    return static_cast<int>(remaining > 0 ? remaining : 0);
}

bool CommandServer::listen(quint16 port, QString& error) {
    if (!commander_ || identities_.empty()) {
        error = QStringLiteral("at least one valid API identity is required");
        return false;
    }
    // Loopback ONLY — remote exposure is a security-reviewed later slice.
    if (!tcp_->listen(QHostAddress::LocalHost, port)) {
        error = tcp_->errorString();
        return false;
    }
    if (!http_->bind(tcp_.get())) {
        error = QStringLiteral("failed to bind the HTTP server");
        tcp_->close();
        return false;
    }
    return true;
}

quint16 CommandServer::port() const {
    return tcp_ ? tcp_->serverPort() : 0;
}
