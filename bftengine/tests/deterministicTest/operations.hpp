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

#ifndef BFTENGINE_TESTS_REPLICA_OPERATIONS_H
#define BFTENGINE_TESTS_REPLICA_OPERATIONS_H

namespace DeterministicTest {

// Op represents every operation that can be issued to a deterministic test. It
// includes not only client operations, but also things like timeouts, message
// drops, disconnections, etc...
//
// We use a tagged union since std::variant is only in C++17. We aren't really
// concenred about type safety since our tests can ensure they only create valid
// Ops.
struct Op
{
   enum {
     CLIENT_WRITE,
     CLIENT_READ,
     VIEW_CHANGE_TIMEOUT,
   } type;

   union {
      ClientWrite client_write;
      ClientRead client_read;
      Timeout timeout;
   }
}

} // namespace DeterministicTest

#endif // BFTENGINE_TESTS_REPLICA_OPERATIONS_H


