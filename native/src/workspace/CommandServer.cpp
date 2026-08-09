#include "CommandServer.h"

#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>

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

QHttpServerResponse resultResponse(const CommandResult& r) {
    QJsonObject body;
    body.insert(QStringLiteral("outcome"),
                QString::fromLatin1(OutcomeName(r.outcome)));
    body.insert(QStringLiteral("message"), QString::fromStdString(r.message));
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
    : QObject(parent), commander_(commander), token_(std::move(token)),
      requestLimit_(requestLimit > 0 ? requestLimit : 1),
      windowMs_(windowMs > 0 ? windowMs : 60000) {
    http_ = std::make_unique<QHttpServer>();
    tcp_ = std::make_unique<QTcpServer>();
    windowClock_.start();

    // GET /v1/commands — the machine-readable catalog (agent discovery).
    http_->route(QStringLiteral("/v1/commands"),
                 QHttpServerRequest::Method::Get,
                 [this](const QHttpServerRequest& req) {
        if (!admitRequest())
            return rateLimitResponse(requestLimit_, windowMs_, retryAfterMs());
        if (bearerOf(req) != token_)
            return errorResponse(StatusCode::Unauthorized,
                                 QStringLiteral("unauthorized"),
                                 QStringLiteral("missing or invalid bearer token"));
        const QJsonDocument doc = QJsonDocument::fromJson(
            commander_->catalogJson().toUtf8());
        return QHttpServerResponse(doc.array());
    });

    // POST /v1/invoke — one validated command through the SAME gate the
    // palette uses (validation, capability, dangerous confirm, audit).
    http_->route(QStringLiteral("/v1/invoke"),
                 QHttpServerRequest::Method::Post,
                 [this](const QHttpServerRequest& req) {
        if (!admitRequest())
            return rateLimitResponse(requestLimit_, windowMs_, retryAfterMs());
        if (bearerOf(req) != token_)
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

        Args args;
        QString why;
        if (!argsFromJson(o.value(QStringLiteral("args")).toObject(), args, why))
            return errorResponse(StatusCode::BadRequest,
                                 QStringLiteral("bad-param"), why);
        const bool confirmed = o.value(QStringLiteral("confirm")).toBool(false);

        // Durable-audit source attribution (inc 28): this attempt came over HTTP.
        commander_->setSource(QStringLiteral("api"));
        const CommandResult r = commander_->registry().invoke(
            id.toStdString(), args, commander_->capabilities(), confirmed);
        commander_->setSource(QStringLiteral("ui"));
        return resultResponse(r);
    });

    // Anything else: an honest JSON 404 (not an empty body).
    http_->setMissingHandler(this, [this](const QHttpServerRequest&,
                                         QHttpServerResponder& responder) {
        if (!admitRequest()) {
            responder.sendResponse(
                rateLimitResponse(requestLimit_, windowMs_, retryAfterMs()));
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
