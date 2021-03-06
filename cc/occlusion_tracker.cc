// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/occlusion_tracker.h"

#include <algorithm>

#include "cc/layer.h"
#include "cc/layer_impl.h"
#include "cc/math_util.h"
#include "cc/overdraw_metrics.h"

using namespace std;
using WebKit::WebTransformationMatrix;

namespace cc {

template<typename LayerType, typename RenderSurfaceType>
OcclusionTrackerBase<LayerType, RenderSurfaceType>::OcclusionTrackerBase(IntRect rootTargetRect, bool recordMetricsForFrame)
    : m_rootTargetRect(rootTargetRect)
    , m_overdrawMetrics(OverdrawMetrics::create(recordMetricsForFrame))
    , m_occludingScreenSpaceRects(0)
{
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::enterLayer(const LayerIteratorPosition<LayerType>& layerIterator)
{
    LayerType* renderTarget = layerIterator.targetRenderSurfaceLayer;

    if (layerIterator.representsItself)
        enterRenderTarget(renderTarget);
    else if (layerIterator.representsTargetRenderSurface)
        finishedRenderTarget(renderTarget);
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::leaveLayer(const LayerIteratorPosition<LayerType>& layerIterator)
{
    LayerType* renderTarget = layerIterator.targetRenderSurfaceLayer;

    if (layerIterator.representsItself)
        markOccludedBehindLayer(layerIterator.currentLayer);
    else if (layerIterator.representsContributingRenderSurface)
        leaveToRenderTarget(renderTarget);
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::enterRenderTarget(const LayerType* newTarget)
{
    if (!m_stack.isEmpty() && m_stack.last().target == newTarget)
        return;

    const LayerType* oldTarget = m_stack.isEmpty() ? 0 : m_stack.last().target;
    const RenderSurfaceType* oldAncestorThatMovesPixels = !oldTarget ? 0 : oldTarget->renderSurface()->nearestAncestorThatMovesPixels();
    const RenderSurfaceType* newAncestorThatMovesPixels = newTarget->renderSurface()->nearestAncestorThatMovesPixels();

    m_stack.append(StackObject(newTarget));

    // We copy the screen occlusion into the new RenderSurfaceImpl subtree, but we never copy in the
    // target occlusion, since we are looking at a new RenderSurfaceImpl target.

    // If we are entering a subtree that is going to move pixels around, then the occlusion we've computed
    // so far won't apply to the pixels we're drawing here in the same way. We discard the occlusion thus
    // far to be safe, and ensure we don't cull any pixels that are moved such that they become visible.
    bool enteringSubtreeThatMovesPixels = newAncestorThatMovesPixels && newAncestorThatMovesPixels != oldAncestorThatMovesPixels;

    bool copyScreenOcclusionForward = m_stack.size() > 1 && !enteringSubtreeThatMovesPixels;
    if (copyScreenOcclusionForward) {
        int lastIndex = m_stack.size() - 1;
        m_stack[lastIndex].occlusionInScreen = m_stack[lastIndex - 1].occlusionInScreen;
    }
}

static inline bool layerOpacityKnown(const Layer* layer) { return !layer->drawOpacityIsAnimating(); }
static inline bool layerOpacityKnown(const LayerImpl*) { return true; }
static inline bool layerTransformsToTargetKnown(const Layer* layer) { return !layer->drawTransformIsAnimating(); }
static inline bool layerTransformsToTargetKnown(const LayerImpl*) { return true; }
static inline bool layerTransformsToScreenKnown(const Layer* layer) { return !layer->screenSpaceTransformIsAnimating(); }
static inline bool layerTransformsToScreenKnown(const LayerImpl*) { return true; }

static inline bool surfaceOpacityKnown(const RenderSurface* surface) { return !surface->drawOpacityIsAnimating(); }
static inline bool surfaceOpacityKnown(const RenderSurfaceImpl*) { return true; }
static inline bool surfaceTransformsToTargetKnown(const RenderSurface* surface) { return !surface->targetSurfaceTransformsAreAnimating(); }
static inline bool surfaceTransformsToTargetKnown(const RenderSurfaceImpl*) { return true; }
static inline bool surfaceTransformsToScreenKnown(const RenderSurface* surface) { return !surface->screenSpaceTransformsAreAnimating(); }
static inline bool surfaceTransformsToScreenKnown(const RenderSurfaceImpl*) { return true; }

static inline bool layerIsInUnsorted3dRenderingContext(const Layer* layer) { return layer->parent() && layer->parent()->preserves3D(); }
static inline bool layerIsInUnsorted3dRenderingContext(const LayerImpl*) { return false; }

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::finishedRenderTarget(const LayerType* finishedTarget)
{
    // Make sure we know about the target surface.
    enterRenderTarget(finishedTarget);

    RenderSurfaceType* surface = finishedTarget->renderSurface();

    // If the occlusion within the surface can not be applied to things outside of the surface's subtree, then clear the occlusion here so it won't be used.
    if (finishedTarget->maskLayer() || !surfaceOpacityKnown(surface) || surface->drawOpacity() < 1 || finishedTarget->filters().hasFilterThatAffectsOpacity()) {
        m_stack.last().occlusionInScreen = Region();
        m_stack.last().occlusionInTarget = Region();
    } else {
        if (!surfaceTransformsToTargetKnown(surface))
            m_stack.last().occlusionInTarget = Region();
        if (!surfaceTransformsToScreenKnown(surface))
            m_stack.last().occlusionInScreen = Region();
    }
}

template<typename RenderSurfaceType>
static inline Region transformSurfaceOpaqueRegion(const RenderSurfaceType* surface, const Region& region, const WebTransformationMatrix& transform)
{
    // Verify that rects within the |surface| will remain rects in its target surface after applying |transform|. If this is true, then
    // apply |transform| to each rect within |region| in order to transform the entire Region.

    bool clipped;
    FloatQuad transformedBoundsQuad = MathUtil::mapQuad(transform, FloatQuad(region.bounds()), clipped);
    // FIXME: Find a rect interior to each transformed quad.
    if (clipped || !transformedBoundsQuad.isRectilinear())
        return Region();

    Region transformedRegion;

    Vector<WebCore::IntRect> rects = region.rects();
    for (size_t i = 0; i < rects.size(); ++i) {
        // We've already checked for clipping in the mapQuad call above, these calls should not clip anything further.
        IntRect transformedRect = enclosedIntRect(MathUtil::mapClippedRect(transform, FloatRect(rects[i])));
        if (!surface->clipRect().isEmpty())
            transformedRect.intersect(surface->clipRect());
        transformedRegion.unite(transformedRect);
    }
    return transformedRegion;
}

static inline void reduceOcclusion(const IntRect& affectedArea, const IntRect& expandedPixel, Region& occlusion)
{
    if (affectedArea.isEmpty())
        return;

    Region affectedOcclusion = intersect(occlusion, affectedArea);
    Vector<WebCore::IntRect> affectedOcclusionRects = affectedOcclusion.rects();

    occlusion.subtract(affectedArea);
    for (size_t j = 0; j < affectedOcclusionRects.size(); ++j) {
        WebCore::IntRect& occlusionRect = affectedOcclusionRects[j];

        // Shrink the rect by expanding the non-opaque pixels outside the rect.

        // The expandedPixel is the IntRect for a single pixel after being
        // expanded by filters on the layer. The original pixel would be
        // IntRect(0, 0, 1, 1), and the expanded pixel is the rect, relative
        // to this original rect, that the original pixel can influence after
        // being filtered.
        // To convert the expandedPixel IntRect back to filter outsets:
        // x = -leftOutset
        // width = leftOutset + rightOutset
        // maxX = x + width = -leftOutset + leftOutset + rightOutset = rightOutset

        // The leftOutset of the filters moves pixels on the right side of
        // the occlusionRect into it, shrinking its right edge.
        int shrinkLeft = occlusionRect.x() == affectedArea.x() ? 0 : expandedPixel.maxX();
        int shrinkTop = occlusionRect.y() == affectedArea.y() ? 0 : expandedPixel.maxY();
        int shrinkRight = occlusionRect.maxX() == affectedArea.maxX() ? 0 : -expandedPixel.x();
        int shrinkBottom = occlusionRect.maxY() == affectedArea.maxY() ? 0 : -expandedPixel.y();

        occlusionRect.move(shrinkLeft, shrinkTop);
        occlusionRect.contract(shrinkLeft + shrinkRight, shrinkTop + shrinkBottom);

        occlusion.unite(occlusionRect);
    }
}

template<typename LayerType>
static void reduceOcclusionBelowSurface(LayerType* contributingLayer, const IntRect& surfaceRect, const WebTransformationMatrix& surfaceTransform, LayerType* renderTarget, Region& occlusionInTarget, Region& occlusionInScreen)
{
    if (surfaceRect.isEmpty())
        return;

    IntRect boundsInTarget = enclosingIntRect(MathUtil::mapClippedRect(surfaceTransform, FloatRect(surfaceRect)));
    if (!contributingLayer->renderSurface()->clipRect().isEmpty())
        boundsInTarget.intersect(contributingLayer->renderSurface()->clipRect());

    int outsetTop, outsetRight, outsetBottom, outsetLeft;
    contributingLayer->backgroundFilters().getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

    // The filter can move pixels from outside of the clip, so allow affectedArea to expand outside the clip.
    boundsInTarget.move(-outsetLeft, -outsetTop);
    boundsInTarget.expand(outsetLeft + outsetRight, outsetTop + outsetBottom);

    IntRect boundsInScreen = enclosingIntRect(MathUtil::mapClippedRect(renderTarget->renderSurface()->screenSpaceTransform(), FloatRect(boundsInTarget)));

    IntRect filterOutsetsInTarget(-outsetLeft, -outsetTop, outsetLeft + outsetRight, outsetTop + outsetBottom);
    IntRect filterOutsetsInScreen = enclosingIntRect(MathUtil::mapClippedRect(renderTarget->renderSurface()->screenSpaceTransform(), FloatRect(filterOutsetsInTarget)));

    reduceOcclusion(boundsInTarget, filterOutsetsInTarget, occlusionInTarget);
    reduceOcclusion(boundsInScreen, filterOutsetsInScreen, occlusionInScreen);
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::leaveToRenderTarget(const LayerType* newTarget)
{
    int lastIndex = m_stack.size() - 1;
    bool surfaceWillBeAtTopAfterPop = m_stack.size() > 1 && m_stack[lastIndex - 1].target == newTarget;

    // We merge the screen occlusion from the current RenderSurfaceImpl subtree out to its parent target RenderSurfaceImpl.
    // The target occlusion can be merged out as well but needs to be transformed to the new target.

    const LayerType* oldTarget = m_stack[lastIndex].target;
    const RenderSurfaceType* oldSurface = oldTarget->renderSurface();
    Region oldTargetOcclusionInNewTarget = transformSurfaceOpaqueRegion<RenderSurfaceType>(oldSurface, m_stack[lastIndex].occlusionInTarget, oldSurface->drawTransform());
    if (oldTarget->hasReplica() && !oldTarget->replicaHasMask())
        oldTargetOcclusionInNewTarget.unite(transformSurfaceOpaqueRegion<RenderSurfaceType>(oldSurface, m_stack[lastIndex].occlusionInTarget, oldSurface->replicaDrawTransform()));

    IntRect unoccludedSurfaceRect;
    IntRect unoccludedReplicaRect;
    if (oldTarget->backgroundFilters().hasFilterThatMovesPixels()) {
        unoccludedSurfaceRect = unoccludedContributingSurfaceContentRect(oldTarget, false, oldSurface->contentRect());
        if (oldTarget->hasReplica())
            unoccludedReplicaRect = unoccludedContributingSurfaceContentRect(oldTarget, true, oldSurface->contentRect());
    }

    if (surfaceWillBeAtTopAfterPop) {
        // Merge the top of the stack down.
        m_stack[lastIndex - 1].occlusionInScreen.unite(m_stack[lastIndex].occlusionInScreen);
        m_stack[lastIndex - 1].occlusionInTarget.unite(oldTargetOcclusionInNewTarget);
        m_stack.removeLast();
    } else {
        // Replace the top of the stack with the new pushed surface. Copy the occluded screen region to the top.
        m_stack.last().target = newTarget;
        m_stack.last().occlusionInTarget = oldTargetOcclusionInNewTarget;
    }

    if (oldTarget->backgroundFilters().hasFilterThatMovesPixels()) {
        reduceOcclusionBelowSurface(oldTarget, unoccludedSurfaceRect, oldSurface->drawTransform(), newTarget, m_stack.last().occlusionInTarget, m_stack.last().occlusionInScreen);
        if (oldTarget->hasReplica())
            reduceOcclusionBelowSurface(oldTarget, unoccludedReplicaRect, oldSurface->replicaDrawTransform(), newTarget, m_stack.last().occlusionInTarget, m_stack.last().occlusionInScreen);
    }
}

// FIXME: Remove usePaintTracking when paint tracking is on for paint culling.
template<typename LayerType>
static inline void addOcclusionBehindLayer(Region& region, const LayerType* layer, const WebTransformationMatrix& transform, const Region& opaqueContents, const IntRect& clipRectInTarget, const IntSize& minimumTrackingSize, Vector<IntRect>* occludingScreenSpaceRects)
{
    DCHECK(layer->visibleContentRect().contains(opaqueContents.bounds()));

    bool clipped;
    FloatQuad visibleTransformedQuad = MathUtil::mapQuad(transform, FloatQuad(layer->visibleContentRect()), clipped);
    // FIXME: Find a rect interior to each transformed quad.
    if (clipped || !visibleTransformedQuad.isRectilinear())
        return;

    Vector<WebCore::IntRect> contentRects = opaqueContents.rects();
    for (size_t i = 0; i < contentRects.size(); ++i) {
        // We've already checked for clipping in the mapQuad call above, these calls should not clip anything further.
        IntRect transformedRect = enclosedIntRect(MathUtil::mapClippedRect(transform, FloatRect(contentRects[i])));
        transformedRect.intersect(clipRectInTarget);
        if (transformedRect.width() >= minimumTrackingSize.width() || transformedRect.height() >= minimumTrackingSize.height()) {
            if (occludingScreenSpaceRects)
                occludingScreenSpaceRects->append(transformedRect);
            region.unite(transformedRect);
        }
    }
}

template<typename LayerType, typename RenderSurfaceType>
void OcclusionTrackerBase<LayerType, RenderSurfaceType>::markOccludedBehindLayer(const LayerType* layer)
{
    DCHECK(!m_stack.isEmpty());
    DCHECK(layer->renderTarget() == m_stack.last().target);
    if (m_stack.isEmpty())
        return;

    if (!layerOpacityKnown(layer) || layer->drawOpacity() < 1)
        return;

    if (layerIsInUnsorted3dRenderingContext(layer))
        return;

    Region opaqueContents = layer->visibleContentOpaqueRegion();
    if (opaqueContents.isEmpty())
        return;

    IntRect clipRectInTarget = layerClipRectInTarget(layer);
    if (layerTransformsToTargetKnown(layer))
        addOcclusionBehindLayer<LayerType>(m_stack.last().occlusionInTarget, layer, layer->drawTransform(), opaqueContents, clipRectInTarget, m_minimumTrackingSize, 0);

    // We must clip the occlusion within the layer's clipRectInTarget within screen space as well. If the clip rect can't be moved to screen space and
    // remain rectilinear, then we don't add any occlusion in screen space.

    if (layerTransformsToScreenKnown(layer)) {
        WebTransformationMatrix targetToScreenTransform = m_stack.last().target->renderSurface()->screenSpaceTransform();
        bool clipped;
        FloatQuad clipQuadInScreen = MathUtil::mapQuad(targetToScreenTransform, FloatQuad(FloatRect(clipRectInTarget)), clipped);
        // FIXME: Find a rect interior to the transformed clip quad.
        if (clipped || !clipQuadInScreen.isRectilinear())
            return;
        IntRect clipRectInScreen = intersection(m_rootTargetRect, enclosedIntRect(clipQuadInScreen.boundingBox()));
        addOcclusionBehindLayer<LayerType>(m_stack.last().occlusionInScreen, layer, layer->screenSpaceTransform(), opaqueContents, clipRectInScreen, m_minimumTrackingSize, m_occludingScreenSpaceRects);
    }
}

static inline bool testContentRectOccluded(const IntRect& contentRect, const WebTransformationMatrix& contentSpaceTransform, const IntRect& clipRectInTarget, const Region& occlusion)
{
    FloatRect transformedRect = MathUtil::mapClippedRect(contentSpaceTransform, FloatRect(contentRect));
    // Take the enclosingIntRect, as we want to include partial pixels in the test.
    IntRect targetRect = intersection(enclosingIntRect(transformedRect), clipRectInTarget);
    return targetRect.isEmpty() || occlusion.contains(targetRect);
}

template<typename LayerType, typename RenderSurfaceType>
bool OcclusionTrackerBase<LayerType, RenderSurfaceType>::occluded(const LayerType* layer, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const
{
    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = false;

    DCHECK(!m_stack.isEmpty());
    if (m_stack.isEmpty())
        return false;
    if (contentRect.isEmpty())
        return true;

    DCHECK(layer->renderTarget() == m_stack.last().target);

    if (layerTransformsToTargetKnown(layer) && testContentRectOccluded(contentRect, layer->drawTransform(), layerClipRectInTarget(layer), m_stack.last().occlusionInTarget))
        return true;

    if (layerTransformsToScreenKnown(layer) && testContentRectOccluded(contentRect, layer->screenSpaceTransform(), m_rootTargetRect, m_stack.last().occlusionInScreen)) {
        if (hasOcclusionFromOutsideTargetSurface)
            *hasOcclusionFromOutsideTargetSurface = true;
        return true;
    }

    return false;
}

// Determines what portion of rect, if any, is unoccluded (not occluded by region). If
// the resulting unoccluded region is not rectangular, we return a rect containing it.
static inline IntRect rectSubtractRegion(const IntRect& rect, const Region& region)
{
    Region rectRegion(rect);
    rectRegion.subtract(region);
    return rectRegion.bounds();
}

static inline IntRect computeUnoccludedContentRect(const IntRect& contentRect, const WebTransformationMatrix& contentSpaceTransform, const IntRect& clipRectInTarget, const Region& occlusion)
{
    if (!contentSpaceTransform.isInvertible())
        return contentRect;

    // Take the enclosingIntRect at each step, as we want to contain any unoccluded partial pixels in the resulting IntRect.
    FloatRect transformedRect = MathUtil::mapClippedRect(contentSpaceTransform, FloatRect(contentRect));
    IntRect shrunkRect = rectSubtractRegion(intersection(enclosingIntRect(transformedRect), clipRectInTarget), occlusion);
    IntRect unoccludedRect = enclosingIntRect(MathUtil::projectClippedRect(contentSpaceTransform.inverse(), FloatRect(shrunkRect)));
    // The rect back in content space is a bounding box and may extend outside of the original contentRect, so clamp it to the contentRectBounds.
    return intersection(unoccludedRect, contentRect);
}

template<typename LayerType, typename RenderSurfaceType>
IntRect OcclusionTrackerBase<LayerType, RenderSurfaceType>::unoccludedContentRect(const LayerType* layer, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const
{
    DCHECK(!m_stack.isEmpty());
    if (m_stack.isEmpty())
        return contentRect;
    if (contentRect.isEmpty())
        return contentRect;

    DCHECK(layer->renderTarget() == m_stack.last().target);

    // We want to return a rect that contains all the visible parts of |contentRect| in both screen space and in the target surface.
    // So we find the visible parts of |contentRect| in each space, and take the intersection.

    IntRect unoccludedInScreen = contentRect;
    if (layerTransformsToScreenKnown(layer))
        unoccludedInScreen = computeUnoccludedContentRect(contentRect, layer->screenSpaceTransform(), m_rootTargetRect, m_stack.last().occlusionInScreen);

    IntRect unoccludedInTarget = contentRect;
    if (layerTransformsToTargetKnown(layer))
        unoccludedInTarget = computeUnoccludedContentRect(contentRect, layer->drawTransform(), layerClipRectInTarget(layer), m_stack.last().occlusionInTarget);

    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = (intersection(unoccludedInScreen, unoccludedInTarget) != unoccludedInTarget);

    return intersection(unoccludedInScreen, unoccludedInTarget);
}

template<typename LayerType, typename RenderSurfaceType>
IntRect OcclusionTrackerBase<LayerType, RenderSurfaceType>::unoccludedContributingSurfaceContentRect(const LayerType* layer, bool forReplica, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const
{
    DCHECK(!m_stack.isEmpty());
    // The layer is a contributing renderTarget so it should have a surface.
    DCHECK(layer->renderSurface());
    // The layer is a contributing renderTarget so its target should be itself.
    DCHECK(layer->renderTarget() == layer);
    // The layer should not be the root, else what is is contributing to?
    DCHECK(layer->parent());
    // This should be called while the layer is still considered the current target in the occlusion tracker.
    DCHECK(layer == m_stack.last().target);

    if (contentRect.isEmpty())
        return contentRect;

    RenderSurfaceType* surface = layer->renderSurface();

    IntRect surfaceClipRect = surface->clipRect();
    if (surfaceClipRect.isEmpty()) {
        LayerType* contributingSurfaceRenderTarget = layer->parent()->renderTarget();
        surfaceClipRect = intersection(contributingSurfaceRenderTarget->renderSurface()->contentRect(), enclosingIntRect(surface->drawableContentRect()));
    }

    // A contributing surface doesn't get occluded by things inside its own surface, so only things outside the surface can occlude it. That occlusion is
    // found just below the top of the stack (if it exists).
    bool hasOcclusion = m_stack.size() > 1;

    const WebTransformationMatrix& transformToScreen = forReplica ? surface->replicaScreenSpaceTransform() : surface->screenSpaceTransform();
    const WebTransformationMatrix& transformToTarget = forReplica ? surface->replicaDrawTransform() : surface->drawTransform();

    IntRect unoccludedInScreen = contentRect;
    if (surfaceTransformsToScreenKnown(surface)) {
        if (hasOcclusion) {
            const StackObject& secondLast = m_stack[m_stack.size() - 2];
            unoccludedInScreen = computeUnoccludedContentRect(contentRect, transformToScreen, m_rootTargetRect, secondLast.occlusionInScreen);
        } else
            unoccludedInScreen = computeUnoccludedContentRect(contentRect, transformToScreen, m_rootTargetRect, Region());
    }

    IntRect unoccludedInTarget = contentRect;
    if (surfaceTransformsToTargetKnown(surface)) {
        if (hasOcclusion) {
            const StackObject& secondLast = m_stack[m_stack.size() - 2];
            unoccludedInTarget = computeUnoccludedContentRect(contentRect, transformToTarget, surfaceClipRect, secondLast.occlusionInTarget);
        } else
            unoccludedInTarget = computeUnoccludedContentRect(contentRect, transformToTarget, surfaceClipRect, Region());
    }

    if (hasOcclusionFromOutsideTargetSurface)
        *hasOcclusionFromOutsideTargetSurface = (intersection(unoccludedInScreen, unoccludedInTarget) != unoccludedInTarget);

    return intersection(unoccludedInScreen, unoccludedInTarget);
}

template<typename LayerType, typename RenderSurfaceType>
IntRect OcclusionTrackerBase<LayerType, RenderSurfaceType>::layerClipRectInTarget(const LayerType* layer) const
{
    // FIXME: we could remove this helper function, but unit tests currently override this
    //        function, and they need to be verified/adjusted before this can be removed.
    return layer->drawableContentRect();
}

// Declare the possible functions here for the linker.
template OcclusionTrackerBase<Layer, RenderSurface>::OcclusionTrackerBase(IntRect rootTargetRect, bool recordMetricsForFrame);
template void OcclusionTrackerBase<Layer, RenderSurface>::enterLayer(const LayerIteratorPosition<Layer>&);
template void OcclusionTrackerBase<Layer, RenderSurface>::leaveLayer(const LayerIteratorPosition<Layer>&);
template void OcclusionTrackerBase<Layer, RenderSurface>::enterRenderTarget(const Layer* newTarget);
template void OcclusionTrackerBase<Layer, RenderSurface>::finishedRenderTarget(const Layer* finishedTarget);
template void OcclusionTrackerBase<Layer, RenderSurface>::leaveToRenderTarget(const Layer* newTarget);
template void OcclusionTrackerBase<Layer, RenderSurface>::markOccludedBehindLayer(const Layer*);
template bool OcclusionTrackerBase<Layer, RenderSurface>::occluded(const Layer*, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const;
template IntRect OcclusionTrackerBase<Layer, RenderSurface>::unoccludedContentRect(const Layer*, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const;
template IntRect OcclusionTrackerBase<Layer, RenderSurface>::unoccludedContributingSurfaceContentRect(const Layer*, bool forReplica, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const;
template IntRect OcclusionTrackerBase<Layer, RenderSurface>::layerClipRectInTarget(const Layer*) const;

template OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::OcclusionTrackerBase(IntRect rootTargetRect, bool recordMetricsForFrame);
template void OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::enterLayer(const LayerIteratorPosition<LayerImpl>&);
template void OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::leaveLayer(const LayerIteratorPosition<LayerImpl>&);
template void OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::enterRenderTarget(const LayerImpl* newTarget);
template void OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::finishedRenderTarget(const LayerImpl* finishedTarget);
template void OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::leaveToRenderTarget(const LayerImpl* newTarget);
template void OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::markOccludedBehindLayer(const LayerImpl*);
template bool OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::occluded(const LayerImpl*, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const;
template IntRect OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::unoccludedContentRect(const LayerImpl*, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const;
template IntRect OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::unoccludedContributingSurfaceContentRect(const LayerImpl*, bool forReplica, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface) const;
template IntRect OcclusionTrackerBase<LayerImpl, RenderSurfaceImpl>::layerClipRectInTarget(const LayerImpl*) const;


} // namespace cc
