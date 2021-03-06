// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/bitmap_canvas_layer_texture_updater.h"

#include "cc/layer_painter.h"
#include "cc/resource_update.h"
#include "cc/texture_update_queue.h"
#include "skia/ext/platform_canvas.h"

namespace cc {

BitmapCanvasLayerTextureUpdater::Texture::Texture(BitmapCanvasLayerTextureUpdater* textureUpdater, scoped_ptr<PrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture.Pass())
    , m_textureUpdater(textureUpdater)
{
}

BitmapCanvasLayerTextureUpdater::Texture::~Texture()
{
}

void BitmapCanvasLayerTextureUpdater::Texture::update(TextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&)
{
    textureUpdater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
}

scoped_refptr<BitmapCanvasLayerTextureUpdater> BitmapCanvasLayerTextureUpdater::create(scoped_ptr<LayerPainter> painter)
{
    return make_scoped_refptr(new BitmapCanvasLayerTextureUpdater(painter.Pass()));
}

BitmapCanvasLayerTextureUpdater::BitmapCanvasLayerTextureUpdater(scoped_ptr<LayerPainter> painter)
    : CanvasLayerTextureUpdater(painter.Pass())
    , m_opaque(false)
{
}

BitmapCanvasLayerTextureUpdater::~BitmapCanvasLayerTextureUpdater()
{
}

scoped_ptr<LayerTextureUpdater::Texture> BitmapCanvasLayerTextureUpdater::createTexture(PrioritizedTextureManager* manager)
{
    return scoped_ptr<LayerTextureUpdater::Texture>(new Texture(this, PrioritizedTexture::create(manager)));
}

void BitmapCanvasLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats& stats)
{
    if (m_canvasSize != contentRect.size()) {
        m_canvasSize = contentRect.size();
        m_canvas = make_scoped_ptr(skia::CreateBitmapCanvas(m_canvasSize.width(), m_canvasSize.height(), m_opaque));
    }

    paintContents(m_canvas.get(), contentRect, contentsWidthScale, contentsHeightScale, resultingOpaqueRect, stats);
}

void BitmapCanvasLayerTextureUpdater::updateTexture(TextureUpdateQueue& queue, PrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate)
{
    ResourceUpdate upload = ResourceUpdate::Create(
        texture,
        &m_canvas->getDevice()->accessBitmap(false),
        contentRect(),
        sourceRect,
        destOffset);
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

void BitmapCanvasLayerTextureUpdater::setOpaque(bool opaque)
{
    if (opaque != m_opaque) {
        m_canvas.reset();
        m_canvasSize = IntSize();
    }
    m_opaque = opaque;
}

} // namespace cc
