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
#include <variant>

#include "bftclient/config.h"
#include "communication/ICommunication.hpp"
#include "request.h"
#include "Logger.hpp"

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
  Reply send(const RequestConfig& config, Msg&& request);

 private:
  std::unique_ptr<bft::communication::ICommunication> communication_;
  ClientConfig config_;
  concordlogger::Logger logger_ = concordlogger::Log::getLogger("concord.bft.client");

  std::optional<uint16_t> primary_;
  std::optional<Request> outstanding_request_;
};

}  // namespace bft::client
