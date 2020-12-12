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

#include "categorization/merkle_category.h"
#include "categorization/column_families.h"
#include "categorization/details.h"

#include "assertUtils.hpp"
#include "kv_types.hpp"
#include "sha_hash.hpp"

using concord::storage::rocksdb::NativeWriteBatch;

namespace concord::kvbc::categorization::detail {

using Hasher = concord::util::SHA3_256;
using Hash = Hasher::Digest;

MerkleBlockValue hashUpdate(const MerkleUpdatesData& update) {
  MerkleBlockValue value;
  value.hashed_added_keys.resize(update.kv.size());
  value.hashed_deleted_keys.resize(update.kv.size());

  // root_hash = h((h(k1) || h(v1)) || ... || (h(kN) || h(vN) || h(dk1) || ... || h(dkN))
  auto root_hasher = Hasher{};
  root_hasher.init();
  auto kv_hasher = Hasher{};

  // Hash all keys and values as part of the root hash
  for (auto& [k, v] : update.kv) {
    auto key_hash = kv_hasher.digest(k.data(), k.size());
    auto val_hash = kv_hasher.digest(v.data(), v.size());
    value.hashed_added_keys.push_back(KeyHash{key_hash});
    root_hasher.update(key_hash.data(), key_hash.size());
    root_hasher.update(val_hash.data(), val_hash.size());
  }

  for (auto& k : update.deletes) {
    auto key_hash = kv_hasher.digest(k.data(), k.size());
    value.hashed_deleted_keys.push_back(KeyHash{key_hash});
    root_hasher.update(key_hash.data(), key_hash.size());
  }
  value.root_hash = root_hasher.finish();
  return value;
}

MerkleCategory::MerkleCategory(const std::shared_ptr<storage::rocksdb::NativeClient>& db)
    : db_{db}, tree_{std::make_shared<Reader>(*db_)} {
  createColumnFamilyIfNotExisting(MERKLE_INTERNAL_NODES_CF, *db);
  createColumnFamilyIfNotExisting(MERKLE_LEAF_NODES_CF, *db);
  createColumnFamilyIfNotExisting(MERKLE_STALE_CF, *db);
  createColumnFamilyIfNotExisting(MERKLE_KEY_VERSIONS_CF, *db);
  createColumnFamilyIfNotExisting(MERKLE_KEYS_CF, *db);
}

MerkleUpdatesInfo MerkleCategory::add(BlockId block_id, MerkleUpdatesData&& update, NativeWriteBatch& batch) {
  // TODO: The sparse merkle tree should take maps and strings not unordered_maps and slivers.

  return MerkleUpdatesInfo{};
}

std::optional<Value> MerkleCategory::get(const std::string& key, BlockId block_id) const { return std::nullopt; }

std::optional<Value> MerkleCategory::getUntilBlock(const std::string& key, BlockId max_block_id) const {
  return std::nullopt;
}

std::optional<Value> getLatest(const std::string& key) { return std::nullopt; }

bool keyExists(const std::string& key, BlockId start, BlockId end) { return true; }

}  // namespace concord::kvbc::categorization::detail
