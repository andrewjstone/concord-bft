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

#include "Logger.hpp"

#include "matcher.h"

namespace bft::client {

concordlogger::Logger logger = concordlogger::Log::getLogger("bftclient.matcher");

std::optional<Match> Matcher::onReply(UnmatchedReply&& reply) {
  if (!valid(reply)) return std::nullopt;

  auto key = MatchKey{std::move(reply.metadata), std::move(reply.data)};
  const auto [it, success] = matches_[key].insert({reply.rsi.from, std::move(reply.rsi.data)});
  if (!success) {
    LOG_ERROR(logger,
              "Received two different pieces of replica specific information from: "
                  << reply.rsi.from.val << ". Replica may be malicious: discarding both replies.");
    matches_[key].erase(it);
  }

  return match();
}

std::optional<Match> Matcher::match() {
  auto result = std::find_if(matches_.begin(), matches_.end(), [this](const auto& match) {
    return match.second.size() == config_.quorum.wait_for;
  });
  if (result == matches_.end()) return std::nullopt;
  return Match{Reply{std::move(result->first.data), std::move(result->second)}, result->first.metadata.primary.val};
}

bool Matcher::valid(const UnmatchedReply& reply) {
  if (config_.sequence_number != reply.metadata.seq_num) {
    LOG_WARN(logger,
             "Received msg with mismatched sequence number. Expected: " << config_.sequence_number
                                                                        << ", got: " << reply.metadata.seq_num);
    return false;
  }

  if (!valid_source(reply.rsi.from)) {
    LOG_WARN(logger, "Received reply from invalid source: " << reply.rsi.from.val);
    return false;
  }

  return true;
}

bool Matcher::valid_source(const ReplicaId source) {
  const auto& valid_sources = config_.quorum.destination;
  return std::find(valid_sources.begin(), valid_sources.end(), source) != valid_sources.end();
}

}  // namespace bft::client