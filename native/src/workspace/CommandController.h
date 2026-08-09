#pragma once

// P1-12 / A0 — the command envelope's Qt bridge (native increment 25). Scope:
// ../command-envelope-P1-12-proposal.md.
//
// Registers the native workspace's operator verbs as validated commands over
// the live controllers and exposes the envelope to QML: the command palette
// types a line ("focus 5", "quality 3 thumb", "device.remove cam-1 confirm"),
// run() sends it through the ONE gate (validate -> capability -> dangerous
// confirm -> execute -> deterministic result + audit), and the session audit
// trail binds to the palette so every attempt — refused or executed — is
// visible. The same registry serves `--commands` (the machine-readable
// catalog, P1-13's discovery seed) and the headless `--command-selftest`.
//
// P0-01A: the session is Administrator, so ALL capabilities named by the
// registered specs are granted — the envelope still checks membership, so
// later role separation (P1-02/P1-03) changes the granted set, not the code.
// P1-06 durable audit attaches through setAuditStore(); the session trail and
// stdout remain available even when no persistent store is open.

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <set>

#include "command/CommandRegistry.h"
#include "command/VoiceCommandMapper.h"
#include "persist/AuditRepo.h"

class WorkspaceController;
class InstantReplayController;
class PlaybackController;
class DeviceController;
class AlarmController;
class PremisesController;

// The audit chain's hash (SHA-256 hex via Qt), shared by writer and verifier.
vms::persist::HashFn Sha256HexHash();

class CommandController : public QObject {
    Q_OBJECT
    // The session audit trail, newest first: { time, command, args, outcome,
    // message, ok }. Every attempt lands here, refusals included.
    Q_PROPERTY(QVariantList auditLog READ auditLog NOTIFY auditChanged)
    // One synopsis line per command ("workspace.focus <tile>") for the palette.
    Q_PROPERTY(QStringList commandHints READ commandHints CONSTANT)

public:
    // Controllers may be null (e.g. no recording DB / no device store); the
    // affected commands then refuse honestly instead of not existing silently.
    CommandController(WorkspaceController* live, InstantReplayController* instant,
                      DeviceController* devices, AlarmController* alarms = nullptr,
                      PlaybackController* playback = nullptr,
                      QObject* parent = nullptr);

    QVariantList auditLog() const { return auditLog_; }
    QStringList commandHints() const { return hints_; }

    // The text leg: parse + send one line through the envelope. Returns the
    // deterministic result line the palette shows ("ok · …" / "refused · …").
    Q_INVOKABLE QString run(const QString& line);

    // P1-12 voice leg: a recognizer-neutral, deterministic grammar maps a
    // language-tagged transcript to typed args, then invokes the same registry.
    // Voice can never satisfy a dangerous command's confirmation in-band.
    Q_INVOKABLE QString runVoice(const QString& transcript,
                                 const QString& languageTag = QStringLiteral("en-US"),
                                 double confidence = 1.0);

    // inc 29 (P1-13): the STRUCTURED leg every UI click uses. Typed arguments
    // (names with spaces, URLs, credentials) go through the same single gate
    // without the text leg's whitespace tokenization. Returns
    // { ok, outcome, message } so QML can show the honest result.
    Q_INVOKABLE QVariantMap invoke(const QString& id, const QVariantMap& args,
                                   bool confirm = false);

    // The machine-readable catalog (JSON) — printed by --commands and later
    // served to external agents (P1-13).
    Q_INVOKABLE QString catalogJson() const;

    // Direct envelope access for the self-test.
    vms::command::CommandRegistry& registry() { return registry_; }
    const std::set<std::string>& capabilities() const { return capabilities_; }

    // inc 28 (P1-06): attach the durable audit store — from then on every
    // attempt (refusals included) ALSO lands one hash-chained row in SQLite.
    // A persist failure is reported once to stderr, never silently dropped.
    void setAuditStore(vms::persist::AuditRepo* repo);
    void setPremisesController(PremisesController* premises) {
        premises_ = premises;
    }

    // Source attribution for the durable rows: run() stamps "palette", the
    // HTTP server stamps "api" around its invokes, everything else is "ui".
    void setSource(const QString& source) { source_ = source; }

signals:
    void auditChanged();
    // workspace.spatial is UI state owned by QML; the envelope requests it.
    void spatialModeRequested(bool on);

private:
    void registerCommands();

    vms::command::CommandRegistry registry_;
    vms::command::VoiceCommandMapper voiceMapper_;
    std::set<std::string> capabilities_;   // Administrator: all named caps
    QVariantList auditLog_;

    WorkspaceController* live_ = nullptr;
    InstantReplayController* instant_ = nullptr;
    PlaybackController* playback_ = nullptr;
    DeviceController* devices_ = nullptr;
    AlarmController* alarms_ = nullptr;
    PremisesController* premises_ = nullptr;
    QStringList hints_;
    vms::persist::AuditRepo* auditRepo_ = nullptr;   // inc 28: durable audit
    QString source_ = QStringLiteral("ui");
};
