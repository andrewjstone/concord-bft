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

#include "environment.h"
#include "orrery_msgs.cmf.hpp"

namespace concord::orrery {

// A World is an abstraction that allows an orrery component to interact with other orrery
// components in the same trust domain.
class World {
 public:
  World(Environment env) : env_(env) {}
  // Send a message to a component or all components.
  //
  // This is the common case for normal protocol behavior.
  //
  // If `envelope.to == broadcast` then the message is broadcast.
  // TODO (AJS): Should we assert instead?
  void send(Envelope envelope);

  // Send a message to all components
  //
  // This is mostly useful for things like overload alarms, status, metrics, or shutdown events.
  // It's an explicit form to help readers of the code see that a message is a broadcast.
  void broadcast(Envelope envelope);

 private:
  Environment env_;
};

};  // namespace concord::orrery
