/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef PAGESPEED_KERNEL_BASE_ATOMIC_INT32_H_
#define PAGESPEED_KERNEL_BASE_ATOMIC_INT32_H_

#include <atomic>

#include "pagespeed/kernel/base/atomicops.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

// An int32 flag that can be set atomically and be visible to other threads.
// Please be extra careful with this --- it can go wrong in incomprehensible
// ways; most of the time, if you care about how the value of this flag relates
// to the value of other memory locations, you probably want to use a mutex
// instead.
//
// In more detail: When communicating multiple values between threads, we need
// to rely on operations with acquire and release semantics.  An example is
// something like this (first without atomic_int32):
//   Writer thread:
//     x_ = 5;
//     x_ = 17;
//     y_ = 3;
//   Reader thread:
//     y = y_;
//     x = x_;
//
// Here if the reader sees y=3, then it can still see either of x=17 OR x=5;
// either the writes to x_ and y_ or the reads of x_ and y_ can be reordered on
// some cpu architectures.  Using atomic_int32 lets us protect against this:
//
//   Writer thread:
//     x_ = 5;
//     x_ = 17;
//     atomic_int_.set_value(3);  // release
//   Reader thread:
//     y = atomic_int_.value()  // acquire
//     x = x_;
//
// Now if the reader sees y=3, x=17 and never 5.  The release insures that
// set_value(3) happens after the stores to x_, and the acquire ensures that
// value() happens before the read of x_.
//
// The important thing here is that without the acquire and release semantics
// (if atomic_int_ was an ordinary int variable, even a volatile one) loads and
// stores need not obey program order.  Release semantics ensure that *prior
// writes* (according to program order) occur before the release operation.
// Acquire semantics ensure that *subsequent reads* (according to program order)
// occur after the acquire operation.  If you don't have both guarantees, you
// must not assume anything about ordering constraints.
//
// Note that Acquire and Release talk about how these operations relate to
// operations on *other* memory locations.  All the operations on the
// AtomicInt32 behave as you would probably expect, though several of them
// (increment, CompareAndSwap) occur as atomic actions.

class AtomicInt32 {
 public:
  explicit AtomicInt32(int32 value) { set_value(value); }
  AtomicInt32() { set_value(0); }
  ~AtomicInt32() {}

  // Return the value currently stored.  Has acquire semantics (see above).
  int32 value() const {
    return value_.load(std::memory_order::memory_order_acquire);
  }

  // Store value.  Has release semantics (see above).
  void set_value(int32 value) {
    value_.store(value, std::memory_order_release);
  }

  // Atomically add an amount to the value currently stored, return the new
  // value. Has *no ordering semantics* with respect to operations on other
  // memory locations.
  int32 NoBarrierIncrement(int32 amount) {
    return amount + value_.fetch_add(amount, std::memory_order_relaxed);
  }

  // Atomically add an amount to the value stored, return the new value.
  // Provides a full barrier --- both acquire and release.
  int32 BarrierIncrement(int32 amount) {
    return amount + value_.fetch_add(amount);
  }

  // Atomic compare and swap.  If current value == expected_value, atomically
  // replace it with new_value.  Return the original value regardless of whether
  // the swap occurred.  Has release semantics as with set_value() (see above).
  //
  // NOTE: does NOT have acquire semantics, so the value returned may not appear
  // to be ordered with respect to subsequent reads of other memory locations --
  // nor can we expect to see changes to other locations made by prior writers
  // based on the read performed by CompareAndSwap.  If you need acquire
  // semantics, use the value() method and validate its result when you
  // CompareAndSwap.
  int32 CompareAndSwap(int32 expected_value, int32 new_value) {
    value_.compare_exchange_strong(expected_value, new_value,
                                   std::memory_order_release,
                                   std::memory_order_relaxed);
    return expected_value;
  }

 private:
  std::atomic<int32> value_;
  DISALLOW_COPY_AND_ASSIGN(AtomicInt32);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_ATOMIC_INT32_H_
