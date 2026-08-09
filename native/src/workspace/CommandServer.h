#pragma once

// P1-13 / A0 — the external control API (native increment 26). Scope:
// ../command-api-P1-13-proposal.md.
//
// Puts an HTTP/JSON transport in front of the increment-25 command envelope so
// an external AI agent (or any programmatic client) can drive the software
// with the SAME authority model as a human operator: every invocation flows
// through the same registry gate — validation, capability membership,
// dangerous-action confirmation, audit — the agent is a client of the
// envelope, never a privileged path.
//
// Security posture (first slice): loopback-only (binds 127.0.0.1, never a
// public interface), off by default (--api-port enables it), and a per-session
// bearer token required on every route. Multi-user auth, scoped tokens, TLS,
// streaming are later P1-13 slices. A bounded fixed-window request budget is
// the increment-42 hardening slice.

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <memory>

class CommandController;
class QHttpServer;
class QTcpServer;

class CommandServer : public QObject {
    Q_OBJECT

public:
    // Serves `commander`'s registry. `token` is the bearer secret every
    // request must present; `port` 0 picks an ephemeral port (self-test).
    CommandServer(CommandController* commander, QString token,
                  int requestLimit = 120, int windowMs = 60000,
                  QObject* parent = nullptr);
    ~CommandServer() override;

    // Bind 127.0.0.1:port and start serving. Returns false + fills `error`
    // when the port cannot be bound. Loopback is not configurable here —
    // remote exposure is a security-reviewed later slice.
    bool listen(quint16 port, QString& error);

    // The actually-bound port (after listen; useful with port 0).
    quint16 port() const;

    int requestLimit() const { return requestLimit_; }
    int windowMs() const { return windowMs_; }

private:
    bool admitRequest();
    int retryAfterMs() const;

    CommandController* commander_ = nullptr;
    QString token_;
    int requestLimit_ = 120;
    int windowMs_ = 60000;
    int requestsInWindow_ = 0;
    QElapsedTimer windowClock_;
    std::unique_ptr<QHttpServer> http_;
    std::unique_ptr<QTcpServer> tcp_;
};
