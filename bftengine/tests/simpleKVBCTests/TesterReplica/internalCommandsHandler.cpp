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

#include "internalCommandsHandler.hpp"
#include <assert.h>
#include "hash_defs.h"
#include "storage/concord_metadata_storage.h"

using namespace BasicRandomTests;

using concord::consensus::Status;
using concord::storage::BlockId;
using concord::storage::ConcordMetadataStorage;
using concord::storage::SetOfKeyValuePairs;

const auto KEY_TYPE = concord::storage::EDBKeyType::E_DB_KEY_TYPE_KEY;

int InternalCommandsHandler::execute(uint16_t clientId, uint64_t sequenceNum,
                                     bool readOnly, uint32_t requestSize,
                                     const char *request, uint32_t maxReplySize,
                                     char *outReply,
                                     uint32_t &outActualReplySize) {
  int res;
  if (requestSize < sizeof(SimpleRequest)) {
    LOG_ERROR(m_logger, "The message is too small: requestSize is "
                            << requestSize << ", required size is "
                            << sizeof(SimpleRequest));
    return -1;
  }
  if (readOnly) {
    res = executeReadOnlyCommand(requestSize, request, maxReplySize, outReply,
                                 outActualReplySize);
  } else {
    res = executeWriteCommand(requestSize, request, sequenceNum, maxReplySize,
                              outReply, outActualReplySize);
  }
  if (!res) LOG_ERROR(m_logger, "Command execution failed!");
  return res ? 0 : -1;
}

void InternalCommandsHandler::addMetadataKeyValue(SetOfKeyValuePairs &updates,
                                                  uint64_t sequenceNum) const {
  ConcordMetadataStorage cmStorage(*m_storage);
  Sliver metadataKey = cmStorage.BlockMetadataKey();
  Sliver metadataValue = cmStorage.SerializeBlockMetadata(sequenceNum);
  updates.insert(KeyValuePair(metadataKey, metadataValue));
}

Sliver InternalCommandsHandler::buildSliverFromStaticBuf(char *buf) {
  char *newBuf = new char[KV_LEN];
  memcpy(newBuf, buf, KV_LEN);
  return Sliver(newBuf, KV_LEN);
}

bool InternalCommandsHandler::verifyWriteCommand(
    uint32_t requestSize, const SimpleCondWriteRequest &request,
    size_t maxReplySize, uint32_t &outReplySize) const {
  if (requestSize < sizeof(SimpleCondWriteRequest)) {
    LOG_ERROR(m_logger, "The message is too small: requestSize is "
                            << requestSize << ", required size is "
                            << sizeof(SimpleCondWriteRequest));
    return false;
  }
  if (requestSize < sizeof(request)) {
    LOG_ERROR(m_logger, "The message is too small: requestSize is "
                            << requestSize << ", required size is "
                            << sizeof(request));
    return false;
  }
  if (maxReplySize < outReplySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize=" << outReplySize
                                                           << ", maxReplySize="
                                                           << maxReplySize);
    return false;
  }
  return true;
}

bool InternalCommandsHandler::executeWriteCommand(
    uint32_t requestSize, const char *request, uint64_t sequenceNum,
    size_t maxReplySize, char *outReply, uint32_t &outReplySize) {
  auto *writeReq = (SimpleCondWriteRequest *)request;
  LOG_INFO(m_logger, "Execute WRITE command: type="
                         << writeReq->header.type << ", numOfWrites="
                         << writeReq->numOfWrites << ", numOfKeysInReadSet="
                         << writeReq->numOfKeysInReadSet
                         << ", readVersion = " << writeReq->readVersion);
  bool result =
      verifyWriteCommand(requestSize, *writeReq, maxReplySize, outReplySize);
  if (!result) assert(0);

  SimpleKey *readSetArray = writeReq->readSetArray();
  BlockId currBlock = m_storage->getLastBlock();

  // Look for conflicts
  bool hasConflict = false;
  for (int i = 0; !hasConflict && i < writeReq->numOfKeysInReadSet; i++) {
    m_storage->mayHaveConflictBetween(
        buildSliverFromStaticBuf(readSetArray[i].key),
        writeReq->readVersion + 1, currBlock, hasConflict);
  }

  if (!hasConflict) {
    SimpleKV *keyValArray = writeReq->keyValueArray();
    SetOfKeyValuePairs updates;
    for (int i = 0; i < writeReq->numOfWrites; i++) {
      KeyValuePair keyValue(
          buildSliverFromStaticBuf(keyValArray[i].simpleKey.key),
          buildSliverFromStaticBuf(keyValArray[i].simpleValue.value));
      updates.insert(keyValue);
    }
    addMetadataKeyValue(updates, sequenceNum);
    BlockId newBlockId = 0;
    Status addSuccess = m_blocksAppender->addBlock(updates, newBlockId);
    assert(addSuccess.isOK());
    assert(newBlockId == currBlock + 1);
  }

  assert(sizeof(SimpleReply_ConditionalWrite) <= maxReplySize);
  auto *reply = (SimpleReply_ConditionalWrite *)outReply;
  reply->header.type = COND_WRITE;
  reply->success = (!hasConflict);
  if (!hasConflict)
    reply->latestBlock = currBlock + 1;
  else
    reply->latestBlock = currBlock;

  outReplySize = sizeof(SimpleReply_ConditionalWrite);
  ++m_writesCounter;
  LOG_INFO(m_logger, "ConditionalWrite message handled; writesCounter="
                         << m_writesCounter);
  return true;
}

bool InternalCommandsHandler::executeReadCommand(uint32_t requestSize,
                                                 const char *request,
                                                 size_t maxReplySize,
                                                 char *outReply,
                                                 uint32_t &outReplySize) {
  auto *readReq = (SimpleReadRequest *)request;
  LOG_INFO(m_logger, "Execute READ command: type="
                         << readReq->header.type << ", numberOfKeysToRead="
                         << readReq->numberOfKeysToRead
                         << ", readVersion=" << readReq->readVersion);

  auto minRequestSize = MAX(sizeof(SimpleReadRequest), readReq->getSize());
  if (requestSize < minRequestSize) {
    LOG_ERROR(m_logger, "The message is too small: requestSize="
                            << requestSize
                            << ", minRequestSize=" << minRequestSize);
    return false;
  }
  size_t numOfItems = readReq->numberOfKeysToRead;
  size_t replySize = SimpleReply_Read::getSize(numOfItems);

  if (maxReplySize < replySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize="
                            << replySize << ", maxReplySize=" << maxReplySize);
    return false;
  }

  auto *reply = (SimpleReply_Read *)(outReply);
  outReplySize = replySize;
  reply->header.type = READ;
  reply->numOfItems = numOfItems;

  SimpleKey *readKeys = readReq->keys;
  SimpleKV *replyItems = reply->items;
  for (size_t i = 0; i < numOfItems; i++) {
    memcpy(replyItems->simpleKey.key, readKeys->key, KV_LEN);
    Sliver value;
    BlockId outBlock = 0;
    m_storage->get(readReq->readVersion,
                   buildSliverFromStaticBuf(readKeys->key), value, outBlock);
    if (value.length() > 0)
      memcpy(replyItems->simpleValue.value, value.data(), KV_LEN);
    else
      memset(replyItems->simpleValue.value, 0, KV_LEN);
    ++readKeys;
    ++replyItems;
  }
  ++m_readsCounter;
  LOG_INFO(m_logger, "READ message handled; readsCounter=" << m_readsCounter);
  return true;
}

bool InternalCommandsHandler::executeGetBlockDataCommand(uint32_t requestSize,
                                                 const char *request,
                                                 size_t maxReplySize,
                                                 char *outReply,
                                                 uint32_t &outReplySize) {

  auto *req = (SimpleGetBlockDataRequest*)request;
  LOG_INFO(m_logger, "Execute GET_BLOCK_DATA command: type="
                         << req->header.type << ", block_id="
                         << req->block_id);

  auto minRequestSize = MAX(sizeof(SimpleGetBlockDataRequest), req->getSize());
  if (requestSize < minRequestSize) {
    LOG_ERROR(m_logger, "The message is too small: requestSize="
                            << requestSize
                            << ", minRequestSize=" << minRequestSize);
    return false;
  }

  auto block_id = req->block_id;

  SetOfKeyValuePairs outBlockData;
  if (!roStorage.getBlockData(block_id, outBlockData).ok()) {
    LOG_ERROR(m_logger, "Failed to retrieve block " << block_id);
    return false;
  }

  auto numOfElements = outBlockData.size();
  size_t replySize = SimpleReply_Read::getSize(numOfElements);
  if (maxReplySize < replySize) {
    LOG_ERROR(m_logger, "replySize is too big: replySize="
                        << replySize << ", maxReplySize=" << maxReplySize);
    return false;
  }

  auto* reply = (SimpleReply_Read*)(outReply);
  outReplySize = replySize;
  memset(pReply, 0, replySize);
  reply->header.type = READ;
  reply->numOfItems= numOfElements;

  auto i = 0;
  for (auto kv : outBlockData) {
    memcpy(reply->elements[i].key, kv.first.data, KV_LEN);
    memcpy(reply->elements[i].val, kv.second.data, KV_LEN);
    ++i;
  }
  return true;
}

bool InternalCommandsHandler::executeGetLastBlockCommand(
    uint32_t requestSize, size_t maxReplySize, char *outReply,
    uint32_t &outReplySize) {
  if (requestSize < sizeof(SimpleGetLastBlockRequest)) {
    LOG_ERROR(m_logger, "The message is too small: requestSize is "
                            << requestSize << ", required size is "
                            << sizeof(SimpleGetLastBlockRequest));
    return false;
  }

  outReplySize = sizeof(SimpleReply_GetLastBlock);
  if (maxReplySize < outReplySize) {
    LOG_ERROR(m_logger, "maxReplySize is too small: replySize="
                            << outReplySize
                            << ", maxReplySize=" << maxReplySize);
    return false;
  }

  auto *reply = (SimpleReply_GetLastBlock *)(outReply);
  reply->header.type = GET_LAST_BLOCK;
  reply->latestBlock = m_storage->getLastBlock();
  ++m_getLastBlockCounter;
  LOG_INFO(m_logger, "GetLastBlock message handled; getLastBlockCounter="
                         << m_getLastBlockCounter
                         << ", latestBlock=" << reply->latestBlock);
  return true;
}

bool InternalCommandsHandler::executeReadOnlyCommand(uint32_t requestSize,
                                                     const char *request,
                                                     size_t maxReplySize,
                                                     char *outReply,
                                                     uint32_t &outReplySize) {
  auto *requestHeader = (SimpleRequest *)request;
  if (requestHeader->type == READ) {
    return executeReadCommand(requestSize, request, maxReplySize, outReply,
                              outReplySize);
  } else if (requestHeader->type == GET_LAST_BLOCK) {
    return executeGetLastBlockCommand(requestSize, maxReplySize, outReply,
                                      outReplySize);
  } else if (requestHeader->type == GET_BLOCK_DATA) {
    return executerGetBlockDataCommand(requestSize, request, maxReplySize, outReply, outreplySize);
  } else {
    outReplySize = 0;
    LOG_ERROR(m_logger, "Illegal message received: requestHeader->type="
                            << requestHeader->type);
    return false;
  }
}
