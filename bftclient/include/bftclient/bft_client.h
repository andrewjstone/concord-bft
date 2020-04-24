// Concord
//
// Copyright (c) 2018-2019 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <chrono>
#include <memory>
#include <variant>
#include <vector>

#include "communication/ICommunication.hpp"

using namespace std::chrono_literals;

namespace bft::client {

// A typesafe replica id.
struct ReplicaId {
  uint16_t val;
};

// The configuration for a single instance of a client.
struct ClientConfig {
  ReplicaId id;
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
  std::vector<ReplicaId> destination;
};

// A quorum of F + 1 matching replies from `destination` must be received for the `send` call to complete.
struct ByzantineSafeQuorum {
  std::vector<ReplicaId> destination;
};

// A matching reply from every replica in `destination` must be received for the `send` call to complete.
struct All {
  std::vector<ReplicaId> destination;
};

// A matching reply from `wait_for` number of replicas from `destination` must be received for the
// `send` call to complete.
struct MofN {
  size_t wait_for;
  std::vector<ReplicaId> destination;
};

typedef std::variant<LinearizableQuorum, ByzantineSafeQuorum, All, MofN> Destination;

enum Flags : uint8_t { EMPTY_FLAGS_REQ = 0x0, READ_ONLY_REQ = 0x1, PRE_PROCESS_REQ = 0x2 };

// The configuration for a single request.
struct RequestConfig {
  Flags flags;
  uint64_t sequence_number;
  std::chrono::milliseconds timeout = 5s;
  std::string correlation_id = "";
  Destination destination;
};

struct ReplicaSpecificInfo {
  ReplicaId from;
  std::vector<char> data;
};

typedef std::vector<char> Request;

// `matched_data` contains any data that must be identical for a quorum of replicas
// `rsi` contains replica specific information that was received for each replying replica.
struct Reply {
  std::vector<char> matched_data;
  std::vector<ReplicaSpecificInfo> rsi;
};

class Client {
 public:
  Client(std::unique_ptr<bft::communication::ICommunication> comm, ClientConfig config)
      : communication_(std::move(comm)), config_(config) {}

  // Send a message where the reply gets allocated by the callee and returned in a vector.
  // The message to be sent is moved into the caller to prevent unnecessary copies.
  //
  // Throws a BftClientException on error.
  Reply send(const RequestConfig& config, Request&& request);

 private:
  std::unique_ptr<bft::communication::ICommunication> communication_;
  ClientConfig config_;
};

}  // namespace bft::client
