// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the
// "License").  You may not use this product except in compliance with the
// Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#ifdef USE_ROCKSDB

#pragma once

#include "rocksdb/native_client.h"

namespace concord::kvbc::v3MerkleTree::KeyFamily {

static const std::string NAME = std::string("v3_keys");

// Create a column family for storing blocks if one does not exist.
void create(storage::rocksdb::NativeClient* db) {
  try {
    db->createColumnFamily(NAME);
  } catch (concord::storage::rocksdb::RocksDBException) {
  }
};

std::string serializeKey(Sliver key, BlockId id) { return key.toString() + concordUtils::toBigEndianStringBuffer(id); }

std::pair<Sliver, BlockId> deserializeKey(const Sliver& sliver) {
  auto key = sliver.subsliver(0, sliver.length() - sizeof(BlockId));
  auto blockId = concordUtils::fromBigEndianBuffer<BlockId>(sliver.data() + key.size());
  return std::make_pair(key, blockId);
}

}  // namespace concord::kvbc::v3MerkleTree::KeyFamily
#endif
