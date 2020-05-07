
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

#include <exception>
#include <string>

#include "bftclient/config.h"

namespace bft::client {

class BftClientException : public std::exception {
 public:
  explicit BftClientException(const std::string& what) : msg(what){};

  virtual const char* what() const noexcept override { return msg.c_str(); }

 private:
  std::string msg;
};

//
class BadQuorumConfigException : public BftClientException {
 public:
  BadQuorumConfigException(const std::string& what) : BftClientException(what) {}
};

class InvalidDestinationException : public BftClientException {
 public:
  InvalidDestinationException(ReplicaId replica_id)
      : BftClientException("Replica: " + std::to_string(replica_id.val) + " is not part of the cluster.") {}
};

}  // namespace bft::client