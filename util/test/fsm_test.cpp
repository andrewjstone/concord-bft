// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the
// LICENSE file.

#include <variant>

#include "gtest/gtest.h"
#include "fsm.h"

using namespace concord::fsm;

struct TestMsg {};
struct TestMsg2 {};
struct TestMsg3 {};
struct TestOutputMsg {};
struct TestOutputMsg2 {};
struct UnhandleableMsg {};

struct FinalState {};

struct TestState {
  Transition<TestState> on(TestMsg&& msg) && {
    ++msgs_received;
    return Transition{std::move(*this)};
  };

  Transition<FinalState, TestOutputMsg> on(TestMsg2&& msg) && {
    ++msgs_received;
    return Transition{FinalState{}, TestOutputMsg{}};
  };

  using T1 = Transition<TestState, TestOutputMsg2>;
  using T2 = Transition<FinalState>;
  std::variant<T1, T2> on(TestMsg3&& msg) && {
    ++msgs_received;
    if (msgs_received < 2) {
      return Transition{std::move(*this), TestOutputMsg2{}};
    }
    return Transition{FinalState{}};
  }

  size_t msgs_received = 0;
};

using AllStates = std::variant<TestState, FinalState>;
using AllMsgs = std::variant<TestMsg, TestMsg2, TestMsg3, TestOutputMsg, TestOutputMsg2, UnhandleableMsg>;

TEST(fsm_test, normal_state_transition) {
  auto transition = TestState{}.on(TestMsg{});
  ASSERT_EQ(1, transition.state.msgs_received);
  ASSERT_EQ(0, transition.numMsgs());
}

TEST(fsm_test, unify_a_single_transition) {
  std::vector<AllMsgs> out;
  auto outstate = unifyTransition<AllStates>(out, TestState{}.on(TestMsg{}));
  ASSERT_TRUE(std::holds_alternative<TestState>(outstate));
  ASSERT_EQ(0, out.size());
}

TEST(fsm_test, dispatch_success) {
  std::vector<AllMsgs> out;
  Dispatcher<AllStates, AllMsgs> dispatcher;
  auto rv = dispatcher.dispatch(out, TestState{}, TestMsg{});
  // The new state is the same as the old state
  ASSERT_TRUE(std::holds_alternative<AllStates>(rv));
  ASSERT_EQ(1, std::get<TestState>(std::get<AllStates>(rv)).msgs_received);
  // No output messages were generated
  ASSERT_EQ(0, out.size());
}

TEST(fsm_test, dispatch_failure) {
  std::vector<AllMsgs> out;
  Dispatcher<AllStates, AllMsgs> dispatcher;
  auto rv = dispatcher.dispatch(out, TestState{}, UnhandleableMsg{});

  // An UnhandledMsg was received. It returns the passed in message and the same state.
  bool is_invalid = std::holds_alternative<UnhandledMsg<AllStates, UnhandleableMsg>>(rv);
  ASSERT_TRUE(is_invalid);

  // No messages were received by the handler that accepts a TestMsg.
  auto invalid_msg = std::get<UnhandledMsg<AllStates, UnhandleableMsg>>(rv);
  auto state = std::get<TestState>(invalid_msg.state);
  ASSERT_EQ(0, state.msgs_received);

  // No output messages were generated
  ASSERT_EQ(0, out.size());
}

TEST(fsm_test, dispatch_to_final_state) {
  std::vector<AllMsgs> out;
  Dispatcher<AllStates, AllMsgs> dispatcher;
  auto rv = dispatcher.dispatch(out, TestState{}, TestMsg2{});

  // The new state is FinalState
  ASSERT_TRUE(std::holds_alternative<AllStates>(rv));

  ASSERT_TRUE(std::holds_alternative<FinalState>(std::get<AllStates>(rv)));
  auto state = std::get<FinalState>(std::get<AllStates>(rv));

  // A single output messages was generated
  ASSERT_EQ(1, out.size());
  ASSERT_TRUE(std::holds_alternative<TestOutputMsg>(out[0]));

  out.clear();

  // All further dispatches result in UnhandledMsg responses.
  auto rv2 = dispatcher.dispatch(out, std::move(state), TestMsg{});
  auto state2 = std::get<UnhandledMsg<AllStates, TestMsg>>(rv2).state;
  ASSERT_EQ(0, out.size());

  auto rv3 = dispatcher.dispatch(out, std::move(state2), TestMsg2{});
  bool is_invalid = std::holds_alternative<UnhandledMsg<AllStates, TestMsg2>>(rv3);
  ASSERT_TRUE(is_invalid);
  ASSERT_EQ(0, out.size());
}

TEST(fsm_test, dispatch_variant_transitions) {
  std::vector<AllMsgs> out;
  Dispatcher<AllStates, AllMsgs> dispatcher;
  auto rv = dispatcher.dispatch(out, TestState{}, TestMsg3{});

  // We stay in TestState after the first receipt of TestMsg3
  ASSERT_TRUE(std::holds_alternative<AllStates>(rv));
  ASSERT_TRUE(std::holds_alternative<TestState>(std::get<AllStates>(rv)));
  auto state = std::get<TestState>(std::get<AllStates>(rv));

  // A single output messages was generated
  ASSERT_EQ(1, out.size());
  ASSERT_TRUE(std::holds_alternative<TestOutputMsg2>(out[0]));
  out.clear();

  // We move to FinalState after second receipt of TestMsg3
  auto rv2 = dispatcher.dispatch(out, std::move(state), TestMsg3{});
  ASSERT_TRUE(std::holds_alternative<AllStates>(rv));
  ASSERT_TRUE(std::holds_alternative<FinalState>(std::get<AllStates>(rv2)));

  // A single output messages was generated
  ASSERT_EQ(0, out.size());
}

// If the FSM driver managing the states doesn't need to know which state is receiving messages it
// can just pass in the variant retrieved from the prior `on` call.
TEST(fsm_test, dispatch_without_unwrapping_state) {
  std::vector<AllMsgs> out;
  Dispatcher<AllStates, AllMsgs> dispatcher;

  // Assume we are sending a message that can be handled.
  auto rv = dispatcher.dispatch(out, TestState{}, TestMsg3{});
  auto rv2 = dispatcher.dispatch(out, std::move(std::get<AllStates>(rv)), TestMsg3{});
  ASSERT_TRUE(std::holds_alternative<FinalState>(std::get<AllStates>(rv2)));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
