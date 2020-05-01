// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the sub-component's license, as noted in the
// LICENSE file.

#include "gtest/gtest.h"
#include "rapidcheck/rapidcheck.h"
#include "rapidcheck/extras/gtest.h"

#include "bftengine/ClientMsgs.hpp"

#include "msg_receiver.h"
#include "bftclient/bft_client.h"

using namespace bft::client;

TEST(msg_receiver_tests, unmatched_replies_returned_no_rsi) {
  MsgReceiver receiver;
  std::vector<char> reply(sizeof(bftEngine::ClientReplyMsgHeader) + 20);

  // Fill in the header part of the reply
  auto* header = reinterpret_cast<bftEngine::ClientReplyMsgHeader*>(reply.data());
  header->msgType = REPLY_MSG_TYPE;
  header->currentPrimaryId = 1;
  header->reqSeqNum = 100;
  header->replyLength = 20;
  header->replicaSpecificInfoLength = 0;

  // Handle the message
  auto source = 1;
  receiver.onNewMessage(source, reply.data(), reply.size());

  // Wait to see that the message gets properly unpacked and delivered.
  auto replies = receiver.wait(1ms);

  ASSERT_EQ(1, replies.size());
  ASSERT_EQ(header->currentPrimaryId, replies[0].metadata.primary.val);
  ASSERT_EQ(header->reqSeqNum, replies[0].metadata.seq_num);
  ASSERT_EQ(Msg(header->replyLength), replies[0].data);
  ASSERT_EQ(1, replies[0].rsi.from.val);
  ASSERT_EQ(0, replies[0].rsi.data.size());
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}