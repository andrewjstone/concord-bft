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


#include "app_state.hpp"

namespace DeterministicTest {

// Handler for the upcall from Concord-BFT.
// Implements bftEngine::RequestHandler.
//
// Since we are generating the test inputs, we assume they are formatted
// correctly and read_only requests only use Reads.
int TestAppState::execute(uint16_t client_id,
                          bool read_only,
                          uint32_t request_size,
                          const char* request,
                          uint32_t max_reply_size,
                          char* out_reply,
                          uint32_t& out_reply_size) {

   if (IsRead(request)) {
      GetVal(out_reply);
   } else {
      WriteVal(request, out_reply);
   }
   out_reply_size = kValSize;

   return 0;
}

} // namespace DeterministicTest
