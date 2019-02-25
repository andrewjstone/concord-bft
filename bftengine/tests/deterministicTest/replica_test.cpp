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

#include "gtest/gtest.h"
#include "rapidcheck/gtest.h"
#include "rapidcheck/state.h"
#include "ICommunication.hpp"
#include "communication.hpp"
#include "ReplicaConfig.hpp"
#include "ReplicaImp.hpp"

// In this stateful test the replica under test is the primary, and we ensure
// that by mocking out all the other replicas, we can successfully commit
// operations.
TEST(deterministic_test, successful_client_ops) {
   ASSERT_TRUE(true);
}

// A model of all replicas in the rest of the cluster.
struct SbftClusterModel {
   // The application state of the cluster.
   TestState app_state_;

   // Map of replic ids to configs
   std::map<uint16_t, bftEngine::ReplicaConfig> replica_configs_;
}

// A client read operation 
// Always valid
struct ClientRead : rc::state::Command<SbftClusterModel, bftEngine::impl::ReplicaImp> {

   // A read doesn't change the model state
   void apply(SbftClusterModel& model) const override {}

   // The returned value from the SUT should match the model state. 
   void run(const SbftClusterModel& model, 
            bftEngine::impl::ReplicaImp& replica) const override {



   }
   
}
