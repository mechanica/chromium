// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/layer_impl.h"

#include "cc/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>

using namespace cc;
using namespace WebKit;

namespace {

#define EXECUTE_AND_VERIFY_SUBTREE_CHANGED(codeToTest)                  \
    root->resetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_TRUE(root->layerPropertyChanged());                          \
    EXPECT_TRUE(child->layerPropertyChanged());                         \
    EXPECT_TRUE(grandChild->layerPropertyChanged());                    \
    EXPECT_FALSE(root->layerSurfacePropertyChanged())


#define EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(codeToTest)           \
    root->resetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_FALSE(root->layerPropertyChanged());                         \
    EXPECT_FALSE(child->layerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->layerPropertyChanged());                   \
    EXPECT_FALSE(root->layerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(codeToTest)               \
    root->resetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_TRUE(root->layerPropertyChanged());                          \
    EXPECT_FALSE(child->layerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->layerPropertyChanged());                   \
    EXPECT_FALSE(root->layerSurfacePropertyChanged())

#define EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(codeToTest)             \
    root->resetAllChangeTrackingForSubtree();                           \
    codeToTest;                                                         \
    EXPECT_FALSE(root->layerPropertyChanged());                         \
    EXPECT_FALSE(child->layerPropertyChanged());                        \
    EXPECT_FALSE(grandChild->layerPropertyChanged());                   \
    EXPECT_TRUE(root->layerSurfacePropertyChanged())

TEST(LayerImplTest, verifyLayerChangesAreTrackedProperly)
{
    //
    // This test checks that layerPropertyChanged() has the correct behavior.
    //

    // The constructor on this will fake that we are on the correct thread.
    DebugScopedSetImplThread setImplThread;

    // Create a simple LayerImpl tree:
    scoped_ptr<LayerImpl> root = LayerImpl::create(1);
    root->addChild(LayerImpl::create(2));
    LayerImpl* child = root->children()[0];
    child->addChild(LayerImpl::create(3));
    LayerImpl* grandChild = child->children()[0];

    // Adding children is an internal operation and should not mark layers as changed.
    EXPECT_FALSE(root->layerPropertyChanged());
    EXPECT_FALSE(child->layerPropertyChanged());
    EXPECT_FALSE(grandChild->layerPropertyChanged());

    FloatPoint arbitraryFloatPoint = FloatPoint(0.125f, 0.25f);
    float arbitraryNumber = 0.352f;
    IntSize arbitraryIntSize = IntSize(111, 222);
    IntPoint arbitraryIntPoint = IntPoint(333, 444);
    IntRect arbitraryIntRect = IntRect(arbitraryIntPoint, arbitraryIntSize);
    FloatRect arbitraryFloatRect = FloatRect(arbitraryFloatPoint, FloatSize(1.234f, 5.678f));
    SkColor arbitraryColor = SkColorSetRGB(10, 20, 30);
    WebTransformationMatrix arbitraryTransform;
    arbitraryTransform.scale3d(0.1, 0.2, 0.3);
    WebFilterOperations arbitraryFilters;
    arbitraryFilters.append(WebFilterOperation::createOpacityFilter(0.5));

    // These properties are internal, and should not be considered "change" when they are used.
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setUseLCDText(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDrawOpacity(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setRenderTarget(0));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDrawTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setScreenSpaceTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDrawableContentRect(arbitraryIntRect));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setUpdateRect(arbitraryFloatRect));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setVisibleContentRect(arbitraryIntRect));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setMaxScrollPosition(arbitraryIntSize));

    // Changing these properties affects the entire subtree of layers.
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setAnchorPoint(arbitraryFloatPoint));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setAnchorPointZ(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setFilters(arbitraryFilters));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setMaskLayer(LayerImpl::create(4)));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setMasksToBounds(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setContentsOpaque(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setReplicaLayer(LayerImpl::create(5)));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setPosition(arbitraryFloatPoint));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setPreserves3D(true));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setDoubleSided(false)); // constructor initializes it to "true".
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->scrollBy(arbitraryIntSize));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setScrollDelta(IntSize()));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setScrollPosition(arbitraryIntPoint));
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setImplTransform(arbitraryTransform));

    // Changing these properties only affects the layer itself.
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setContentBounds(arbitraryIntSize));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setDebugBorderColor(arbitraryColor));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setDebugBorderWidth(arbitraryNumber));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setDrawsContent(true));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setBackgroundColor(SK_ColorGRAY));
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setBackgroundFilters(arbitraryFilters));

    // Changing these properties only affects how render surface is drawn
    EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(root->setOpacity(arbitraryNumber));
    EXECUTE_AND_VERIFY_ONLY_SURFACE_CHANGED(root->setTransform(arbitraryTransform));

    // Special case: check that sublayer transform changes all layer's descendants, but not the layer itself.
    root->resetAllChangeTrackingForSubtree();
    root->setSublayerTransform(arbitraryTransform);
    EXPECT_FALSE(root->layerPropertyChanged());
    EXPECT_TRUE(child->layerPropertyChanged());
    EXPECT_TRUE(grandChild->layerPropertyChanged());

    // Special case: check that setBounds changes behavior depending on masksToBounds.
    root->setMasksToBounds(false);
    EXECUTE_AND_VERIFY_ONLY_LAYER_CHANGED(root->setBounds(IntSize(135, 246)));
    root->setMasksToBounds(true);
    EXECUTE_AND_VERIFY_SUBTREE_CHANGED(root->setBounds(arbitraryIntSize)); // should be a different size than previous call, to ensure it marks tree changed.

    // After setting all these properties already, setting to the exact same values again should
    // not cause any change.
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setAnchorPoint(arbitraryFloatPoint));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setAnchorPointZ(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setMasksToBounds(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setPosition(arbitraryFloatPoint));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setPreserves3D(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDoubleSided(false)); // constructor initializes it to "true".
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setScrollDelta(IntSize()));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setScrollPosition(arbitraryIntPoint));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setImplTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setContentBounds(arbitraryIntSize));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setContentsOpaque(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setOpacity(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDebugBorderColor(arbitraryColor));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDebugBorderWidth(arbitraryNumber));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setDrawsContent(true));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setSublayerTransform(arbitraryTransform));
    EXECUTE_AND_VERIFY_SUBTREE_DID_NOT_CHANGE(root->setBounds(arbitraryIntSize));
}

} // namespace
