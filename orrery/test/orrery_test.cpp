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

#include <iostream>

#include "gtest/gtest.h"

#include "orrery/executor.h"
#include "orrery/environment.h"
#include "orrery_msgs.cmf.hpp"

namespace concord::orrery::test {

class StateTransferComponent {
 public:
  ComponentId id{ComponentId::state_transfer};
  void handle(ConsensusMsg&& msg) { std::cout << "Handling consensus msg with id: " << msg.id << std::endl; }
};

class ReplicaComponent {
 public:
  ComponentId id{ComponentId::replica};
  void handle(StateTransferMsg&& msg) { std::cout << "Handling state transfer msg with id: " << msg.id << std::endl; }
};

// This is what "main" will look like.
TEST(orrery_test, init) {
  auto exec1 = Executor();
  auto exec2 = Executor();

  // Assign components to executors/mailboxes
  auto env = Environment();
  env.add(ComponentId::replica, exec1.mailbox());
  env.add(ComponentId::state_transfer, exec2.mailbox());

  // Inform the executors about the environment
  exec1.init(env);
  exec2.init(env);

  // Create polymorphic versions of components and give ownership to the executors
  exec1.add(ComponentId::replica, std::make_unique<Component<ReplicaComponent>>(ReplicaComponent{}));
  exec2.add(ComponentId::state_transfer, std::make_unique<Component<StateTransferComponent>>(StateTransferComponent{}));
}

}  // namespace concord::orrery::test

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
