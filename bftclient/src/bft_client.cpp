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

namespace bft::client {

Reply Client::send(const WriteConfig& config, Msg&& request) { return Reply{}; }

Reply Client::send(const ReadConfig& config, Msg&& request) { return Reply{}; }

MatchConfig Client::WriteConfigToMatchConfig(const WriteConfig& write_config) {
  MatchConfig mc;
  mc.sequence_number = write_config.request.sequence_number;

  if (std::holds_alternative<LinearizableQuorum>(write_config.quorum)) {
    mc.quorum = quorum_converter_.ToMofN(std::get<LinearizableQuorum>(write_config.quorum));
  } else {
    mc.quorum = quorum_converter_.ToMofN(std::get<ByzantineSafeQuorum>(write_config.quorum));
  }
  return mc;
}

MatchConfig Client::ReadConfigToMatchConfig(const ReadConfig& read_config) {
  MatchConfig mc;
  mc.sequence_number = read_config.request.sequence_number;

  if (std::holds_alternative<LinearizableQuorum>(read_config.quorum)) {
    mc.quorum = quorum_converter_.ToMofN(std::get<LinearizableQuorum>(read_config.quorum));

  } else if (std::holds_alternative<ByzantineSafeQuorum>(read_config.quorum)) {
    mc.quorum = quorum_converter_.ToMofN(std::get<ByzantineSafeQuorum>(read_config.quorum));

  } else if (std::holds_alternative<All>(read_config.quorum)) {
    mc.quorum = quorum_converter_.ToMofN(std::get<All>(read_config.quorum));

  } else {
    mc.quorum = quorum_converter_.ToMofN(std::get<MofN>(read_config.quorum));
  }
  return mc;
}

}  // namespace bft::client