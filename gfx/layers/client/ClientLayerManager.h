/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTLAYERMANAGER_H
#define GFX_CLIENTLAYERMANAGER_H

#include <stdint.h>                     // for int32_t
#include "Layers.h"
#include "gfxContext.h"                 // for gfxContext
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/LinkedList.h"         // For LinkedList
#include "mozilla/WidgetUtils.h"        // for ScreenRotation
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"  // for BufferMode, LayersBackend, etc
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder, etc
#include "mozilla/layers/APZTestData.h" // for APZTestData
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ABORT_IF_FALSE
#include "nsIObserver.h"                // for nsIObserver
#include "nsISupportsImpl.h"            // for Layer::Release, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsTArray.h"                   // for nsTArray
#include "nscore.h"                     // for nsAString
#include "mozilla/layers/TransactionIdAllocator.h"
#include "nsIWidget.h"                  // For plugin window configuration information structs

namespace mozilla {
namespace layers {

class ClientPaintedLayer;
class CompositorChild;
class ImageLayer;
class PLayerChild;
class TextureClientPool;

class ClientLayerManager MOZ_FINAL : public LayerManager
{
  typedef nsTArray<nsRefPtr<Layer> > LayerRefArray;

public:
  explicit ClientLayerManager(nsIWidget* aWidget);

  virtual void Destroy() MOZ_OVERRIDE
  {
    LayerManager::Destroy();
    ClearCachedResources();
  }

protected:
  virtual ~ClientLayerManager();

public:
  virtual ShadowLayerForwarder* AsShadowForwarder() MOZ_OVERRIDE
  {
    return mForwarder;
  }

  virtual ClientLayerManager* AsClientLayerManager() MOZ_OVERRIDE
  {
    return this;
  }

  virtual int32_t GetMaxTextureSize() const MOZ_OVERRIDE;

  virtual void SetDefaultTargetConfiguration(BufferMode aDoubleBuffering, ScreenRotation aRotation);
  virtual void BeginTransactionWithTarget(gfxContext* aTarget) MOZ_OVERRIDE;
  virtual void BeginTransaction() MOZ_OVERRIDE;
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) MOZ_OVERRIDE;
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) MOZ_OVERRIDE;

  virtual LayersBackend GetBackendType() MOZ_OVERRIDE { return LayersBackend::LAYERS_CLIENT; }
  virtual LayersBackend GetCompositorBackendType() MOZ_OVERRIDE
  {
    return AsShadowForwarder()->GetCompositorBackendType();
  }
  virtual void GetBackendName(nsAString& name) MOZ_OVERRIDE;
  virtual const char* Name() const MOZ_OVERRIDE { return "Client"; }

  virtual void SetRoot(Layer* aLayer) MOZ_OVERRIDE;

  virtual void Mutated(Layer* aLayer) MOZ_OVERRIDE;

  virtual bool IsOptimizedFor(PaintedLayer* aLayer, PaintedLayerCreationHint aHint) MOZ_OVERRIDE;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<PaintedLayer> CreatePaintedLayerWithHint(PaintedLayerCreationHint aHint) MOZ_OVERRIDE;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<RefLayer> CreateRefLayer() MOZ_OVERRIDE;

  TextureFactoryIdentifier GetTextureFactoryIdentifier()
  {
    return mForwarder->GetTextureFactoryIdentifier();
  }

  virtual void FlushRendering() MOZ_OVERRIDE;
  void SendInvalidRegion(const nsIntRegion& aRegion);

  virtual uint32_t StartFrameTimeRecording(int32_t aBufferSize) MOZ_OVERRIDE;

  virtual void StopFrameTimeRecording(uint32_t         aStartIndex,
                                      nsTArray<float>& aFrameIntervals) MOZ_OVERRIDE;

  virtual bool NeedsWidgetInvalidation() MOZ_OVERRIDE { return false; }

  ShadowableLayer* Hold(Layer* aLayer);

  bool HasShadowManager() const { return mForwarder->HasShadowManager(); }

  virtual bool IsCompositingCheap() MOZ_OVERRIDE;
  virtual bool HasShadowManagerInternal() const MOZ_OVERRIDE { return HasShadowManager(); }

  virtual void SetIsFirstPaint() MOZ_OVERRIDE;

  TextureClientPool* GetTexturePool(gfx::SurfaceFormat aFormat);

  /// Utility methods for managing texture clients.
  void ReturnTextureClientDeferred(TextureClient& aClient);
  void ReturnTextureClient(TextureClient& aClient);
  void ReportClientLost(TextureClient& aClient);

  /**
   * Pass through call to the forwarder for nsPresContext's
   * CollectPluginGeometryUpdates. Passes widget configuration information
   * to the compositor for transmission to the chrome process. This
   * configuration gets set when the window paints.
   */
  void StorePluginWidgetConfigurations(const nsTArray<nsIWidget::Configuration>&
                                       aConfigurations) MOZ_OVERRIDE;

  // Drop cached resources and ask our shadow manager to do the same,
  // if we have one.
  virtual void ClearCachedResources(Layer* aSubtree = nullptr) MOZ_OVERRIDE;

  void HandleMemoryPressure();

  void SetRepeatTransaction() { mRepeatTransaction = true; }
  bool GetRepeatTransaction() { return mRepeatTransaction; }

  bool IsRepeatTransaction() { return mIsRepeatTransaction; }

  void SetTransactionIncomplete() { mTransactionIncomplete = true; }

  bool HasShadowTarget() { return !!mShadowTarget; }

  void SetShadowTarget(gfxContext* aTarget) { mShadowTarget = aTarget; }

  bool CompositorMightResample() { return mCompositorMightResample; } 
  
  DrawPaintedLayerCallback GetPaintedLayerCallback() const
  { return mPaintedLayerCallback; }

  void* GetPaintedLayerCallbackData() const
  { return mPaintedLayerCallbackData; }

  CompositorChild* GetRemoteRenderer();

  CompositorChild* GetCompositorChild();

  // Disable component alpha layers with the software compositor.
  virtual bool ShouldAvoidComponentAlphaLayers() MOZ_OVERRIDE { return !IsCompositingCheap(); }

  /**
   * Called for each iteration of a progressive tile update. Updates
   * aMetrics with the current scroll offset and scale being used to composite
   * the primary scrollable layer in this manager, to determine what area
   * intersects with the target composition bounds.
   * aDrawingCritical will be true if the current drawing operation is using
   * the critical displayport.
   * Returns true if the update should continue, or false if it should be
   * cancelled.
   * This is only called if gfxPlatform::UseProgressiveTilePainting() returns
   * true.
   */
  bool ProgressiveUpdateCallback(bool aHasPendingNewThebesContent,
                                 FrameMetrics& aMetrics,
                                 bool aDrawingCritical);

  bool InConstruction() { return mPhase == PHASE_CONSTRUCTION; }
#ifdef DEBUG
  bool InDrawing() { return mPhase == PHASE_DRAWING; }
  bool InForward() { return mPhase == PHASE_FORWARD; }
#endif
  bool InTransaction() { return mPhase != PHASE_NONE; }

  void SetNeedsComposite(bool aNeedsComposite)
  {
    mNeedsComposite = aNeedsComposite;
  }
  bool NeedsComposite() const { return mNeedsComposite; }

  virtual void Composite() MOZ_OVERRIDE;
  virtual bool RequestOverfill(mozilla::dom::OverfillCallback* aCallback) MOZ_OVERRIDE;
  virtual void RunOverfillCallback(const uint32_t aOverfill) MOZ_OVERRIDE;

  virtual void DidComposite(uint64_t aTransactionId);

  virtual bool SupportsMixBlendModes(EnumSet<gfx::CompositionOp>& aMixBlendModes) MOZ_OVERRIDE
  {
   return (GetTextureFactoryIdentifier().mSupportedBlendModes & aMixBlendModes) == aMixBlendModes;
  }

  virtual bool AreComponentAlphaLayersEnabled() MOZ_OVERRIDE;

  // Log APZ test data for the current paint. We supply the paint sequence
  // number ourselves, and take care of calling APZTestData::StartNewPaint()
  // when a new paint is started.
  void LogTestDataForCurrentPaint(FrameMetrics::ViewID aScrollId,
                                  const std::string& aKey,
                                  const std::string& aValue)
  {
    mApzTestData.LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey, aValue);
  }

  // Log APZ test data for a repaint request. The sequence number must be
  // passed in from outside, and APZTestData::StartNewRepaintRequest() needs
  // to be called from the outside as well when a new repaint request is started.
  void StartNewRepaintRequest(SequenceNumber aSequenceNumber);

  // TODO(botond): When we start using this and write a wrapper similar to
  // nsLayoutUtils::LogTestDataForPaint(), make sure that wrapper checks
  // gfxPrefs::APZTestLoggingEnabled().
  void LogTestDataForRepaintRequest(SequenceNumber aSequenceNumber,
                                    FrameMetrics::ViewID aScrollId,
                                    const std::string& aKey,
                                    const std::string& aValue)
  {
    mApzTestData.LogTestDataForRepaintRequest(aSequenceNumber, aScrollId, aKey, aValue);
  }

  // Get the content-side APZ test data for reading. For writing, use the
  // LogTestData...() functions.
  const APZTestData& GetAPZTestData() const {
    return mApzTestData;
  }

  // Get a copy of the compositor-side APZ test data for our layers ID.
  void GetCompositorSideAPZTestData(APZTestData* aData) const;

  void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) { mTransactionIdAllocator = aAllocator; }

  float RequestProperty(const nsAString& aProperty) MOZ_OVERRIDE;
protected:
  enum TransactionPhase {
    PHASE_NONE, PHASE_CONSTRUCTION, PHASE_DRAWING, PHASE_FORWARD
  };
  TransactionPhase mPhase;

private:
  // Listen memory-pressure event for ClientLayerManager
  class MemoryPressureObserver MOZ_FINAL : public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    explicit MemoryPressureObserver(ClientLayerManager* aClientLayerManager)
      : mClientLayerManager(aClientLayerManager)
    {
      RegisterMemoryPressureEvent();
    }

    void Destroy();

  private:
    virtual ~MemoryPressureObserver() {}
    void RegisterMemoryPressureEvent();
    void UnregisterMemoryPressureEvent();

    ClientLayerManager* mClientLayerManager;
  };

  /**
   * Forward transaction results to the parent context.
   */
  void ForwardTransaction(bool aScheduleComposite);

  /**
   * Take a snapshot of the parent context, and copy
   * it into mShadowTarget.
   */
  void MakeSnapshotIfRequired();

  void ClearLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags);

  LayerRefArray mKeepAlive;

  nsIWidget* mWidget;
  
  /* PaintedLayer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawPaintedLayerCallback mPaintedLayerCallback;
  void *mPaintedLayerCallbackData;

  // When we're doing a transaction in order to draw to a non-default
  // target, the layers transaction is only performed in order to send
  // a PLayers:Update.  We save the original non-default target to
  // mShadowTarget, and then perform the transaction using
  // mDummyTarget as the render target.  After the transaction ends,
  // we send a message to our remote side to capture the actual pixels
  // being drawn to the default target, and then copy those pixels
  // back to mShadowTarget.
  nsRefPtr<gfxContext> mShadowTarget;

  nsRefPtr<TransactionIdAllocator> mTransactionIdAllocator;
  uint64_t mLatestTransactionId;

  // Sometimes we draw to targets that don't natively support
  // landscape/portrait orientation.  When we need to implement that
  // ourselves, |mTargetRotation| describes the induced transform we
  // need to apply when compositing content to our target.
  ScreenRotation mTargetRotation;

  // Used to repeat the transaction right away (to avoid rebuilding
  // a display list) to support progressive drawing.
  bool mRepeatTransaction;
  bool mIsRepeatTransaction;
  bool mTransactionIncomplete;
  bool mCompositorMightResample;
  bool mNeedsComposite;

  // An incrementing sequence number for paints.
  // Incremented in BeginTransaction(), but not for repeat transactions.
  uint32_t mPaintSequenceNumber;

  APZTestData mApzTestData;

  RefPtr<ShadowLayerForwarder> mForwarder;
  nsAutoTArray<RefPtr<TextureClientPool>,2> mTexturePools;
  nsAutoTArray<dom::OverfillCallback*,0> mOverfillCallbacks;
  mozilla::TimeStamp mTransactionStart;

  nsRefPtr<MemoryPressureObserver> mMemoryPressureObserver;
};

class ClientLayer : public ShadowableLayer
{
public:
  ClientLayer()
  {
    MOZ_COUNT_CTOR(ClientLayer);
  }

  ~ClientLayer();

  void SetShadow(PLayerChild* aShadow)
  {
    NS_ABORT_IF_FALSE(!mShadow, "can't have two shadows (yet)");
    mShadow = aShadow;
  }

  virtual void Disconnect()
  {
    // This is an "emergency Disconnect()", called when the compositing
    // process has died.  |mShadow| and our Shmem buffers are
    // automatically managed by IPDL, so we don't need to explicitly
    // free them here (it's hard to get that right on emergency
    // shutdown anyway).
    mShadow = nullptr;
  }

  virtual void ClearCachedResources() { }

  virtual void RenderLayer() = 0;
  virtual void RenderLayerWithReadback(ReadbackProcessor *aReadback) { RenderLayer(); }

  virtual ClientPaintedLayer* AsThebes() { return nullptr; }

  static inline ClientLayer *
  ToClientLayer(Layer* aLayer)
  {
    return static_cast<ClientLayer*>(aLayer->ImplData());
  }
};

// Create a shadow layer (PLayerChild) for aLayer, if we're forwarding
// our layer tree to a parent process.  Record the new layer creation
// in the current open transaction as a side effect.
template<typename CreatedMethod> void
CreateShadowFor(ClientLayer* aLayer,
                ClientLayerManager* aMgr,
                CreatedMethod aMethod)
{
  PLayerChild* shadow = aMgr->AsShadowForwarder()->ConstructShadowFor(aLayer);
  // XXX error handling
  NS_ABORT_IF_FALSE(shadow, "failed to create shadow");

  aLayer->SetShadow(shadow);
  (aMgr->AsShadowForwarder()->*aMethod)(aLayer);
  aMgr->Hold(aLayer->AsLayer());
}

#define CREATE_SHADOW(_type)                                       \
  CreateShadowFor(layer, this,                                     \
                  &ShadowLayerForwarder::Created ## _type ## Layer)


}
}

#endif /* GFX_CLIENTLAYERMANAGER_H */
