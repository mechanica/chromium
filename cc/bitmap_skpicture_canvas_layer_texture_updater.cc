// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/bitmap_skpicture_canvas_layer_texture_updater.h"

#include "base/time.h"
#include "cc/layer_painter.h"
#include "cc/rendering_stats.h"
#include "cc/texture_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

BitmapSkPictureCanvasLayerTextureUpdater::Texture::Texture(BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater, scoped_ptr<PrioritizedTexture> texture)
    : CanvasLayerTextureUpdater::Texture(texture.Pass())
    , m_textureUpdater(textureUpdater)
{
}

void BitmapSkPictureCanvasLayerTextureUpdater::Texture::update(TextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats& stats)
{
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, sourceRect.width(), sourceRect.height());
    m_bitmap.allocPixels();
    m_bitmap.setIsOpaque(m_textureUpdater->layerIsOpaque());
    SkDevice device(m_bitmap);
    SkCanvas canvas(&device);
    base::TimeTicks paintBeginTime = base::TimeTicks::Now();
    textureUpdater()->paintContentsRect(&canvas, sourceRect, stats);
    stats.totalPaintTimeInSeconds += (base::TimeTicks::Now() - paintBeginTime).InSecondsF();

    ResourceUpdate upload = ResourceUpdate::Create(
        texture(), &m_bitmap, sourceRect, sourceRect, destOffset);
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

scoped_refptr<BitmapSkPictureCanvasLayerTextureUpdater> BitmapSkPictureCanvasLayerTextureUpdater::create(scoped_ptr<LayerPainter> painter)
{
    return make_scoped_refptr(new BitmapSkPictureCanvasLayerTextureUpdater(painter.Pass()));
}

BitmapSkPictureCanvasLayerTextureUpdater::BitmapSkPictureCanvasLayerTextureUpdater(scoped_ptr<LayerPainter> painter)
    : SkPictureCanvasLayerTextureUpdater(painter.Pass())
{
}

BitmapSkPictureCanvasLayerTextureUpdater::~BitmapSkPictureCanvasLayerTextureUpdater()
{
}

scoped_ptr<LayerTextureUpdater::Texture> BitmapSkPictureCanvasLayerTextureUpdater::createTexture(PrioritizedTextureManager* manager)
{
    return scoped_ptr<LayerTextureUpdater::Texture>(new Texture(this, PrioritizedTexture::create(manager)));
}

void BitmapSkPictureCanvasLayerTextureUpdater::paintContentsRect(SkCanvas* canvas, const IntRect& sourceRect, RenderingStats& stats)
{
    // Translate the origin of contentRect to that of sourceRect.
    canvas->translate(contentRect().x() - sourceRect.x(),
                      contentRect().y() - sourceRect.y());
    base::TimeTicks rasterizeBeginTime = base::TimeTicks::Now();
    drawPicture(canvas);
    stats.totalRasterizeTimeInSeconds += (base::TimeTicks::Now() - rasterizeBeginTime).InSecondsF();
}

} // namespace cc
