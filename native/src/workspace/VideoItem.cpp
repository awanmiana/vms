#include "VideoItem.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

VideoItem::VideoItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void VideoItem::submitFrame(const QImage& frame) {
    storeFrame(frame);
    emit frameSubmitted(frame);
}

void VideoItem::receiveSourceFrame(const QImage& frame) {
    // Called on the source/GStreamer thread through a DirectConnection; the
    // same mutex + queued GUI update as submitFrame keeps the handoff safe.
    storeFrame(frame);
}

void VideoItem::storeFrame(const QImage& frame) {
    {
        QMutexLocker lock(&mutex_);
        // QImage is implicitly shared. The pipeline made one deep copy; each
        // consumer shares it until its render-thread crop is materialized.
        pending_ = frame;
        dirty_ = true;
    }
    submittedFrames_.fetch_add(1, std::memory_order_relaxed);
    // update() must run on the GUI thread; submitFrame is on a gst thread.
    QMetaObject::invokeMethod(this, "scheduleUpdate", Qt::QueuedConnection);
}

void VideoItem::setFrameSource(VideoItem* source) {
    if (source == this || frameSource_ == source) return;
    if (frameSource_) disconnect(frameSource_, nullptr, this, nullptr);
    frameSource_ = source;
    if (frameSource_) {
        connect(frameSource_, &VideoItem::frameSubmitted, this,
                &VideoItem::receiveSourceFrame, Qt::DirectConnection);
    }
    emit frameSourceChanged();
}

void VideoItem::setSourceIndex(int value) {
    if (sourceIndex_ == value) return;
    sourceIndex_ = value;
    emit sourceCropChanged();
    update();
}

void VideoItem::setSourceColumns(int value) {
    value = qMax(1, value);
    if (sourceColumns_ == value) return;
    sourceColumns_ = value;
    emit sourceCropChanged();
    update();
}

void VideoItem::setSourceRows(int value) {
    value = qMax(1, value);
    if (sourceRows_ == value) return;
    sourceRows_ = value;
    emit sourceCropChanged();
    update();
}

void VideoItem::scheduleUpdate() {
    update();
}

QRect VideoItem::sourceRectForFrame(const QSize& frameSize, int sourceIndex,
                                    int sourceColumns, int sourceRows) {
    const QRect full(QPoint(0, 0), frameSize);
    if (frameSize.isEmpty() || sourceColumns < 1 || sourceRows < 1 ||
        sourceIndex < 0 || sourceIndex >= sourceColumns * sourceRows)
        return full;

    const int col = sourceIndex % sourceColumns;
    const int row = sourceIndex / sourceColumns;
    const int left = frameSize.width() * col / sourceColumns;
    const int right = frameSize.width() * (col + 1) / sourceColumns;
    const int top = frameSize.height() * row / sourceRows;
    const int bottom = frameSize.height() * (row + 1) / sourceRows;
    return QRect(left, top, right - left, bottom - top);
}

QSGNode* VideoItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    QImage frame;
    {
        QMutexLocker lock(&mutex_);
        frame = pending_;
        dirty_ = false;
    }

    if (frame.isNull() || width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }

    const QRect sourceRect = sourceRectForFrame(
        frame.size(), sourceIndex_, sourceColumns_, sourceRows_);
    if (sourceRect != QRect(QPoint(0, 0), frame.size()))
        frame = frame.copy(sourceRect);

    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);
    }

    // Owning the texture makes setTexture() free the previous frame's texture.
    QSGTexture* tex = window()->createTextureFromImage(frame);
    node->setOwnsTexture(true);
    node->setTexture(tex);
    node->setRect(boundingRect());
    return node;
}
