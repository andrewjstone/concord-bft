
// Concord
//
// Copyright (c) 2019 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the
// LICENSE file.

#ifndef BFTENGINE_TESTS_REPLICA_REPLICA_H
#define BFTENGINE_TESTS_REPLICA_REPLICA_H

#include <memory>
#include "Replica.hpp"
#include "SimpleStateTransfer.hpp"
#include "communication.hpp"
#include "app_state.hpp"

namespace DeterministicTest {

typedef bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer
   IStateTransfer;

// This class contains all information necessary to test a sbft replica.
class Replica {
public:
   Replica(bftEngine::ReplicaConfig config): config_(config) {
      state_transfer_ = std::unique_ptr<IStateTransfer>(
         bftEngine::SimpleInMemoryStateTransfer::create(
            app_state_.GetStatePtr(),
            sizeof(TestState),
            config.replicaId,
            config.fVal,
            config.cVal,
            true
         )
      );

      app_state_.SetStateTransfer(state_transfer_.get());
      replica_ = std::unique_ptr<bftEngine::Replica>(
         bftEngine::Replica::createNewReplica(
            &config_,
            &app_state_,
            state_transfer_.get(),
            &communication_,
            nullptr
         )
      );
   }

private:
   bftEngine::ReplicaConfig config_;
   TestCommunication  communication_;
   TestAppState app_state_;
   std::unique_ptr<IStateTransfer> state_transfer_;
   std::unique_ptr<bftEngine::Replica> replica_;
};

} // namespace DeterministicTest

#endif // BFTENGINE_TESTS_REPLICA_REPLICA_H

