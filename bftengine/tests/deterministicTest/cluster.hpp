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

#ifndef BFTENGINE_TESTS_REPLICA_CLUSTER_H
#define BFTENGINE_TESTS_REPLICA_CLUSTER_H

#include "Replica.hpp"
#include "communication.hpp"
#include "replica.hpp"

namespace DeterministicTest {

// A configuration for an entire test cluster.
// ReplicaConfigs are created based on TestClusterConfig.
struct TestClusterConfig {
   uint16_t num_replicas;
   uint16_t f_val;
   uint16_t c_val;
   uint16_t concurrency_level;
};

// A TestCluster represents a collection of replicas, and their communication
// links implemented via TestCommunication.
class TestCluster {
   public:
      explicit TestCluster(TestClusterConfig config): cluster_config_{config} {

         for (uint16_t i = 0; i < config.num_replicas; i++) {
            replicas_.push_back(Replica(CreateReplicaConfig(config, i)));
         }
      }

      TestClusterConfig GetConfig() {
         return cluster_config_;
      }

   private:
      static bftEngine::ReplicaConfig CreateReplicaConfig(
            TestClusterConfig config, uint16_t id);

      TestClusterConfig cluster_config_;
      std::vector<Replica> replicas_;
};

} // namespace DeterministicTest

#endif // BFTENGINE_TESTS_REPLICA_CLUSTER_H
