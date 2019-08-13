// Concord
//
// Copyright (c) 2019 VMware, Inc. All Rights Reserved.
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

#include <list>
#include <map>
#include "messages.hpp"
#include "storage/blockchain_interfaces.h"

namespace SimpleKVBC {
namespace Test {

class SimpleKeyBlockIdPair  // Represents <key, blockId>
{
 public:
  const SimpleKey key;
  const concord::storage::BlockId blockId;

  SimpleKeyBlockIdPair(const SimpleKey& simpleKey,
                       concord::storage::BlockId bId)
      : key(simpleKey), blockId(bId) {}

  bool operator<(const SimpleKeyBlockIdPair& other) const {
    int c = memcmp((char*)&this->key, (char*)&other.key, sizeof(SimpleKey));
    if (c == 0)
      return this->blockId > other.blockId;
    else
      return (c < 0);
  }

  bool operator==(const SimpleKeyBlockIdPair& other) const {
    if (this->blockId != other.blockId) return false;
    int c = memcmp((char*)&this->key, (char*)&other.key, sizeof(SimpleKey));
    return (c == 0);
  }
};

typedef std::map<SimpleKeyBlockIdPair, SimpleValue> KeyBlockIdToValueMap;
typedef std::list<SimpleRequest*> RequestsList;
typedef std::list<SimpleReply*> RepliesList;

class TestsBuilder {
 public:
  explicit TestsBuilder(concordlogger::Logger& logger,
                        concord::storage::IClient& client);
  ~TestsBuilder();

  static size_t sizeOfRequest(SimpleRequest* req);
  static size_t sizeOfReply(SimpleReply* rep);

  void createRandomTest(size_t numOfRequests, size_t seed);
  RequestsList getRequests() { return requests_; }
  RepliesList getReplies() { return replies_; }

 private:
  void create(size_t numOfRequests, size_t seed);
  void createAndInsertRandomConditionalWrite();
  void createAndInsertRandomRead();
  void createAndInsertGetLastBlock();
  void addExpectedWriteReply(bool foundConflict);
  bool lookForConflicts(concord::storage::BlockId readVersion,
                        size_t numOfKeysInReadSet, SimpleKey* readKeysArray);
  void addNewBlock(size_t numOfWrites, SimpleKV* writesKVArray);
  void retrieveExistingBlocksFromKVB();
  concord::storage::BlockId getInitialLastBlockId();

 private:
  concordlogger::Logger& logger_;
  concord::storage::IClient& client_;
  RequestsList requests_;
  RepliesList replies_;
  std::map<concord::storage::BlockId, SimpleBlock*> internalBlockchain_;
  KeyBlockIdToValueMap allKeysToValueMap_;
  concord::storage::BlockId prevLastBlockId_ = 0;
  concord::storage::BlockId lastBlockId_ = 0;
};

}
}
