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

#include <utility>
#include <type_traits>

namespace concord::fsm {

template <typename State, typename... Msgs>
struct Transition {
  Transition(State state, Msgs... msgs) : state(std::move(state)), msgs(std::move(msgs)...) {}

  size_t numMsgs() { return std::tuple_size<decltype(msgs)>(); }

  State state;
  std::tuple<Msgs...> msgs;
};

// Take a transition and unify its state and messages into global variants: i.e. variants containing
// the supersets of all states and all messages used in a given FSM.
//
// Since there can be multiple messages, we must put them in a collection of the unified type. We
// use a vector, as it is the most straightforward. We take the vector by reference, so that we
// don't force an allocation per call.
template <typename StateVariant, typename MsgVariant, typename State>
StateVariant unify(std::vector<MsgVariant>& out, State state) {
  return std::move(state);
}

template <typename StateVariant, typename MsgVariant, typename State, typename Msg, typename... Msgs>
StateVariant unify(std::vector<MsgVariant>& out, State state, Msg msg, Msgs... msgs) {
  out.emplace_back(std::move(msg));
  return unify<StateVariant>(out, std::move(state), std::move(msgs)...);
}

template <typename StateVariant, typename MsgVariant, typename State, typename... Msgs>
StateVariant unifyTransition(std::vector<MsgVariant>& out, Transition<State, Msgs...>&& transition) {
  auto all_params = std::tuple_cat(std::make_tuple(std::ref(out), std::forward<State>(transition.state)),
                                   std::forward<decltype(transition.msgs)>(transition.msgs));
  // std::apply cannot take a function template, so wrap the call to unify in a lambda
  // See https://blog.tartanllama.xyz/passing-overload-sets/
  auto f = [](auto&&... p) -> decltype(auto) { return unify<StateVariant>(std::forward<decltype(p)>(p)...); };
  return std::apply(f, all_params);
}

template <typename StateVariant, typename MsgVariant, typename Transitions>
StateVariant unifyTransition(std::vector<MsgVariant>& out, Transitions&& transitions) {
  return std::visit(
      [&out](auto&& transition) {
        return unifyTransition<StateVariant>(out, std::forward<decltype(transition)>(transition));
      },
      std::forward<decltype(transitions)>(transitions));
}

template <typename StateVariant, typename Msg>
struct UnhandledMsg {
  StateVariant state;
  Msg msg;
};

// This is the entrypoint to each state in the state machine.
//
// A dispatcher has the responsibility of calling the `on` method of each state with a given message
// and then translating the response such that:
//  * Each state is wrapped in a StateVariant so that all states can be handled by the caller uniformly.
//  * Each output message is wrapped in a MsgVariant and put into a vector so that the caller can handle the messages
//  in a single type and does not have to play template tricks as in this library to deal with different responses
//  from different overloads.
//  * Ensuring that if a Msg cannot be handled in a given state, that a proper `UnhandledMsg` response is returned to
//  the user containing the current state and the message that was not handled.

template <typename StateVariant, typename MsgVariant>
class Dispatcher {
 public:
  template <typename State, typename Msg>
  using Result = std::variant<StateVariant, UnhandledMsg<StateVariant, Msg>>;

  // Trait to ensure that a State has an `on` method that takes a message of type `Msg`.
  template <typename State, typename Msg, typename = std::void_t<>>
  struct CanHandleMsgT : std::false_type {};

  template <typename State, typename Msg>
  struct CanHandleMsgT<State, Msg, std::void_t<decltype(std::declval<State>().on(std::declval<Msg>()))>>
      : std::true_type {};

  // `dispatch` is the way an FSM interacts with individual states.
  //
  // We always want to move the state to a new state. This will be violated if users accidentally
  // call `on` through an lvalue and result in no matching call to dispatch.
  // In order to prevent that mistake, users should implement their `on` methods with a trailing
  // `&&`.
  template <typename State,
            typename Msg,
            typename = std::enable_if_t<CanHandleMsgT<State, Msg>::value && std::is_rvalue_reference<State&&>::value>>
  Result<State, Msg> dispatch(std::vector<MsgVariant>& out, State&& state, Msg&& msg) {
    return unifyTransition<StateVariant>(out, std::forward<State>(state).on(std::forward<Msg>(msg)));
  }

  template <typename State,
            typename Msg,
            typename = std::enable_if_t<!CanHandleMsgT<State, Msg>::value && std::is_rvalue_reference<State&&>::value>,
            // Extra parameter to disambiguate the prior enable_if overload, since default templates
            // are not accounted for in overloading.
            typename = int>
  Result<State, Msg> dispatch(std::vector<MsgVariant>& out, State&& state, Msg&& msg) {
    return UnhandledMsg<StateVariant, Msg>{std::forward<State>(state), std::forward<Msg>(msg)};
  }

  template <typename Msg>
  decltype(auto) dispatch(std::vector<MsgVariant>& out, StateVariant&& wrapped, Msg&& msg) {
    return std::visit(
        [&out, this, msg = std::forward<Msg>(msg)](auto&& state) mutable {
          return this->dispatch(out, std::forward<decltype(state)>(state), std::forward<Msg>(msg));
        },
        std::forward<decltype(wrapped)>(wrapped));
  }
};

}  // namespace concord::fsm
