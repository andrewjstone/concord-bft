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

#ifdef USE_ROCKSDB

#include "Logger.hpp"
#include "rocksdb/native_client.h"
#include "sparse_merkle/tree.h"

#include "base_types.h"
#include "categorized_kvbc_msgs.cmf.hpp"
#include "details.h"

namespace concord::kvbc::categorization::detail {

// This category puts only block relevant information into the sparse merkle tree. This drastically
// reduces the storage load and merkle tree overhead, but still allows the same proof guarantees.
// The `key` going into the merkle tree is the block version, while the value consists of:
//     * The root hash of the merkle provable keys and values for the block
//     * The hash of each provable key in the block.
//
// The latter is necessary for maintaining proof guarantees after a block is pruned.
class MerkleCategory {
 public:
  MerkleCategory(const std::shared_ptr<storage::rocksdb::NativeClient>&);

  // Add the given block updates and return the information that needs to be persisted in the block.
  MerkleUpdatesInfo add(BlockId block_id, MerkleUpdatesData&& update, storage::rocksdb::NativeWriteBatch&);

  // Return the value of `key` at `block_id`.
  // Return std::nullopt if the key doesn't exist at `block_id`.
  std::optional<Value> get(const std::string& key, BlockId block_id) const;
  std::optional<Value> get(const Hash& hashed_key, BlockId block_id) const;

  // Return the value of `key` at its most recent block version.
  // Return std::nullopt if the key doesn't exist.
  std::optional<Value> getLatest(const std::string& key) const;

  // Returns the latest *block* version of a key.
  //
  // This is useful for fast conflict detection.
  BlockId getLatestVersion(const std::string& key) const;
  BlockId getLatestVersion(const Hash& key) const;

 private:
  std::map<Hash, KeyVersions> getKeyVersions(const std::vector<KeyHash>& added,
                                             const std::vector<KeyHash>& deleted) const;

  void putKeyVersions(storage::rocksdb::NativeWriteBatch& batch, const std::map<Hash, KeyVersions>& key_versions);

  StaleKeys putKeys(storage::rocksdb::NativeWriteBatch& batch,
                    uint64_t block_id,
                    const std::vector<KeyHash>& hashed_added_keys,
                    const std::vector<KeyHash>& hashed_deleted_keys,
                    MerkleUpdatesData& updates,
                    std::map<Hash, KeyVersions>& versions);

  void putMerkleNodes(storage::rocksdb::NativeWriteBatch& batch,
                      sparse_merkle::UpdateBatch&& update_batch,
                      uint64_t tree_version);

 private:
  class Reader : public sparse_merkle::IDBReader {
   public:
    Reader(const storage::rocksdb::NativeClient& db) : db_{db} {}

    // Return the latest root node in the system.
    sparse_merkle::BatchedInternalNode get_latest_root() const override;

    // Retrieve a BatchedInternalNode given an InternalNodeKey.gguuuuu
    //
    // Throws a std::out_of_range exception if the internal node does not exist.
    sparse_merkle::BatchedInternalNode get_internal(const sparse_merkle::InternalNodeKey&) const override;

   private:
    // The lifetime of this reference is shorter than the lifetime of the tree which is shorter than
    // the lifetime of the category.
    const storage::rocksdb::NativeClient& db_;
  };

 private:
  const std::shared_ptr<storage::rocksdb::NativeClient> db_;

  logging::Logger logger_;
  sparse_merkle::Tree tree_;
};

}  // namespace concord::kvbc::categorization::detail

#endif
