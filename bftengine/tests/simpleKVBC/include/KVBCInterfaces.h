// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include "status.hpp"
#include "ReplicaConfig.hpp"
#include "ICommunication.hpp"
#include "Metrics.hpp"
#include "blockchain_db_interfaces.h"

namespace SimpleKVBC {

using concordUtils::Status;
using concordUtils::Sliver;

// forward declarations
class ICommandsHandler;

struct ClientConfig
{
  // F value - max number of faulty/malicious replicas. fVal >= 1
  uint16_t fVal;

  // C value. cVal >=0
  uint16_t cVal;

  // unique identifier of the client.
  // clientId should also represent this client in ICommunication.
  // In the current version, replicaId should be a number between N and
  // N+numOfClientProxies-1 (N is the number replicas in the system.
  // numOfClientProxies is part of the replicas' configuration)
  uint16_t clientId;

  // maximum reply size supported by this client
  uint32_t maxReplySize;
};

/////////////////////////////////////////////////////////////////////////////
// Client proxy
/////////////////////////////////////////////////////////////////////////////

// Represents a client of the blockchain database
class IClient {
 public:
  virtual Status start() = 0;
  virtual Status stop() = 0;

  virtual bool isRunning() = 0;

  virtual Status invokeCommandSynch(const Sliver command, bool isReadOnly, Sliver& outReply) = 0;

  virtual Status release(Sliver& slice) = 0;  // release memory allocated by invokeCommandSynch
};

// creates a new Client object
IClient* createClient(const ClientConfig& conf, bftEngine::ICommunication* comm);

// TODO: Implement:
//  // deletes a Client object
//  void release(IClient* r);

/////////////////////////////////////////////////////////////////////////////
// Replica
/////////////////////////////////////////////////////////////////////////////

// Represents a replica of the blockchain database
class IReplica {
 public:
  virtual Status start() = 0;
  virtual Status stop() = 0;
  virtual ~IReplica() {}

  enum class RepStatus  // status of the replica
  { UnknownError = -1,
    Ready = 0,
    Starting,
    Running,
    Stopping,
  };

  // returns the current status of the replica
  virtual RepStatus getReplicaStatus() const = 0;

  virtual bool isRunning() const = 0;

  // TODO: Implement:
  //      // this callback is called by the library every time the replica status is changed
  //      typedef void(*StatusNotifier)(RepStatus newStatus);
  //      virtual Status setStatusNotifier(StatusNotifier statusNotifier);
  //
  //      // Used to update the local storage (may be needed for initializing and maintenance).
  //      // Can only be used when the replica's status is Idle.
  //      virtual const ILocalKeyValueStorageReadOnly& getReadOnlyStorage() = 0;
  //      virtual Status addBlockToIdleReplica(const SetOfKeyValuePairs& updates) = 0; // write the updates by adding a
  //      block
};

// creates a new Replica object
IReplica* createReplica(const bftEngine::ReplicaConfig& conf,
                        bftEngine::ICommunication* comm,
                        ICommandsHandler* _cmdHandler,
                        std::shared_ptr<concordMetrics::Aggregator> aggregator);

// TODO: Implement:
//  // deletes a Replica object
//  void release(IReplica* r);

/////////////////////////////////////////////////////////////////////////////
// Replica's commands handle
/////////////////////////////////////////////////////////////////////////////

class ICommandsHandler {
 public:
  virtual bool executeCommand(const Sliver command,
                              const concord::storage::blockchain::ILocalKeyValueStorageReadOnly& roStorage,
                              concord::storage::blockchain::IBlocksAppender& blockAppender,
                              const size_t maxReplySize,
                              char* outReply,
                              size_t& outReplySize) const = 0;

  virtual bool executeReadOnlyCommand(const Sliver command,
                                      const concord::storage::blockchain::ILocalKeyValueStorageReadOnly& roStorage,
                                      const size_t maxReplySize,
                                      char* outReply,
                                      size_t& outReplySize) const = 0;
};
}  // namespace SimpleKVBC
