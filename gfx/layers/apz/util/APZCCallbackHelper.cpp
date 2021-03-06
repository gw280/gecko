/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCCallbackHelper.h"

#include "gfxPlatform.h" // For gfxPlatform::UseTiling
#include "mozilla/dom/TabParent.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsIDOMElement.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"

#define APZCCH_LOG(...)
// #define APZCCH_LOG(...) printf_stderr("APZCCH: " __VA_ARGS__)

namespace mozilla {
namespace layers {

using dom::TabParent;

bool
APZCCallbackHelper::HasValidPresShellId(nsIDOMWindowUtils* aUtils,
                                        const FrameMetrics& aMetrics)
{
    MOZ_ASSERT(aUtils);

    uint32_t presShellId;
    nsresult rv = aUtils->GetPresShellId(&presShellId);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return NS_SUCCEEDED(rv) && aMetrics.GetPresShellId() == presShellId;
}

static void
AdjustDisplayPortForScrollDelta(mozilla::layers::FrameMetrics& aFrameMetrics,
                                const CSSPoint& aActualScrollOffset)
{
    // Correct the display-port by the difference between the requested scroll
    // offset and the resulting scroll offset after setting the requested value.
    ScreenPoint shift =
        (aFrameMetrics.GetScrollOffset() - aActualScrollOffset) *
        aFrameMetrics.DisplayportPixelsPerCSSPixel();
    ScreenMargin margins = aFrameMetrics.GetDisplayPortMargins();
    margins.left -= shift.x;
    margins.right += shift.x;
    margins.top -= shift.y;
    margins.bottom += shift.y;
    aFrameMetrics.SetDisplayPortMargins(margins);
}

static void
RecenterDisplayPort(mozilla::layers::FrameMetrics& aFrameMetrics)
{
    ScreenMargin margins = aFrameMetrics.GetDisplayPortMargins();
    margins.right = margins.left = margins.LeftRight() / 2;
    margins.top = margins.bottom = margins.TopBottom() / 2;
    aFrameMetrics.SetDisplayPortMargins(margins);
}

static CSSPoint
ScrollFrameTo(nsIScrollableFrame* aFrame, const CSSPoint& aPoint, bool& aSuccessOut)
{
  aSuccessOut = false;

  if (!aFrame) {
    return aPoint;
  }

  CSSPoint targetScrollPosition = aPoint;

  // If the frame is overflow:hidden on a particular axis, we don't want to allow
  // user-driven scroll on that axis. Simply set the scroll position on that axis
  // to whatever it already is. Note that this will leave the APZ's async scroll
  // position out of sync with the gecko scroll position, but APZ can deal with that
  // (by design). Note also that when we run into this case, even if both axes
  // have overflow:hidden, we want to set aSuccessOut to true, so that the displayport
  // follows the async scroll position rather than the gecko scroll position.
  CSSPoint geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());
  if (aFrame->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_HIDDEN) {
    targetScrollPosition.y = geckoScrollPosition.y;
  }
  if (aFrame->GetScrollbarStyles().mHorizontal == NS_STYLE_OVERFLOW_HIDDEN) {
    targetScrollPosition.x = geckoScrollPosition.x;
  }

  // If the scrollable frame is currently in the middle of an async or smooth
  // scroll then we don't want to interrupt it (see bug 961280).
  // Also if the scrollable frame got a scroll request from something other than us
  // since the last layers update, then we don't want to push our scroll request
  // because we'll clobber that one, which is bad.
  bool scrollInProgress = aFrame->IsProcessingAsyncScroll()
      || (aFrame->LastScrollOrigin() && aFrame->LastScrollOrigin() != nsGkAtoms::apz)
      || aFrame->LastSmoothScrollOrigin();
  if (!scrollInProgress) {
    aFrame->ScrollToCSSPixelsApproximate(targetScrollPosition, nsGkAtoms::apz);
    geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());
    aSuccessOut = true;
  }
  // Return the final scroll position after setting it so that anything that relies
  // on it can have an accurate value. Note that even if we set it above re-querying it
  // is a good idea because it may have gotten clamped or rounded.
  return geckoScrollPosition;
}

void
APZCCallbackHelper::UpdateRootFrame(nsIDOMWindowUtils* aUtils,
                                    FrameMetrics& aMetrics)
{
    // Precondition checks
    MOZ_ASSERT(aUtils);
    MOZ_ASSERT(aMetrics.GetUseDisplayPortMargins());
    if (aMetrics.GetScrollId() == FrameMetrics::NULL_SCROLL_ID) {
        return;
    }

    // Set the scroll port size, which determines the scroll range. For example if
    // a 500-pixel document is shown in a 100-pixel frame, the scroll port length would
    // be 100, and gecko would limit the maximum scroll offset to 400 (so as to prevent
    // overscroll). Note that if the content here was zoomed to 2x, the document would
    // be 1000 pixels long but the frame would still be 100 pixels, and so the maximum
    // scroll range would be 900. Therefore this calculation depends on the zoom applied
    // to the content relative to the container.
    CSSSize scrollPort = aMetrics.CalculateCompositedSizeInCssPixels();
    aUtils->SetScrollPositionClampingScrollPortSize(scrollPort.width, scrollPort.height);

    // Scroll the window to the desired spot
    nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aMetrics.GetScrollId());
    bool scrollUpdated = false;
    CSSPoint actualScrollOffset = ScrollFrameTo(sf, aMetrics.GetScrollOffset(), scrollUpdated);

    if (scrollUpdated) {
        // Correct the display port due to the difference between mScrollOffset and the
        // actual scroll offset.
        AdjustDisplayPortForScrollDelta(aMetrics, actualScrollOffset);
    } else {
        // For whatever reason we couldn't update the scroll offset on the scroll frame,
        // which means the data APZ used for its displayport calculation is stale. Fall
        // back to a sane default behaviour. Note that we don't tile-align the recentered
        // displayport because tile-alignment depends on the scroll position, and the
        // scroll position here is out of our control. See bug 966507 comment 21 for a
        // more detailed explanation.
        RecenterDisplayPort(aMetrics);
    }

    aMetrics.SetScrollOffset(actualScrollOffset);

    // The pres shell resolution is updated by the the async zoom since the
    // last paint.
    float presShellResolution = aMetrics.GetPresShellResolution()
                              * aMetrics.GetAsyncZoom().scale;
    aUtils->SetResolutionAndScaleTo(presShellResolution, presShellResolution);

    // Finally, we set the displayport.
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aMetrics.GetScrollId());
    if (!content) {
        return;
    }
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(content);
    if (!element) {
        return;
    }

    ScreenMargin margins = aMetrics.GetDisplayPortMargins();
    aUtils->SetDisplayPortMarginsForElement(margins.left,
                                            margins.top,
                                            margins.right,
                                            margins.bottom,
                                            element, 0);
    CSSRect baseCSS = aMetrics.CalculateCompositedRectInCssPixels();
    nsRect base(0, 0,
                baseCSS.width * nsPresContext::AppUnitsPerCSSPixel(),
                baseCSS.height * nsPresContext::AppUnitsPerCSSPixel());
    nsLayoutUtils::SetDisplayPortBaseIfNotSet(content, base);
}

void
APZCCallbackHelper::UpdateSubFrame(nsIContent* aContent,
                                   FrameMetrics& aMetrics)
{
    // Precondition checks
    MOZ_ASSERT(aContent);
    MOZ_ASSERT(aMetrics.GetUseDisplayPortMargins());
    if (aMetrics.GetScrollId() == FrameMetrics::NULL_SCROLL_ID) {
        return;
    }

    nsCOMPtr<nsIDOMWindowUtils> utils = GetDOMWindowUtils(aContent);
    if (!utils) {
        return;
    }

    // We currently do not support zooming arbitrary subframes. They can only
    // be scrolled, so here we only have to set the scroll position and displayport.

    nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aMetrics.GetScrollId());
    bool scrollUpdated = false;
    CSSPoint actualScrollOffset = ScrollFrameTo(sf, aMetrics.GetScrollOffset(), scrollUpdated);

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aContent);
    if (element) {
        if (scrollUpdated) {
            AdjustDisplayPortForScrollDelta(aMetrics, actualScrollOffset);
        } else {
            RecenterDisplayPort(aMetrics);
        }
        ScreenMargin margins = aMetrics.GetDisplayPortMargins();
        utils->SetDisplayPortMarginsForElement(margins.left,
                                               margins.top,
                                               margins.right,
                                               margins.bottom,
                                               element, 0);
        CSSRect baseCSS = aMetrics.CalculateCompositedRectInCssPixels();
        nsRect base(0, 0,
                    baseCSS.width * nsPresContext::AppUnitsPerCSSPixel(),
                    baseCSS.height * nsPresContext::AppUnitsPerCSSPixel());
        nsLayoutUtils::SetDisplayPortBaseIfNotSet(aContent, base);
    }

    aMetrics.SetScrollOffset(actualScrollOffset);
}

already_AddRefed<nsIDOMWindowUtils>
APZCCallbackHelper::GetDOMWindowUtils(const nsIDocument* aDoc)
{
    nsCOMPtr<nsIDOMWindowUtils> utils;
    nsCOMPtr<nsIDOMWindow> window = aDoc->GetDefaultView();
    if (window) {
        utils = do_GetInterface(window);
    }
    return utils.forget();
}

already_AddRefed<nsIDOMWindowUtils>
APZCCallbackHelper::GetDOMWindowUtils(const nsIContent* aContent)
{
    nsCOMPtr<nsIDOMWindowUtils> utils;
    nsIDocument* doc = aContent->GetCurrentDoc();
    if (doc) {
        utils = GetDOMWindowUtils(doc);
    }
    return utils.forget();
}

bool
APZCCallbackHelper::GetOrCreateScrollIdentifiers(nsIContent* aContent,
                                                 uint32_t* aPresShellIdOut,
                                                 FrameMetrics::ViewID* aViewIdOut)
{
    if (!aContent) {
        return false;
    }
    *aViewIdOut = nsLayoutUtils::FindOrCreateIDFor(aContent);
    nsCOMPtr<nsIDOMWindowUtils> utils = GetDOMWindowUtils(aContent);
    return utils && (utils->GetPresShellId(aPresShellIdOut) == NS_OK);
}

class AcknowledgeScrollUpdateEvent : public nsRunnable
{
    typedef mozilla::layers::FrameMetrics::ViewID ViewID;

public:
    AcknowledgeScrollUpdateEvent(const ViewID& aScrollId, const uint32_t& aScrollGeneration)
        : mScrollId(aScrollId)
        , mScrollGeneration(aScrollGeneration)
    {
    }

    NS_IMETHOD Run() {
        MOZ_ASSERT(NS_IsMainThread());

        nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(mScrollId);
        if (sf) {
            sf->ResetScrollInfoIfGeneration(mScrollGeneration);
        }

        // Since the APZ and content are in sync, we need to clear any callback transform
        // that might have been set on the last repaint request (which might have failed
        // due to the inflight scroll update that this message is acknowledging).
        nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(mScrollId);
        if (content) {
            content->SetProperty(nsGkAtoms::apzCallbackTransform, new CSSPoint(),
                                 nsINode::DeleteProperty<CSSPoint>);
        }

        return NS_OK;
    }

protected:
    ViewID mScrollId;
    uint32_t mScrollGeneration;
};

void
APZCCallbackHelper::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                            const uint32_t& aScrollGeneration)
{
    nsCOMPtr<nsIRunnable> r1 = new AcknowledgeScrollUpdateEvent(aScrollId, aScrollGeneration);
    if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(r1);
    } else {
        r1->Run();
    }
}

void
APZCCallbackHelper::UpdateCallbackTransform(const FrameMetrics& aApzcMetrics, const FrameMetrics& aActualMetrics)
{
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aApzcMetrics.GetScrollId());
    if (!content) {
        return;
    }
    CSSPoint scrollDelta = aApzcMetrics.GetScrollOffset() - aActualMetrics.GetScrollOffset();
    content->SetProperty(nsGkAtoms::apzCallbackTransform, new CSSPoint(scrollDelta),
                         nsINode::DeleteProperty<CSSPoint>);
}

CSSPoint
APZCCallbackHelper::ApplyCallbackTransform(const CSSPoint& aInput,
                                           const ScrollableLayerGuid& aGuid,
                                           float aPresShellResolution)
{
    // First, scale inversely by the pres shell resolution to cancel the
    // scale-to-resolution transform that the compositor adds to the layer with
    // the pres shell resolution. The points sent to Gecko by APZ don't have
    // this transform unapplied (unlike other compositor-side transforms)
    // because APZ doesn't know about it.
    CSSPoint input = aInput / aPresShellResolution;

    // Now apply the callback-transform.
    // XXX: technically we need to walk all the way up the layer tree from the layer
    // represented by |aGuid.mScrollId| up to the root of the layer tree and apply
    // the input transforms at each level in turn. However, it is quite difficult
    // to do this given that the structure of the layer tree may be different from
    // the structure of the content tree. Also it may be impossible to do correctly
    // at this point because there are other CSS transforms and such interleaved in
    // between so applying the inputTransforms all in a row at the end may leave
    // some things transformed improperly. In practice we should rarely hit scenarios
    // where any of this matters, so I'm skipping it for now and just doing the single
    // transform for the layer that the input hit.

    if (aGuid.mScrollId != FrameMetrics::NULL_SCROLL_ID) {
        nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aGuid.mScrollId);
        if (content) {
            void* property = content->GetProperty(nsGkAtoms::apzCallbackTransform);
            if (property) {
                CSSPoint delta = (*static_cast<CSSPoint*>(property));
                return input + delta;
            }
        }
    }
    return input;
}

LayoutDeviceIntPoint
APZCCallbackHelper::ApplyCallbackTransform(const LayoutDeviceIntPoint& aPoint,
                                           const ScrollableLayerGuid& aGuid,
                                           const CSSToLayoutDeviceScale& aScale,
                                           float aPresShellResolution)
{
    LayoutDevicePoint point = LayoutDevicePoint(aPoint.x, aPoint.y);
    point = ApplyCallbackTransform(point / aScale, aGuid, aPresShellResolution) * aScale;
    return gfx::RoundedToInt(point);
}

nsEventStatus
APZCCallbackHelper::DispatchWidgetEvent(WidgetGUIEvent& aEvent)
{
  if (!aEvent.widget)
    return nsEventStatus_eConsumeNoDefault;

  // A nested process may be capturing events.
  if (TabParent* capturer = TabParent::GetEventCapturer()) {
    if (capturer->TryCapture(aEvent)) {
      // Only touch events should be captured, and touch events from a parent
      // process should not make it here. Capture for those is done elsewhere
      // (for gonk, in nsWindow::DispatchTouchInputViaAPZ).
      MOZ_ASSERT(!XRE_IsParentProcess());

      return nsEventStatus_eConsumeNoDefault;
    }
  }
  nsEventStatus status;
  NS_ENSURE_SUCCESS(aEvent.widget->DispatchEvent(&aEvent, status),
                    nsEventStatus_eConsumeNoDefault);
  return status;
}

nsEventStatus
APZCCallbackHelper::DispatchSynthesizedMouseEvent(uint32_t aMsg,
                                                  uint64_t aTime,
                                                  const LayoutDevicePoint& aRefPoint,
                                                  nsIWidget* aWidget)
{
  MOZ_ASSERT(aMsg == NS_MOUSE_MOVE || aMsg == NS_MOUSE_BUTTON_DOWN ||
             aMsg == NS_MOUSE_BUTTON_UP || aMsg == NS_MOUSE_MOZLONGTAP);

  WidgetMouseEvent event(true, aMsg, nullptr,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  event.refPoint = LayoutDeviceIntPoint(aRefPoint.x, aRefPoint.y);
  event.time = aTime;
  event.button = WidgetMouseEvent::eLeftButton;
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  event.ignoreRootScrollFrame = true;
  if (aMsg != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }
  event.widget = aWidget;

  return DispatchWidgetEvent(event);
}

void
APZCCallbackHelper::FireSingleTapEvent(const LayoutDevicePoint& aPoint,
                                       nsIWidget* aWidget)
{
  if (aWidget->Destroyed()) {
    return;
  }
  APZCCH_LOG("Dispatching single-tap component events to %s\n",
    Stringify(aPoint).c_str());
  int time = 0;
  DispatchSynthesizedMouseEvent(NS_MOUSE_MOVE, time, aPoint, aWidget);
  DispatchSynthesizedMouseEvent(NS_MOUSE_BUTTON_DOWN, time, aPoint, aWidget);
  DispatchSynthesizedMouseEvent(NS_MOUSE_BUTTON_UP, time, aPoint, aWidget);
}

}
}
