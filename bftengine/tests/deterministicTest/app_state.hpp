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

#ifndef BFTENGINE_TESTS_REPLICA_APP_STATE_H
#define BFTENGINE_TESTS_REPLICA_APP_STATE_H

#include "Replica.hpp"
#include "SimpleStateTransfer.hpp"

namespace DeterministicTest {

typedef bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer
   IStateTransfer;

// Types of operations on the register. This is in the first 8 bytes of a
// request.
static const uint64_t kRead = 1;
static const uint64_t kWrite = 2;
static constexpr uint32_t kValSize = sizeof(uint64_t);

// We must collocate our data for the state transfer module
struct TestState {
      uint64_t val;
      uint64_t write_count;
};

// The application state machine for a replica.
// This state machine is a register with a single uint64_t value.
class TestAppState : public bftEngine::RequestsHandler {
   public:
      // Handler for the upcall from Concord-BFT.
      // Implements bftEngine::RequestHandler
      int execute(uint16_t client_id,
                  bool read_only,
                  uint32_t request_size,
                  const char* request,
                  uint32_t max_reply_size,
                  char* out_reply,
                  uint32_t& out_reply_size) override;

      // Return the internal state for use by the state transfer module.
      void* GetStatePtr() {
         return reinterpret_cast<void*>(&state_);
      }

      void SetStateTransfer(IStateTransfer* st) {
         state_transfer_ = st;
      }

   private:
      // Return true if a request is a Read, false if a Write.
      bool IsRead(const char* request) {
         return *reinterpret_cast<const uint64_t*>(request) == kRead;
      }

      // Return the value serialized as a char*.
      void GetVal(char* reply) {
         uint64_t* pRet = reinterpret_cast<uint64_t*>(reply);
         *pRet = state_.val;
      }

      // Write a value to the register and return a serialized counter
      // indicating the number of times the register was written.
      void WriteVal(const char* request, char* reply) {
         // The value to write is the second eight bytes of the request.
         const uint64_t* pVal = reinterpret_cast<const uint64_t*>(request) + 1;
         state_.val = *pVal;

         ++state_.write_count;
         uint64_t* pRet = reinterpret_cast<uint64_t*>(reply);
         *pRet = state_.write_count;
      }

      TestState state_;

      // This is owned by the replica. It will be cleaned up in the replica
      // destructor.
      IStateTransfer* state_transfer_;
};

} // namespace DeterministicTest

#endif // BFTENGINE_TESTS_REPLICA_APP_STATE_H
