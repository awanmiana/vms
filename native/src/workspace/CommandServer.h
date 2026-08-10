#pragma once

// P1-13/A0 external control API. Loopback-only, opt-in, bounded, and served
// through capability-scoped bearer identities. Remote/TLS deployment remains
// fail-closed until a hostname and trust model are configured.

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <memory>
#include <set>
#include <string>
#include <vector>

class CommandController;
class QHttpServer;
class QTcpServer;

struct ApiIdentity {
    QString id;
    QString token;
    std::set<std::string> capabilities;
};

class CommandServer : public QObject {
    Q_OBJECT

public:
    // Backward-compatible administrator session identity.
    CommandServer(CommandController* commander, QString token,
                  int requestLimit = 120, int windowMs = 60000,
                  QObject* parent = nullptr);
    // Scoped identities. Invalid/duplicate/empty identities are omitted; the
    // server refuses to listen when none remain.
    CommandServer(CommandController* commander,
                  std::vector<ApiIdentity> identities,
                  int requestLimit = 120, int windowMs = 60000,
                  QObject* parent = nullptr);
    ~CommandServer() override;

    bool listen(quint16 port, QString& error);
    quint16 port() const;
    int requestLimit() const { return requestLimit_; }
    int windowMs() const { return windowMs_; }
    int identityCount() const { return static_cast<int>(identities_.size()); }

private:
    bool admitRequest();
    int retryAfterMs() const;
    const ApiIdentity* authenticate(const QString& bearer) const;

    CommandController* commander_ = nullptr;
    std::vector<ApiIdentity> identities_;
    int requestLimit_ = 120;
    int windowMs_ = 60000;
    int requestsInWindow_ = 0;
    QElapsedTimer windowClock_;
    std::unique_ptr<QHttpServer> http_;
    std::unique_ptr<QTcpServer> tcp_;
};
