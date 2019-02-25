// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

// TODO(GG): remove
#define DEBUG_STATISTICS

#ifdef DEBUG_STATISTICS

#include <cstddef>
#include <stdint.h>
#include "TimeUtils.hpp"

namespace bftEngine {
namespace impl {

const double DEBUG_STAT_PERIOD_SECONDS = 7;  // 2;

class DebugStatistics {
 public:
  static void onCycleCheck();

  static void onReceivedExMessage(uint16_t type);

  static void onSendExMessage(uint16_t type);

  static void onRequestCompleted(bool isReadOnly);

  static void initDebugStatisticsData();

  static void freeDebugStatisticsData();

 private:
  struct DebugStatDesc {
    bool initialized;
    Time lastCycleTime;

    size_t receivedMessages;
    size_t sendMessages;
    size_t completedReadOnlyRequests;
    size_t completedReadWriteRequests;
    size_t numberOfReceivedSTMessages;
    size_t numberOfReceivedStatusMessages;
    size_t numberOfReceivedCommitMessages;

    DebugStatDesc() : initialized(false) {}
  };

#ifdef USE_TLS
  static DebugStatDesc& getDebugStatDesc() {
    ThreadLocalData* l = ThreadLocalData::Get();
    DebugStatDesc* dsd = (DebugStatDesc*)l->debugStatData;
    return *(dsd);
  }
#else

  static DebugStatDesc globalDebugStatDesc;

  static DebugStatDesc& getDebugStatDesc() { return globalDebugStatDesc; }

#endif

  static void clearCounters(DebugStatDesc& d);
};

}  // namespace impl
}  // namespace bftEngine

#endif
