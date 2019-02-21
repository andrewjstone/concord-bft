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

#include "cluster.hpp"

using namespace std;

namespace DeterministicTest {

bftEngine::ReplicaConfig TestCluster::CreateReplicaConfig(
      TestClusterConfig config, uint16_t id) {

   bftEngine::ReplicaConfig replica_config;
   replica_config.fVal = config.f_val;
   replica_config.cVal = config.c_val;
   replica_config.replicaId = id;
   replica_config.numOfClientProxies = 1;
   replica_config.concurrencyLevel = config.concurrency_level;
   return replica_config;
}

} // namespace DeterministicTest
