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

#include <optional>

#include "bftclient/config.h"

namespace bft::client {
class Request {
  Request(const ClientConfig& client_config, const RequestConfig& config, std::optional<uint16_t> primary)
      : client_config_(client_config), config_(config), primary_(primary), start_(std::chrono::steady_clock::now()) {}

  // A blocking send call.
  //
  // Send a message and return a reply for the client caller along with the known primary.
  //
  // Throw a BftClientException on error.
  std::pair<Reply, std::optional<uint16_t>> send(Msg&& msg);

 private:
  ClientConfig client_config_;
  RequestConfig config_;

  std::optional<uint16_t> primary_;

  // The time the request started
  std::chrono::steady_clock::time_point start_;
};

};  // namespace bft::client