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

#include <map>
#include <optional>
#include <set>

#include "bftclient/config.h"
#include "msg_receiver.h"

namespace bft::client {

struct MatchConfig {
  // All quorums can be distilled into an MofN quorum.
  MofN quorum;
  uint64_t sequence_number;
};

// The parts of data that must match in a reply for quorum to be reached
struct MatchKey {
  ReplyMetadata metadata;
  Msg data;
};

// A successful match
struct Match {
  Reply reply;
  std::optional<uint16_t> primary;
};

// Match replies for a given quorum.
class Matcher {
 public:
  Matcher(const MatchConfig& config) : config_(config) {}

  std::optional<Match> onReply(UnmatchedReply&& reply);

 private:
  // Check the validity of a reply
  bool valid(const UnmatchedReply& reply);

  // Is the reply from a source listed in the quorum's destination?
  bool valid_source(const ReplicaId source);

  // Check for a quorum based on config_ and matches_
  std::optional<Match> match();

  MatchConfig config_;
  std::optional<uint16_t> primary_;

  std::map<MatchKey, std::set<ReplicaSpecificInfo>> matches_;
};

};  // namespace bft::client