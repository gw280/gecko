/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileDescriptorSetChild_h__
#define mozilla_dom_FileDescriptorSetChild_h__

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/PFileDescriptorSetChild.h"
#include "nsTArray.h"

namespace mozilla {

namespace ipc {

class FileDescriptor;

} // namespace ipc

namespace dom {

class ContentChild;

class FileDescriptorSetChild MOZ_FINAL: public PFileDescriptorSetChild
{
  friend class ContentChild;

public:
  typedef mozilla::ipc::FileDescriptor FileDescriptor;

  void
  ForgetFileDescriptors(nsTArray<FileDescriptor>& aFileDescriptors)
  {
    aFileDescriptors.Clear();
    mFileDescriptors.SwapElements(aFileDescriptors);
  }

private:
  FileDescriptorSetChild(const FileDescriptor& aFileDescriptor)
  {
    mFileDescriptors.AppendElement(aFileDescriptor);
  }

  ~FileDescriptorSetChild()
  {
    MOZ_ASSERT(mFileDescriptors.IsEmpty());
  }

  virtual bool
  RecvAddFileDescriptor(const FileDescriptor& aFileDescriptor) MOZ_OVERRIDE
  {
    mFileDescriptors.AppendElement(aFileDescriptor);
    return true;
  }

  nsAutoTArray<FileDescriptor, 1> mFileDescriptors;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileDescriptorSetChild_h__
