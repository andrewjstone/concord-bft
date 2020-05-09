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
#include "DynamicUpperLimitWithSimpleFilter.hpp"

#include "bftclient/config.h"
#include "matcher.h"
#include "msg_receiver.h"
#include "exception.h"

using namespace std::chrono_literals;

namespace bft::client {

class Client {
 public:
  Client(std::unique_ptr<bft::communication::ICommunication> comm, const ClientConfig& config)
      : communication_(std::move(comm)),
        config_(config),
        quorum_converter_(config_.all_replicas, config_.f_val, config_.c_val) {}

  // Send a message where the reply gets allocated by the callee and returned in a vector.
  // The message to be sent is moved into the caller to prevent unnecessary copies.
  //
  // Throws a BftClientException on error.
  Reply Send(const WriteConfig& config, Msg&& request);
  Reply Send(const ReadConfig& config, Msg&& request);

 private:
  // Extract a matcher configurations from operational configurations
  //
  // Throws BftClientException on error.
  MatchConfig WriteConfigToMatchConfig(const WriteConfig&);
  MatchConfig ReadConfigToMatchConfig(const ReadConfig&);

  std::unique_ptr<bft::communication::ICommunication> communication_;
  ClientConfig config_;
  concordlogger::Logger logger_ = concordlogger::Log::getLogger("bftclient");

  // The client doesn't always know the current primary.
  std::optional<uint16_t> primary_;

  // Each outstanding request matches replies using a new matcher.
  // If there are no outstanding requests, then this is a nullopt;
  std::optional<Matcher> outstanding_request_;

  // A class that takes all Quorum types and converts them to an MofN quorum, with validation.
  QuorumConverter quorum_converter_;

  // A utility for calculating dynamic timeouts for replies
  DynamicUpperLimitWithSimpleFilter<uint64_t> limitOfExpectedOperationTime_;
};

}  // namespace bft::client
