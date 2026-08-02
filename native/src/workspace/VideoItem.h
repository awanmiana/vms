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
#include <QPointer>
#include <QQuickItem>
#include <QRect>
#include <QSize>

#include <atomic>
#include <cstdint>

class VideoItem : public QQuickItem {
    Q_OBJECT
    // A consumer may reuse frames submitted to another VideoItem. This is the
    // spatial-live-tile path: one GridPipeline owns decode/composite; room-level
    // tiles crop that already-produced frame instead of opening new sessions.
    Q_PROPERTY(VideoItem* frameSource READ frameSource WRITE setFrameSource
               NOTIFY frameSourceChanged)
    Q_PROPERTY(int sourceIndex READ sourceIndex WRITE setSourceIndex
               NOTIFY sourceCropChanged)
    Q_PROPERTY(int sourceColumns READ sourceColumns WRITE setSourceColumns
               NOTIFY sourceCropChanged)
    Q_PROPERTY(int sourceRows READ sourceRows WRITE setSourceRows
               NOTIFY sourceCropChanged)
    Q_PROPERTY(qulonglong frameCount READ submittedFrameCount)
public:
    explicit VideoItem(QQuickItem* parent = nullptr);

    // Thread-safe: called from any thread with a self-contained (deep-copied)
    // frame. Stores it and schedules a repaint on the GUI thread.
    void submitFrame(const QImage& frame);

    VideoItem* frameSource() const { return frameSource_.data(); }
    void setFrameSource(VideoItem* source);
    int sourceIndex() const { return sourceIndex_; }
    int sourceColumns() const { return sourceColumns_; }
    int sourceRows() const { return sourceRows_; }
    void setSourceIndex(int value);
    void setSourceColumns(int value);
    void setSourceRows(int value);

    std::uint64_t submittedFrameCount() const {
        return submittedFrames_.load(std::memory_order_relaxed);
    }

    // Integer partitioning keeps adjacent cells gap-free even when a composite
    // frame is not evenly divisible by its grid dimensions. Public so the
    // spatial self-test can pin the media-to-tile mapping without a scene graph.
    static QRect sourceRectForFrame(const QSize& frameSize, int sourceIndex,
                                    int sourceColumns, int sourceRows);

signals:
    void frameSourceChanged();
    void sourceCropChanged();
    void frameSubmitted(const QImage& frame);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    void receiveSourceFrame(const QImage& frame);
    void storeFrame(const QImage& frame);
    Q_INVOKABLE void scheduleUpdate();  // runs on the GUI thread

    QMutex mutex_;
    QImage pending_;
    bool dirty_ = false;
    QPointer<VideoItem> frameSource_;
    int sourceIndex_ = -1;
    int sourceColumns_ = 1;
    int sourceRows_ = 1;
    std::atomic<std::uint64_t> submittedFrames_{0};
};
