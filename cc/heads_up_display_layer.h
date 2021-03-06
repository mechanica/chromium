// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeadsUpDisplayLayerChromium_h
#define HeadsUpDisplayLayerChromium_h

#include "CCFontAtlas.h"
#include "IntSize.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layer.h"

namespace cc {

class HeadsUpDisplayLayer : public Layer {
public:
    static scoped_refptr<HeadsUpDisplayLayer> create();

    virtual void update(TextureUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;

    void setFontAtlas(scoped_ptr<FontAtlas>);

    virtual scoped_ptr<LayerImpl> createLayerImpl() OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

protected:
    HeadsUpDisplayLayer();

private:
    virtual ~HeadsUpDisplayLayer();

    scoped_ptr<FontAtlas> m_fontAtlas;
};

}  // namespace cc

#endif
