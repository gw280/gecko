/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPDecryptorChild_h_
#define GMPDecryptorChild_h_

#include "mozilla/gmp/PGMPDecryptorChild.h"
#include "gmp-decryption.h"
#include "mozilla/gmp/GMPTypes.h"
#include "GMPEncryptedBufferDataImpl.h"
#include <string>

namespace mozilla {
namespace gmp {

class GMPChild;

class GMPDecryptorChild : public GMPDecryptorCallback
                        , public GMPDecryptorHost
                        , public PGMPDecryptorChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPDecryptorChild);

  explicit GMPDecryptorChild(GMPChild* aPlugin,
                             const nsTArray<uint8_t>& aPluginVoucher,
                             const nsTArray<uint8_t>& aSandboxVoucher);

  void Init(GMPDecryptor* aSession);

  // GMPDecryptorCallback
  virtual void SetSessionId(uint32_t aCreateSessionToken,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) MOZ_OVERRIDE;
  virtual void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                         bool aSuccess) MOZ_OVERRIDE;
  virtual void ResolvePromise(uint32_t aPromiseId) MOZ_OVERRIDE;

  virtual void RejectPromise(uint32_t aPromiseId,
                             GMPDOMException aException,
                             const char* aMessage,
                             uint32_t aMessageLength) MOZ_OVERRIDE;

  virtual void SessionMessage(const char* aSessionId,
                              uint32_t aSessionIdLength,
                              GMPSessionMessageType aMessageType,
                              const uint8_t* aMessage,
                              uint32_t aMessageLength) MOZ_OVERRIDE;

  virtual void ExpirationChange(const char* aSessionId,
                                 uint32_t aSessionIdLength,
                                 GMPTimestamp aExpiryTime) MOZ_OVERRIDE;

   virtual void SessionClosed(const char* aSessionId,
                             uint32_t aSessionIdLength) MOZ_OVERRIDE;

  virtual void SessionError(const char* aSessionId,
                            uint32_t aSessionIdLength,
                            GMPDOMException aException,
                            uint32_t aSystemCode,
                            const char* aMessage,
                            uint32_t aMessageLength) MOZ_OVERRIDE;

  virtual void KeyStatusChanged(const char* aSessionId,
                                uint32_t aSessionIdLength,
                                const uint8_t* aKeyId,
                                uint32_t aKeyIdLength,
                                GMPMediaKeyStatus aStatus) MOZ_OVERRIDE;

  virtual void SetCapabilities(uint64_t aCaps) MOZ_OVERRIDE;

  virtual void Decrypted(GMPBuffer* aBuffer, GMPErr aResult) MOZ_OVERRIDE;

  // GMPDecryptorHost
  virtual void GetSandboxVoucher(const uint8_t** aVoucher,
                                 uint32_t* aVoucherLength) MOZ_OVERRIDE;

  virtual void GetPluginVoucher(const uint8_t** aVoucher,
                                uint32_t* aVoucherLength) MOZ_OVERRIDE;
private:
  ~GMPDecryptorChild();

  // GMPDecryptorChild
  virtual bool RecvInit() MOZ_OVERRIDE;

  virtual bool RecvCreateSession(const uint32_t& aCreateSessionToken,
                                 const uint32_t& aPromiseId,
                                 const nsCString& aInitDataType,
                                 InfallibleTArray<uint8_t>&& aInitData,
                                 const GMPSessionType& aSessionType) MOZ_OVERRIDE;

  virtual bool RecvLoadSession(const uint32_t& aPromiseId,
                               const nsCString& aSessionId) MOZ_OVERRIDE;

  virtual bool RecvUpdateSession(const uint32_t& aPromiseId,
                                 const nsCString& aSessionId,
                                 InfallibleTArray<uint8_t>&& aResponse) MOZ_OVERRIDE;

  virtual bool RecvCloseSession(const uint32_t& aPromiseId,
                                const nsCString& aSessionId) MOZ_OVERRIDE;

  virtual bool RecvRemoveSession(const uint32_t& aPromiseId,
                                 const nsCString& aSessionId) MOZ_OVERRIDE;

  virtual bool RecvDecrypt(const uint32_t& aId,
                           InfallibleTArray<uint8_t>&& aBuffer,
                           const GMPDecryptionData& aMetadata) MOZ_OVERRIDE;

  // Resolve/reject promise on completion.
  virtual bool RecvSetServerCertificate(const uint32_t& aPromiseId,
                                        InfallibleTArray<uint8_t>&& aServerCert) MOZ_OVERRIDE;

  virtual bool RecvDecryptingComplete() MOZ_OVERRIDE;

  // GMP's GMPDecryptor implementation.
  // Only call into this on the (GMP process) main thread.
  GMPDecryptor* mSession;
  GMPChild* mPlugin;

  // Reference to the vouchers owned by the GMPChild.
  const nsTArray<uint8_t>& mPluginVoucher;
  const nsTArray<uint8_t>& mSandboxVoucher;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPDecryptorChild_h_
