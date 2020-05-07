// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use
// this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license
// terms. Your use of these subcomponents is subject to the terms and conditions of the
// subcomponent's license, as noted in the LICENSE file.

#include "bftclient/bft_client.h"
#include "bftengine/ClientMsgs.hpp"
#include "exception.h"

namespace bft::client {

void Client::ValidateDestinations(const std::set<ReplicaId>& destinations) {
  ReplicaId captured;
  if (std::any_of(destinations.begin(), destinations.end(), [this, &captured](auto replica_id) {
        captured = replica_id;
        return config_.all_replicas.count(replica_id) == 0;
      })) {
    throw InvalidDestinationException(captured);
  }
}

MofN Client::QuorumToMofN(const LinearizableQuorum& quorum) {
  MofN new_quorum;
  new_quorum.wait_for = 2 * config_.f_val + config_.c_val + 1;
  if (quorum.destinations.empty()) {
    // If the user doesn't provide destinations, send to all replicas
    new_quorum.destinations = config_.all_replicas;
  } else if (quorum.destinations.size() < new_quorum.wait_for) {
    throw BadQuorumConfigException(
        "Destination does not contain enough replicas for a linearizable quorum. Destination size: " +
        std::to_string(quorum.destinations.size()) +
        "is less than 2f + c + 1 = " + std::to_string(new_quorum.wait_for));
  } else {
    ValidateDestinations(quorum.destinations);
    new_quorum.destinations = quorum.destinations;
  }
  return new_quorum;
}

MofN Client::QuorumToMofN(const ByzantineSafeQuorum& quorum) {
  MofN new_quorum;
  new_quorum.wait_for = config_.f_val + 1;
  if (quorum.destinations.empty()) {
    // If the user doesn't provide a destination, send to all replicas
    new_quorum.destinations = config_.all_replicas;
  } else if (quorum.destinations.size() < new_quorum.wait_for) {
    throw BadQuorumConfigException(
        "Destination does not contain enough replicas for a byzantine fault tolerant quorum. Destination size: " +
        std::to_string(quorum.destinations.size()) + "is less than f + 1 = " + std::to_string(new_quorum.wait_for));
  } else {
    ValidateDestinations(quorum.destinations);
    new_quorum.destinations = quorum.destinations;
  }
  return new_quorum;
}

MofN Client::QuorumToMofN(const All& quorum) {
  MofN new_quorum;
  if (quorum.destinations.empty()) {
    // If the user doesn't provide a destination, send to all replicas
    new_quorum.wait_for = config_.all_replicas.size();
    new_quorum.destinations = config_.all_replicas;
  } else {
    ValidateDestinations(quorum.destinations);
    new_quorum.wait_for = quorum.destinations.size();
    new_quorum.destinations = quorum.destinations;
  }
  return new_quorum;
}

MatchConfig Client::WriteConfigToMatchConfig(const WriteConfig& write_config) {
  MatchConfig mc;
  mc.sequence_number = write_config.request.sequence_number;

  if (std::holds_alternative<LinearizableQuorum>(write_config.quorum)) {
    mc.quorum = QuorumToMofN(std::get<LinearizableQuorum>(write_config.quorum));
  } else {
    mc.quorum = QuorumToMofN(std::get<ByzantineSafeQuorum>(write_config.quorum));
  }
  return mc;
}

MatchConfig Client::ReadConfigToMatchConfig(const ReadConfig& read_config) {
  MatchConfig mc;
  mc.sequence_number = read_config.request.sequence_number;

  if (std::holds_alternative<LinearizableQuorum>(read_config.quorum)) {
    mc.quorum = QuorumToMofN(std::get<LinearizableQuorum>(read_config.quorum));

  } else if (std::holds_alternative<ByzantineSafeQuorum>(read_config.quorum)) {
    mc.quorum = QuorumToMofN(std::get<ByzantineSafeQuorum>(read_config.quorum));

  } else if (std::holds_alternative<All>(read_config.quorum)) {
    mc.quorum = QuorumToMofN(std::get<All>(read_config.quorum));

  } else {
    mc.quorum = std::move(std::get<MofN>(read_config.quorum));
    ValidateDestinations(mc.quorum.destinations);
  }
  return mc;
}

Reply send(const WriteConfig& config, Msg&& request) { return Reply{}; }

Reply send(const ReadConfig& config, Msg&& request) { return Reply{}; }

}  // namespace bft::client