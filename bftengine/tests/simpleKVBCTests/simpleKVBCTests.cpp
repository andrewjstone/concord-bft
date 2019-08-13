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

#include "simpleKVBCTests.h"
#include <inttypes.h>
#include <map>
#include <set>
#include <list>
#include <chrono>
#include "messages.hpp"

using std::list;
using std::map;
using std::set;

#define NUMBER_OF_KEYS (200)
#define CONFLICT_DISTANCE (49)
#define MAX_WRITES_IN_REQ (7)
#define MAX_READ_SET_SIZE_IN_REQ (10)
#define MAX_READS_IN_REQ (7)

using namespace SimpleKVBC;

#define CHECK(_cond_, _msg_)              \
  if (!(_cond_)) {                        \
    printf("\nTest failed: %s\n", _msg_); \
    assert(_cond_);                       \
  }

namespace SimpleKVBC 

namespace BasicRandomTests {
namespace Internal {
class InternalCommandsHandler : public ICommandsHandler {
 public:
  virtual bool executeCommand(const Slice command,
                              const ILocalKeyValueStorageReadOnly& roStorage,
                              IBlocksAppender& blockAppender,
                              const size_t maxReplySize,
                              char* outReply,
                              size_t& outReplySize) const {
    printf("Got message of size %zu\n", command.size);

    // DEBUG_RNAME("InternalCommandsHandler::executeCommand");
    CHECK((command.size >= sizeof(SimpleRequestHeader)), "small message");
    SimpleRequestHeader* p = (SimpleRequestHeader*)command.data;
    if (p->type != 1) return executeReadOnlyCommand(command, roStorage, maxReplySize, outReply, outReplySize);

    // conditional write
CHECK(command.size >= sizeof(SimpleConditionalWriteHeader), "small message");
    SimpleConditionalWriteHeader* pCondWrite = (SimpleConditionalWriteHeader*)command.data;
    CHECK(command.size >= pCondWrite->size(), "small message");
    SimpleKey* readSetArray = pCondWrite->readSetArray();

    BlockId currBlock = roStorage.getLastBlock();

    // look for conflicts
    bool hasConflict = false;
    for (size_t i = 0; !hasConflict && i < pCondWrite->numberOfKeysInReadSet; i++) {
      Slice key(readSetArray[i].key, KV_LEN);
      roStorage.mayHaveConflictBetween(key, pCondWrite->readVersion + 1, currBlock, hasConflict);
    }

    if (!hasConflict) {
      SimpleKV* keyValArray = pCondWrite->keyValArray();
      SetOfKeyValuePairs updates;

      // printf("\nAdding BlockId=%" PRId64 " ", currBlock + 1);

      for (size_t i = 0; i < pCondWrite->numberOfWrites; i++) {
        Slice key(keyValArray[i].key, KV_LEN);
        Slice val(keyValArray[i].val, KV_LEN);
        KeyValuePair kv(key, val);
        updates.insert(kv);
        // printf("\n");
        // for (int k = 0; k < sizeof(size_t); k++)
        //	printf("%02X", key.data()[k]);
        // printf("%04s", " ");
        // for (int k = 0; k < sizeof(size_t); k++)
        //	printf("%02X", val.data()[k]);
      }
      // printf("\n\n");
      BlockId newBlockId = 0;
      Status addSucc = blockAppender.addBlock(updates, newBlockId);
      assert(addSucc.ok());
      assert(newBlockId == currBlock + 1);
    }

    assert(sizeof(SimpleReplyHeader_ConditionalWrite) <= maxReplySize);
    SimpleReplyHeader_ConditionalWrite* pReply = (SimpleReplyHeader_ConditionalWrite*)outReply;
    memset(pReply, 0, sizeof(SimpleReplyHeader_ConditionalWrite));
    pReply->h.type = 1;
    pReply->succ = (!hasConflict);
    if (!hasConflict)
      pReply->latestBlock = currBlock + 1;
    else
      pReply->latestBlock = currBlock;

    outReplySize = sizeof(SimpleReplyHeader_ConditionalWrite);
    return true;
  }

  virtual bool executeReadOnlyCommand(const Slice command,
                                      const ILocalKeyValueStorageReadOnly& roStorage,
                                      const size_t maxReplySize,
                                      char* outReply,
                                      size_t& outReplySize) const {
    CHECK(command.size >= sizeof(SimpleRequestHeader), "small message");
    SimpleRequestHeader* p = (SimpleRequestHeader*)command.data;
    if (p->type == 2) {
      // read
      CHECK(command.size >= sizeof(SimpleReadHeader), "small message");
      SimpleReadHeader* pRead = (SimpleReadHeader*)command.data;
      CHECK(command.size >= pRead->size(), "small message");
      size_t numOfElements = pRead->numberOfKeysToRead;
      size_t replySize = SimpleReplyHeader_Read::size(numOfElements);

      CHECK(maxReplySize >= replySize, "small message");

      //					printf("\nRead request");  print(p);

      SimpleReplyHeader_Read* pReply = (SimpleReplyHeader_Read*)(outReply);
      outReplySize = replySize;
      memset(pReply, 0, replySize);
      pReply->h.type = 2;
      pReply->numberOfElements = numOfElements;

      SimpleKey* keysArray = pRead->keysArray();
      for (size_t i = 0; i < numOfElements; i++) {
        memcpy(pReply->elements[i].key, keysArray[i].key, KV_LEN);
        Slice val;
        Slice k(keysArray[i].key, KV_LEN);
        BlockId outBlock = 0;
        roStorage.get(pRead->readVersion, k, val, outBlock);
        if (val.size > 0)
          memcpy(pReply->elements[i].val, val.data, KV_LEN);
        else
          memset(pReply->elements[i].val, 0, KV_LEN);
      }

      //					printf("\nRead reply");  print((SimpleReplyHeader*)pReply);

      return true;

    } else if (p->type == 3) {
      // read
      CHECK(command.size >= sizeof(SimpleGetLastBlockHeader), "small message");
      //					SimpleGetLastBlockHeader* pGetLast =
      //(SimpleGetLastBlockHeader*)command.data;

      CHECK(maxReplySize >= sizeof(SimpleReplyHeader_GetLastBlockHeader), "small message");
      SimpleReplyHeader_GetLastBlockHeader* pReply = (SimpleReplyHeader_GetLastBlockHeader*)(outReply);
      outReplySize = sizeof(SimpleReplyHeader_GetLastBlockHeader);
      memset(pReply, 0, sizeof(SimpleReplyHeader_GetLastBlockHeader));
      pReply->h.type = 3;
      pReply->latestBlock = roStorage.getLastBlock();

      return true;
    } else if (p->type == 4) {
      CHECK(command.size >= sizeof(SimpleGetBlockDataHeader), "small message");
      SimpleGetBlockDataHeader* pGetBlock = (SimpleGetBlockDataHeader*)command.data;
      auto block_id = pGetBlock->block_id;
      SetOfKeyValuePairs outBlockData;
      if (!roStorage.getBlockData(block_id, outBlockData).ok()) {
        printf("GetBlockData: Failed to retrieve block %" PRId64, block_id);
        return false;
      }

      auto numOfElements = outBlockData.size();
      size_t replySize = SimpleReplyHeader_Read::size(numOfElements);
      CHECK(maxReplySize >= replySize, "small message");

      SimpleReplyHeader_Read* pReply = (SimpleReplyHeader_Read*)(outReply);
      outReplySize = replySize;
      memset(pReply, 0, replySize);
      pReply->h.type = 2;
      pReply->numberOfElements = numOfElements;

      auto i = 0;
      for (auto kv : outBlockData) {
        memcpy(pReply->elements[i].key, kv.first.data, KV_LEN);
        memcpy(pReply->elements[i].val, kv.second.data, KV_LEN);
        ++i;
      }
      return true;
    }

    else {
      outReplySize = 0;
      CHECK(false, "illegal message");
      return false;
    }
  }
};

static size_t sizeOfReq(SimpleRequestHeader* req) {
  if (req->type == 1) {
    SimpleConditionalWriteHeader* p = (SimpleConditionalWriteHeader*)req;
    return p->size();
  } else if (req->type == 2) {
    SimpleReadHeader* p = (SimpleReadHeader*)req;
    return p->size();
  } else if (req->type == 3) {
    return SimpleGetLastBlockHeader::size();
  } else if (req->type == 4) {
    return SimpleGetBlockDataHeader::size();
  }
  assert(0);
  return 0;
}

static size_t sizeOfRep(SimpleReplyHeader* rep) {
  if (rep->type == 1) {
    return sizeof(SimpleReplyHeader_ConditionalWrite);
  } else if (rep->type == 2) {
    SimpleReplyHeader_Read* p = (SimpleReplyHeader_Read*)rep;
    return p->size();
  } else if (rep->type == 3) {
    return sizeof(SimpleReplyHeader_GetLastBlockHeader);
  }
  assert(0);
  return 0;
}

static void verifyEmptyBlockchain(IClient* client) {
  SimpleGetLastBlockHeader* p = SimpleGetLastBlockHeader::alloc();
  p->h.type = 3;
  Slice command((const char*)p, sizeof(SimpleGetLastBlockHeader));
  Slice reply;

  client->invokeCommandSynch(command, true, reply);

  assert(reply.size == sizeof(SimpleReplyHeader_GetLastBlockHeader));

  SimpleReplyHeader_GetLastBlockHeader* pReplyData = (SimpleReplyHeader_GetLastBlockHeader*)reply.data;
  (void)pReplyData;

  assert(pReplyData->h.type == 3);
  assert(pReplyData->latestBlock == 0);

  client->release(reply);
}
}  // namespace Internal

void run(IClient* client, const size_t numOfOperations) {
  assert(!client->isRunning());

  std::list<Internal::SimpleRequestHeader*> requests;
  std::list<Internal::SimpleReplyHeader*> expectedReplies;

  Internal::InternalTestsBuilder::createRandomTest(
      numOfOperations, 1111, INT64_MIN /* INT64_MAX */, requests, expectedReplies);

  client->start();

  Internal::verifyEmptyBlockchain(client);

  assert(requests.size() == expectedReplies.size());

  int ops = 0;

  while (!requests.empty()) {
#ifndef _WIN32
    if (ops % 100 == 0) usleep(100 * 1000);
#endif
    Internal::SimpleRequestHeader* pReq = requests.front();
    Internal::SimpleReplyHeader* pExpectedRep = expectedReplies.front();
    requests.pop_front();
    expectedReplies.pop_front();

    bool readOnly = (pReq->type != 1);
    size_t expectedReplySize = Internal::sizeOfRep(pExpectedRep);

    Slice command((const char*)pReq, Internal::sizeOfReq(pReq));
    Slice reply;

    client->invokeCommandSynch(command, readOnly, reply);

    bool equiv = (reply.size == expectedReplySize);

    if (equiv) equiv = (memcmp(reply.data, pExpectedRep, expectedReplySize) == 0);

    //			if (!equiv)	{
    //				print(pReq);
    //				print(pExpectedRep);
    //				print((Internal::SimpleReplyHeader*)reply.data());
    //				assert(0);
    //			}

    CHECK(equiv, "actual reply != expected reply");

    if (equiv) {
      ops++;
      if (ops % 20 == 0) printf("\nop %d passed", ops);
    }

    client->release(reply);
  }

  client->stop();

  Internal::InternalTestsBuilder::free(requests, expectedReplies);
}

ICommandsHandler* commandsHandler() { return new Internal::InternalCommandsHandler(); }
}  // namespace BasicRandomTests
