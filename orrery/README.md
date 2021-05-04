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

# Abstractions

Orrery is based around a unified set of abstractions to enable robust implementation of concord replicas in C++. Users only implement **components**, which form the high level building blocks of concord, and **messages** which allow interaction between components. All other abstractions are provided by orrery and are usable out of the box.

The complete set of abstractions included in an orrery based system is listed below. The following subsections describe each of these at a high level, followed by a justification for each and implementation details where necesssary.

 * Component
 * Message
 * Executor
 * Environment
 * Mailbox
 * World

## Component

### Overview
A [component](include/orrery/component.h) is the highest level user abstraction in concord. Components
are defined in terms of their message handlers and state. It is appropriate also to document, in
English, the high level behavior of the component and its intention.

Some examples of components are:
 * Consensus (ReplicaImp)
 * State Transfer
 * Reconfiguration Manager
 * Execution Engine
 * Crypto (signing)
 * Communication (networking)

The primary purpose of a component is to encapsulate logic for a certain subset code in a concord replica. The logic for a component should live on a natural boundary in terms of functionality or purpose that is clear to the builders and users of concord. There are a fixed number of components defined statically in main. New components can only be added to the system at build time, as necessary.

Components interact via message passing. The public interface of a component consists of a set of
message handlers. Components must implement `handle(ComponentId from, ConcreteMsg&&)` methods
only for messages they handle, where `ConcreteMsg` is a struct generated from a CMF message. Components send messages to other components via `worlds` as shown in the example below.

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

### Rationale
* Why fixed components?
  * Most consensus based systems, including concord, have a concise set of high-level abstractions that remain mostly fixed over the life of the product. Specifying these explicitly, and reifying them as types allows for many of the architectural values related to modularity.
  * Implementation becomes easier and more efficient with a fixed set of components.
  * Glancing at the instantiation point of the system (main) shows exactly which components are in use for a given replica.

* Why message handlers for specific messages only?
  * Components become smaller with less code duplication.
  * It's easy to see what messages a component can handle by glancing at the interface.
  * Unhandled messages can have handlers automatically generated with a standard error reporting mechanism.

### Implementation Details

 A component is actually the `ComponentImpl` template parameter in the [linked implementation](include/orrery/component.h). As described in the next section, only a top-level `AllMsgs` type is passed between components. However, this variant type is destructured automatically into a specific message that a component can handle. If a component cannot handle a given sub-message an error handler is automatically generated.


## Message

Interestingly, Orrery is *not* a generic framework. It was specifically designed for concord, and this is evident in the choice of messages. Orrery uses [cmf](https://github.com/vmware/concord-bft/tree/master/messages) for messages, and defines a [**global** hierarchy of messages for concord](https://github.com/andrewjstone/concord-bft/blob/orrery/orrery/cmf/orrery_msgs.cmf). For now, this hierarchy is full of stubs, but in practice, it will contain all concord messages under a root type of `AllMsgs`, which contains a `oneof` consisting of a top-level message per component, as well as orrery specific messages:

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

### Rationale
* Why a global hierarchy ?
  * A concord is a closed system with a fixed number of components. We know a-priori these components and their corresponding messages, and so we can define them all in a closed sum-type.
  * C++ is a strongly typed language. It's easier and safer to implement message passing where all messages are of a single type.
  * The components themselves do not need to know or care about all messages. They just need to use and handle messages they care about. So the closed type is not detrimental.

## Executor
Orrery deliberately separates logic and state from threads of execution. You'll notice that in the discussion of components above, that user code only defined message handlers. Yet, how those handlers were called, and by whom was left unstated. The job of calling component handlers is done by `Executor`s. Components are statically assigned to executors at startup, and executors are in charge of receiving and forwarding messages to their respective components.

An executor's primary job is to take messages off a queue and call component handlers.  Note that
in some cases, executors also must process messages.  For example, executors respond to broadcast
control messages, such as `shutdown`, by calling the handle method of each of their components,
and then returning from their own thread callable.

Executors return a `std::thread` upon a call to `start` and this thread should be joined in main, along with all other executor threads. This allows for graceful shutdown.

### Rationale
* Why statically assign components to executors?
  * It's much simpler than building a work stealing scheduler.
  * Components are very granular, with relatively stable performance characteristics. Migrating them would be overkill. In all cases it should be clear whether a component should live in its own Executor or not, or if it is light weight enough to share an executor.
  * We need an incremental mechanism to allow us to immediately start refactoring code a runtime modularity. As all of our code is currently single threaded or operates using thread pools, we can direclty migrgate it to this architecture without more upfront work.
  * C++ concurrency is still very much a work in progress. In the long-term future we may want to use that internally to a component itself, but it comes with tradeoffs. The [C++ Unified Executors Proposal](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0443r12.html) gives a nice declarative interface, but leaves implementation details opaque. Since our code is full of state machines and cryptographic computation, and uses existing libraries such as RocksDB, which is threaded, it's unclear how much value such a thing would have.
  * If we wanted to write code in other languages via the use of proxy components, they would have their own production grade schedulers that we could utilize.

## Mailbox


## Environment
## World

# Component Scenarios
## Thread pool
## IPC
