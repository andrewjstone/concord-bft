// Concord
//
// Copyright (c) 2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the
// "License").  You may not use this product except in compliance with the
// Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include "component.h"
#include "environment.h"
#include "mailbox.h"
#include "queue.h"

namespace concord::orrery {

// An executor runs component handler callbacks. It is the actively running thread of control.
//
// An executor is always a single thread, although it can proxy work for a thread pool,
// external process, or even remote compute cluster as necessary.
//
// An executor owns a mailbox and its underlying queue. It is driven forward by pulling messages off
// the queue after they are placed in the mailbox.
class Executor {
 public:
  Executor() : mailbox_(queue_) {}
  Mailbox& mailbox() { return mailbox_; }
  void init(const Environment& environment) { environment_ = environment; }
  void add(ComponentId id, std::unique_ptr<IComponent>&& component) {
    components_[static_cast<uint8_t>(id)] = std::move(component);
  }

 private:
  std::shared_ptr<detail::Queue> queue_;
  Mailbox mailbox_;
  Environment environment_;
  std::vector<std::unique_ptr<IComponent>> components_;
};

}  // namespace concord::orrery
