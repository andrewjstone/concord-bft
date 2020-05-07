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

#include <memory>
#include <optional>

#include "communication/ICommunication.hpp"
#include "Logger.hpp"

#include "bftclient/config.h"
#include "matcher.h"
#include "msg_receiver.h"
#include "exception.h"

using namespace std::chrono_literals;

namespace bft::client {

class Client {
 public:
  Client(std::unique_ptr<bft::communication::ICommunication> comm, ClientConfig config)
      : communication_(std::move(comm)), config_(config) {}

  // Send a message where the reply gets allocated by the callee and returned in a vector.
  // The message to be sent is moved into the caller to prevent unnecessary copies.
  //
  // Throws a BftClientException on error.
  Reply send(const WriteConfig& config, Msg&& request);
  Reply send(const ReadConfig& config, Msg&& request);

 private:
  // Extract matcher configurations from operational configurations
  //
  // Throws BftClientException on error.
  MatchConfig WriteConfigToMatchConfig(const WriteConfig&);
  MatchConfig ReadConfigToMatchConfig(const ReadConfig&);

  // Ensure that each replica in `destination` is part of `all_replicas`
  //
  // Throws an InvalidDestinationException if validation vails
  void ValidateDestinations(const std::set<ReplicaId>& destinations);

  // Convert Quorums to a compatible MofN quorum for use by the matcher.
  // This conversion handles quorum validation.
  //
  // Throws BftClientException on error.
  MofN QuorumToMofN(const LinearizableQuorum& quorum);
  MofN QuorumToMofN(const ByzantineSafeQuorum& quorum);
  MofN QuorumToMofN(const All& quorum);

  std::unique_ptr<bft::communication::ICommunication> communication_;
  ClientConfig config_;
  concordlogger::Logger logger_ = concordlogger::Log::getLogger("concord.bft.client");

  std::optional<uint16_t> primary_;
  std::optional<Matcher> outstanding_request_;
};

}  // namespace bft::client
