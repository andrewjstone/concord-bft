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

#pragma once

#include "rocksdb/native_client.h"

namespace concord::kvbc::v2MerkleTree {

// Take an UpdateBatch returned from a merkle tree and write it efficiently into storage.
class MerkleTreeBatchWriter {
 public:
  MerkleTreeBatchWriter(std::shared_ptr<concord::storage::rocksdb::NativeClient>& client) : client_(client) {}

 private:
  std::shared_ptr<concord::storage::rocksdb::NativeClient> client_;
};

// Provide a Merkle Teee DBReader interface that is capable of reading data written with the MerkleTreeBatchWriter.
class MerkleTreeBatchReader {};

};  // namespace concord::kvbc::v2MerkleTree
