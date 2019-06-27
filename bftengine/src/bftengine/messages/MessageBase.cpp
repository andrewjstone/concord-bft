// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include <cstring>

#include "MessageBase.hpp"
#include "assertUtils.hpp"

#ifdef DEBUG_MEMORY_MSG
#include <set>

#ifdef USE_TLS
#error DEBUG_MEMORY_MSG is not supported with USE_TLS
#endif

namespace bftEngine {
namespace impl {

static std::set<MessageBase*>
    liveMessagesDebug;  // GG: if needed, add debug information

void MessageBase::printLiveMessages() {
  printf("\nDumping all live messages:");
  for (std::set<MessageBase*>::iterator it = liveMessagesDebug.begin();
       it != liveMessagesDebug.end();
       it++) {
    printf("<type=%d, size=%d>", (*it)->type(), (*it)->size());
    printf("%8s", " ");  // space
  }
  printf("\n");
}

}  // namespace impl
}  // namespace bftEngine

#endif

namespace bftEngine {
namespace impl {

void MessageBase::debugLiveMessages() {
#ifdef DEBUG_MEMORY_MSG
  liveMessagesDebug.insert(this);
#endif
}

MessageBase::~MessageBase() {
#ifdef DEBUG_MEMORY_MSG
  liveMessagesDebug.erase(this);
#endif
}

}  // namespace impl

}  // namespace bftEngine
}  // namespace bftEngine
