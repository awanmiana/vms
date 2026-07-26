#pragma once

// A Qt Quick scene-graph item that displays frames handed to it from a
// GStreamer appsink. This is the bridge P3-14 slice 2 needs: GStreamer decodes
// and (for the grid) composites video, an appsink pulls finished RGBA frames,
// and this item uploads each one to a scene-graph texture so QML can draw the
// honest per-tile state chrome *on top of live video*.
//
// It is pure Qt — it knows nothing about GStreamer — so the media layer stays
// swappable and the class stays unit-simple. Frames arrive on a GStreamer
// streaming thread via submitFrame(); the handoff to the render thread is
// mutex-guarded and an update is marshalled onto the GUI thread.

#include <QImage>
#include <QMutex>
#include <QQuickItem>

class VideoItem : public QQuickItem {
    Q_OBJECT
public:
    explicit VideoItem(QQuickItem* parent = nullptr);

    // Thread-safe: called from any thread with a self-contained (deep-copied)
    // frame. Stores it and schedules a repaint on the GUI thread.
    void submitFrame(const QImage& frame);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    Q_INVOKABLE void scheduleUpdate();  // runs on the GUI thread

    QMutex mutex_;
    QImage pending_;
    bool dirty_ = false;
};
