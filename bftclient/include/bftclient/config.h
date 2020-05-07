// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "bftclient/base_types.h"
#include "bftclient/quorums.h"

using namespace std::chrono_literals;

namespace bft::client {

// The configuration for a single instance of a client.
struct ClientConfig {
  ReplicaId id;
  std::set<ReplicaId> all_replicas;
  uint16_t f_val;
  uint16_t c_val;
  std::chrono::milliseconds initial_retry_timeout = 150ms;
  std::chrono::milliseconds min_retry_timeout = 50ms;
  std::chrono::milliseconds max_retry_timeout = 1s;
  size_t send_to_all_replicas_first_threshold = 4;
  size_t sent_to_all_replicas_period_threshold = 2;
  size_t periodic_reset_threshold = 30;
};

// Generic per-request configuration shared by reads and writes.
struct RequestConfig {
  bool pre_execute;
  uint64_t sequence_number;
  uint32_t max_reply_size;
  std::chrono::milliseconds timeout = 5s;
  std::string correlation_id = "";
};

// The configuration for a single write request.
struct WriteConfig {
  RequestConfig request;
  WriteQuorum quorum;
};

// The configuration for a single read request.
struct ReadConfig {
  RequestConfig request;
  ReadQuorum quorum;
};

}  // namespace bft::client