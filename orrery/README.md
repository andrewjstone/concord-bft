# Introduction

Orrery is a C++ framework for reifying a message passing, component based architecture for concord. Rather than past attempts at describing an abstract architecture, the overarching design goals for concord are made concrete with a description of the capabilities and usage of orrery.

# Architectural Values

 * Build a modular system
  * Enable local reasoning
  * Create abstractions that do one thing well
  * Isolate technical debt
  * Allow incremental improvement
  * Enable easy discovery of where to add code
  * Make it easy to test libraries in isolation
  * Make it easier to diagnose and fix bugs
 * Send messsages instead of sharing state
 * Be judicious about dependencies
 * Enable writing components and replicas in other languages

# Abstraction Overview

Orrery is based around a unified set of abstractions to enable robust implementation of concord replicas in C++. Users only implement **components**, which form the high level building blocks of concord, and **messages** which allow interaction between components. All other abstractions are provided by orrery and are usable out of the box.

An example of how to build upon orrery is describe

## Component

A [component](include/orrery/component.h) is the highest level user abstraction in concord. Components
are defined in terms of their message handlers and state. It is appropriate also to document, in
English, the high level behavior of the component and its intention.

Some examples of components are:
 * Replica
 * State Transfer
 * Reconfiguration Manager
 * Execution Engine
 * Crypto (signing)
 * Communication

The primary purpose of a component is to encapsulate logic for a certain subset of the system. As
components interact via message passing, the public interface of a component consists of a set of
message handlers. Components must implement `handle(ComponentId from, ConcreteMsg&&)` methods
only for messages they handle. This requirement is convenient for developers, but also allows
standardization of error handling code in a single place.

An [example of a
component](https://github.com/andrewjstone/concord-bft/blob/orrery/orrery/test/orrery_test.cpp#L30-L44)
can be seen in the orrery test code, and is reproduced here for convenience:

```C++
class StateTransferComponent {
 public:
  StateTransferComponent(World&& world) : world_(std::move(world)) {}
  ComponentId id{ComponentId::state_transfer};

  void handle(ComponentId from, StateTransferMsg&& msg) {
    std::cout << "Handling state transfer msg with id: " << msg.id << std::endl;
    st_msgs_received++;
    world_.send(ComponentId::replica, ConsensusMsg{});
  }
  void handle(ComponentId from, ControlMsg&& msg) { control_msgs_received++; }

 private:
  World world_;
};
```

## Message

Interestingly, Orrery is *not* a generic framework. It was specifically designed for concord, and this is evident in the choice of messages. Orrery uses [cmf](https://github.com/vmware/concord-bft/tree/master/messages) for messages, and defines a [**global** hierarchy of messages for concord](https://github.com/andrewjstone/concord-bft/blob/orrery/orrery/cmf/orrery_msgs.cmf). For now, this hierarchy is full of tubs, but in practice, it will contain all concord messages under a root type of `AllMsgs`, which contains a `oneof` consisting of a top-level message per component, as well as orrery specific messages:

```
Msg AllMsgs 999 {
    oneof {
        ControlMsg
        ConsensusMsg
        StateTransferMsg
        NetworkMsg
        CryptoMsg
        ExecutionEngineMsg
    } msg
}
```

Note that the first message type in the oneof is a `ControlMsg`. This is an orrery specific message that allows control commands to be dispatched to all components and executors. For now, the sole command is `shutdown`.

Components receive and send messages. However, the rest of the orrery infrastructure packs these messages into `Envelope`s, so that they can be properly routed to their destinations.

```
Msg Envelope 10000 {
    ComponentId to
    ComponentId from
    AllMsgs all_msgs
}
```

## Executor

Orrery deliberately separates logic and state from threads of execution. You'll notice that in the discussion of components above, that user code only defined message handlers. Yet, how those handlers were called, and by whom was left unstated. The job of calling component handlers is done by `Executor`s. Executors are statically assigned components at startup, and are in charge of receiving and forwarding messages to their components.

Executors are very simple, and their job is to take messages off a queue and call component handlers. Note that in some cases, executors also must process messages. For example, executors respond to broadcast control messages, such as `shutdown`, but calling the handle method of each of their components, and then returning from their own thread callable.

Executors return a `std::thread` upon a call `start` and this thread should be joined in main, along with all other executor threads. This allows for graceful shutdown.

## Mailbox
## Environment
## World

# Abstraction Implementation
