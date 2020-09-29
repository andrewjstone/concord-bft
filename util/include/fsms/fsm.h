// Concord is a distributed system based around message passing. As such, a finite state machine in
// concord is message driven. A state machine based system is made up of two primitives:
//   * State - A data type that maintains internal state data and can handle messages.
//   * Message - An event containing data that drives the behavior of the current state and triggers state transitions
//
//
// State transitions are modeled as the construction of one state from another state. This is a
// typestate driven process, meaning that a transition can only take place if one state can convert
// to another.
//
// In essence a state machine is modeled by a state that handles a single message at a time, mutates
// its internal state data, and returns messages that need to be sent, as well as the next state.
//
// An 'Executor' drives state machines by delivering events.
//
template <typename State, typename Msg, typename MsgContainer>
std::pair<State, MsgContainer<Msg>> handle(Msg&& msg);
