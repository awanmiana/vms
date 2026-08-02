#include "PremisesController.h"

PremisesController::PremisesController(vms::persist::PremisesRepo* repo,
                                       QString workspace, QObject* parent)
    : QObject(parent), repo_(repo), workspace_(std::move(workspace)) {
    if (repo_) {
        repo_->ensureDefault(workspace_.toStdString());
        reload();
    }
}

QString PremisesController::reload() {
    if (!repo_) return QStringLiteral("no premises store attached");
    vms::persist::PremisesState state;
    bool found = false;
    const vms::persist::Error e =
        repo_->loadActive(workspace_.toStdString(), state, found);
    if (!e) return QString::fromStdString(e.message);
    if (!found) return QStringLiteral("no active premises");
    siteId_ = QString::fromStdString(state.siteId);
    siteName_ = QString::fromStdString(state.siteName);
    timezone_ = QString::fromStdString(state.timezone);
    floorId_ = QString::fromStdString(state.floorId);
    floorName_ = QString::fromStdString(state.floorName);
    planUri_ = QString::fromStdString(state.planUri);
    worldWidth_ = state.worldWidth;
    worldHeight_ = state.worldHeight;
    emit changed();
    return {};
}

QString PremisesController::configureSite(const QString& id,
                                          const QString& name,
                                          const QString& timezone) {
    if (!repo_) return QStringLiteral("no premises store attached");
    const vms::persist::Error e = repo_->configureSite(
        workspace_.toStdString(), id.toStdString(), name.toStdString(),
        timezone.toStdString());
    if (!e) return QString::fromStdString(e.message);
    return reload();
}

QString PremisesController::configureFloor(const QString& id,
                                           const QString& siteId,
                                           const QString& name,
                                           const QString& planUri,
                                           double width, double height) {
    if (!repo_) return QStringLiteral("no premises store attached");
    const vms::persist::Error e = repo_->configureFloor(
        workspace_.toStdString(), id.toStdString(), siteId.toStdString(),
        name.toStdString(), planUri.toStdString(), width, height);
    if (!e) return QString::fromStdString(e.message);
    return reload();
}
