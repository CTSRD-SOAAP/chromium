// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DATA_PIPE_H_
#define MOJO_EDK_SYSTEM_DATA_PIPE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/memory.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace system {

class Awakable;
class AwakableList;

// |DataPipe| is a base class for secondary objects implementing data pipes,
// similar to |MessagePipe| (see the explanatory comment in core.cc). It is
// typically owned by the dispatcher(s) corresponding to the local endpoints.
// Its subclasses implement the three cases: local producer and consumer, local
// producer and remote consumer, and remote producer and local consumer. This
// class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipe
    : public base::RefCountedThreadSafe<DataPipe> {
 public:
  // The default options for |MojoCreateDataPipe()|. (Real uses should obtain
  // this via |ValidateCreateOptions()| with a null |in_options|; this is
  // exposed directly for testing convenience.)
  static MojoCreateDataPipeOptions GetDefaultCreateOptions();

  // Validates and/or sets default options for |MojoCreateDataPipeOptions|. If
  // non-null, |in_options| must point to a struct of at least
  // |in_options->struct_size| bytes. |out_options| must point to a (current)
  // |MojoCreateDataPipeOptions| and will be entirely overwritten on success (it
  // may be partly overwritten on failure).
  static MojoResult ValidateCreateOptions(
      UserPointer<const MojoCreateDataPipeOptions> in_options,
      MojoCreateDataPipeOptions* out_options);

  // These are called by the producer dispatcher to implement its methods of
  // corresponding names.
  void ProducerCancelAllAwakables();
  void ProducerClose();
  MojoResult ProducerWriteData(UserPointer<const void> elements,
                               UserPointer<uint32_t> num_bytes,
                               bool all_or_none);
  MojoResult ProducerBeginWriteData(UserPointer<void*> buffer,
                                    UserPointer<uint32_t> buffer_num_bytes,
                                    bool all_or_none);
  MojoResult ProducerEndWriteData(uint32_t num_bytes_written);
  HandleSignalsState ProducerGetHandleSignalsState();
  MojoResult ProducerAddAwakable(Awakable* awakable,
                                 MojoHandleSignals signals,
                                 uint32_t context,
                                 HandleSignalsState* signals_state);
  void ProducerRemoveAwakable(Awakable* awakable,
                              HandleSignalsState* signals_state);
  bool ProducerIsBusy() const;

  // These are called by the consumer dispatcher to implement its methods of
  // corresponding names.
  void ConsumerCancelAllAwakables();
  void ConsumerClose();
  // This does not validate its arguments, except to check that |*num_bytes| is
  // a multiple of |element_num_bytes_|.
  MojoResult ConsumerReadData(UserPointer<void> elements,
                              UserPointer<uint32_t> num_bytes,
                              bool all_or_none,
                              bool peek);
  MojoResult ConsumerDiscardData(UserPointer<uint32_t> num_bytes,
                                 bool all_or_none);
  MojoResult ConsumerQueryData(UserPointer<uint32_t> num_bytes);
  MojoResult ConsumerBeginReadData(UserPointer<const void*> buffer,
                                   UserPointer<uint32_t> buffer_num_bytes,
                                   bool all_or_none);
  MojoResult ConsumerEndReadData(uint32_t num_bytes_read);
  HandleSignalsState ConsumerGetHandleSignalsState();
  MojoResult ConsumerAddAwakable(Awakable* awakable,
                                 MojoHandleSignals signals,
                                 uint32_t context,
                                 HandleSignalsState* signals_state);
  void ConsumerRemoveAwakable(Awakable* awakable,
                              HandleSignalsState* signals_state);
  bool ConsumerIsBusy() const;

 protected:
  DataPipe(bool has_local_producer,
           bool has_local_consumer,
           const MojoCreateDataPipeOptions& validated_options);

  friend class base::RefCountedThreadSafe<DataPipe>;
  virtual ~DataPipe();

  virtual void ProducerCloseImplNoLock() = 0;
  // |num_bytes.Get()| will be a nonzero multiple of |element_num_bytes_|.
  virtual MojoResult ProducerWriteDataImplNoLock(
      UserPointer<const void> elements,
      UserPointer<uint32_t> num_bytes,
      uint32_t max_num_bytes_to_write,
      uint32_t min_num_bytes_to_write) = 0;
  virtual MojoResult ProducerBeginWriteDataImplNoLock(
      UserPointer<void*> buffer,
      UserPointer<uint32_t> buffer_num_bytes,
      uint32_t min_num_bytes_to_write) = 0;
  virtual MojoResult ProducerEndWriteDataImplNoLock(
      uint32_t num_bytes_written) = 0;
  // Note: A producer should not be writable during a two-phase write.
  virtual HandleSignalsState ProducerGetHandleSignalsStateImplNoLock()
      const = 0;

  virtual void ConsumerCloseImplNoLock() = 0;
  // |*num_bytes| will be a nonzero multiple of |element_num_bytes_|.
  virtual MojoResult ConsumerReadDataImplNoLock(UserPointer<void> elements,
                                                UserPointer<uint32_t> num_bytes,
                                                uint32_t max_num_bytes_to_read,
                                                uint32_t min_num_bytes_to_read,
                                                bool peek) = 0;
  virtual MojoResult ConsumerDiscardDataImplNoLock(
      UserPointer<uint32_t> num_bytes,
      uint32_t max_num_bytes_to_discard,
      uint32_t min_num_bytes_to_discard) = 0;
  // |*num_bytes| will be a nonzero multiple of |element_num_bytes_|.
  virtual MojoResult ConsumerQueryDataImplNoLock(
      UserPointer<uint32_t> num_bytes) = 0;
  virtual MojoResult ConsumerBeginReadDataImplNoLock(
      UserPointer<const void*> buffer,
      UserPointer<uint32_t> buffer_num_bytes,
      uint32_t min_num_bytes_to_read) = 0;
  virtual MojoResult ConsumerEndReadDataImplNoLock(uint32_t num_bytes_read) = 0;
  // Note: A consumer should not be writable during a two-phase read.
  virtual HandleSignalsState ConsumerGetHandleSignalsStateImplNoLock()
      const = 0;

  // Thread-safe and fast (they don't take the lock):
  bool may_discard() const { return may_discard_; }
  size_t element_num_bytes() const { return element_num_bytes_; }
  size_t capacity_num_bytes() const { return capacity_num_bytes_; }

  // Must be called under lock.
  bool producer_open_no_lock() const {
    lock_.AssertAcquired();
    return producer_open_;
  }
  bool consumer_open_no_lock() const {
    lock_.AssertAcquired();
    return consumer_open_;
  }
  uint32_t producer_two_phase_max_num_bytes_written_no_lock() const {
    lock_.AssertAcquired();
    return producer_two_phase_max_num_bytes_written_;
  }
  uint32_t consumer_two_phase_max_num_bytes_read_no_lock() const {
    lock_.AssertAcquired();
    return consumer_two_phase_max_num_bytes_read_;
  }
  void set_producer_two_phase_max_num_bytes_written_no_lock(
      uint32_t num_bytes) {
    lock_.AssertAcquired();
    producer_two_phase_max_num_bytes_written_ = num_bytes;
  }
  void set_consumer_two_phase_max_num_bytes_read_no_lock(uint32_t num_bytes) {
    lock_.AssertAcquired();
    consumer_two_phase_max_num_bytes_read_ = num_bytes;
  }
  bool producer_in_two_phase_write_no_lock() const {
    lock_.AssertAcquired();
    return producer_two_phase_max_num_bytes_written_ > 0;
  }
  bool consumer_in_two_phase_read_no_lock() const {
    lock_.AssertAcquired();
    return consumer_two_phase_max_num_bytes_read_ > 0;
  }

 private:
  void AwakeProducerAwakablesForStateChangeNoLock(
      const HandleSignalsState& new_producer_state);
  void AwakeConsumerAwakablesForStateChangeNoLock(
      const HandleSignalsState& new_consumer_state);

  bool has_local_producer_no_lock() const {
    lock_.AssertAcquired();
    return !!producer_awakable_list_;
  }
  bool has_local_consumer_no_lock() const {
    lock_.AssertAcquired();
    return !!consumer_awakable_list_;
  }

  const bool may_discard_;
  const size_t element_num_bytes_;
  const size_t capacity_num_bytes_;

  mutable base::Lock lock_;  // Protects the following members.
  // *Known* state of producer or consumer.
  bool producer_open_;
  bool consumer_open_;
  // Non-null only if the producer or consumer, respectively, is local.
  scoped_ptr<AwakableList> producer_awakable_list_;
  scoped_ptr<AwakableList> consumer_awakable_list_;
  // These are nonzero if and only if a two-phase write/read is in progress.
  uint32_t producer_two_phase_max_num_bytes_written_;
  uint32_t consumer_two_phase_max_num_bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(DataPipe);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DATA_PIPE_H_
