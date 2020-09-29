#include "messages.hpp"
// Msgs a type representing all possible messages that any state can handle
// MsgContainer is where outgoing messages are placed upon state transition
// States is a std::variant containing all possible states of the state machine
//
// All the parameterized types must be the same for every state in a given state machine, so that
// the executor can transition acrosss states.
template <typename MsgsContainer, typename States>
class PrePrepare {
 public:
  PrePrepare(PrimaryCtx&& ctx) : ctx_(std::move(ctx)) {}
  States handle(Msgs&& msg, MsgsContainer& output) {}

 private:
  PrimaryCtx ctx_;
};
