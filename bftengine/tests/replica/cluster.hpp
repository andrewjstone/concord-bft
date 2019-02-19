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

#include "Replica.hpp"
#include "TestCommunication.hpp"
#include "Replica.hpp"

struct TestClusterConfig {
   int num_replicas;
}

// A TestCluster represents a collection of replicas, and their communication
// links implemented via TestCommunication.
class TestCluster {
public:
   TestCluster(TestClusterConfig config): cluster_config_{config} {
      replica_configs_ = CreateReplicaConfigs(config);
   }

   GetConfig() {
      return cluster_config_;
   }

private:
   static std::set<ReplicaConfig> CreateReplicaConfigs(TestClusterConfig config);

private:
      TestClusterConfig cluster_config_;
      std::set<ReplicaConfig> replica_configs_;
}
