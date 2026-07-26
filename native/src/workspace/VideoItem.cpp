#include "VideoItem.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

VideoItem::VideoItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void VideoItem::submitFrame(const QImage& frame) {
    {
        QMutexLocker lock(&mutex_);
        pending_ = frame;   // already a deep copy from the caller
        dirty_ = true;
    }
    // update() must run on the GUI thread; submitFrame is on a gst thread.
    QMetaObject::invokeMethod(this, "scheduleUpdate", Qt::QueuedConnection);
}

void VideoItem::scheduleUpdate() {
    update();
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
