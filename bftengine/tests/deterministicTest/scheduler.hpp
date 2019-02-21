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

#ifndef BFTENGINE_TESTS_REPLICA_SCHEDULER_H
#define BFTENGINE_TESTS_REPLICA_SCHEDULER_H

namespace DeterministicTest {

// The scheduler operates in a single thread. It determines the order
// messages and timer ticks are delivered to all replicas, and the order faults
// are introduced.
class Scheduler {
   public:
      explicit Scheduler(TestCluster cluster): cluster_(cluster){}

      bool Run(std::vector<Op>)
}

#endif // BFTENGINE_TESTS_REPLICA_SCHEDULER_H
