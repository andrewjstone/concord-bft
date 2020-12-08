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

namespace concord::kvbc::v3MerkleTree::BlockFamily {

static const std::string NAME = std::string("v3_blocks");

// Create a column family for storing blocks if one does not exist.
void create(storage::rocksdb::NativeClient* db) {
  try {
    db->createColumnFamily(NAME);
  } catch (concord::storage::rocksdb::RocksDBException) {
  }
};

std::string serializeKey(BlockId id) { return concordUtils::toBigEndianStringBuffer(id); }

BlockId deserializeKey(const char* key) { return concordUtils::fromBigEndianBuffer<BlockId>(key); }

}  // namespace concord::kvbc::v3MerkleTree::BlockFamily
#endif
