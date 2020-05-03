// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the 'License").
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

TEST(msg_receiver_tests, unmatched_replies_with_rsi) {
  MsgReceiver receiver;
  std::vector<char> reply(sizeof(bftEngine::ClientReplyMsgHeader) + 20);

  // Fill in the header part of the reply
  auto* header = reinterpret_cast<bftEngine::ClientReplyMsgHeader*>(reply.data());
  header->msgType = REPLY_MSG_TYPE;
  header->currentPrimaryId = 1;
  header->reqSeqNum = 100;
  header->replyLength = 20;
  header->replicaSpecificInfoLength = 5;

  // Handle the message
  auto source = 1;
  receiver.onNewMessage(source, reply.data(), reply.size());

  // Wait to see that the message gets properly unpacked and delivered.
  auto replies = receiver.wait(1ms);

  ASSERT_EQ(1, replies.size());
  ASSERT_EQ(header->currentPrimaryId, replies[0].metadata.primary.val);
  ASSERT_EQ(header->reqSeqNum, replies[0].metadata.seq_num);
  ASSERT_EQ(Msg(header->replyLength - header->replicaSpecificInfoLength), replies[0].data);
  ASSERT_EQ(1, replies[0].rsi.from.val);
  ASSERT_EQ(Msg(header->replicaSpecificInfoLength), replies[0].rsi.data);
}

TEST(msg_receiver_tests, no_replies_small_msg) {
  MsgReceiver receiver;
  std::vector<char> reply(sizeof(bftEngine::ClientReplyMsgHeader) - 1);

  // Handle the message
  auto source = 1;
  receiver.onNewMessage(source, reply.data(), reply.size());

  // Wait to see that the message gets properly unpacked and delivered.
  auto replies = receiver.wait(1ms);

  ASSERT_EQ(0, replies.size());
}

TEST(msg_receiver_tests, no_replies_bad_msg_type) {
  MsgReceiver receiver;
  std::vector<char> reply(sizeof(bftEngine::ClientReplyMsgHeader) + 20);

  // Fill in the header part of the reply
  auto* header = reinterpret_cast<bftEngine::ClientReplyMsgHeader*>(reply.data());
  header->msgType = REQUEST_MSG_TYPE;

  // Handle the message
  auto source = 1;
  receiver.onNewMessage(source, reply.data(), reply.size());

  // Wait to see that the message gets properly unpacked and delivered.
  auto replies = receiver.wait(1ms);
  ASSERT_EQ(0, replies.size());
}

TEST(matcher_tests, wait_for_1_out_of_1) {
  ReplicaId source{1};
  uint64_t seq_num = 5;
  MatchConfig config{MofN{1, {source}}, seq_num};
  Matcher matcher(config);

  ReplicaId primary{1};
  UnmatchedReply reply{
      ReplyMetadata{primary, seq_num}, {'h', 'e', 'l', 'l', 'o'}, ReplicaSpecificInfo{source, {'r', 's', 'i'}}};
  auto match = matcher.onReply(std::move(reply));
  ASSERT_TRUE(match.has_value());
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}