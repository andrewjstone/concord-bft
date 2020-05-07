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

using namespace std::chrono_literals;

namespace bft::client {

// A typesafe replica id.
struct ReplicaId {
  uint16_t val;

  bool operator==(const ReplicaId& other) const { return val == other.val; }
  bool operator!=(const ReplicaId& other) const { return val != other.val; }
  bool operator<(const ReplicaId& other) const { return val < other.val; }
};

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

// A quorum of 2F + C + 1 matching replies from `destination` must be received for the `send` call
// to complete.
struct LinearizableQuorum {
  std::set<ReplicaId> destinations;
};

// A quorum of F + 1 matching replies from `destination` must be received for the `send` call to complete.
struct ByzantineSafeQuorum {
  std::set<ReplicaId> destinations;
};

// A matching reply from every replica in `destination` must be received for the `send` call to complete.
struct All {
  std::set<ReplicaId> destinations;
};

// A matching reply from `wait_for` number of replicas from `destination` must be received for the
// `send` call to complete.
struct MofN {
  size_t wait_for;
  std::set<ReplicaId> destinations;
};

// Reads and writes support different types of quorums.
typedef std::variant<LinearizableQuorum, ByzantineSafeQuorum, All, MofN> ReadQuorum;
typedef std::variant<LinearizableQuorum, ByzantineSafeQuorum> WriteQuorum;

enum Flags : uint8_t { EMPTY_FLAGS_REQ = 0x0, READ_ONLY_REQ = 0x1, PRE_PROCESS_REQ = 0x2 };

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

struct ReplicaSpecificInfo {
  ReplicaId from;
  std::vector<char> data;
};

typedef std::vector<char> Msg;

// `matched_data` contains any data that must be identical for a quorum of replicas
// `rsi` contains replica specific information that was received for each replying replica.
struct Reply {
  Msg matched_data;
  std::map<ReplicaId, Msg> rsi;
};

}  // namespace bft::client