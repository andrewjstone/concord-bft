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
using concordUtils::Sliver;

namespace concord::kvbc::categorization::detail {

MerkleBlockValue hashUpdate(const MerkleUpdatesData& updates) {
  MerkleBlockValue value;
  value.hashed_added_keys.resize(updates.kv.size());
  value.hashed_deleted_keys.resize(updates.kv.size());

  // root_hash = h((h(k1) || h(v1)) || ... || (h(kN) || h(vN) || h(dk1) || ... || h(dkN))
  auto root_hasher = Hasher{};
  root_hasher.init();
  auto kv_hasher = Hasher{};

  // Hash all keys and values as part of the root hash
  for (const auto& [k, v] : updates.kv) {
    auto key_hash = kv_hasher.digest(k.data(), k.size());
    auto val_hash = kv_hasher.digest(v.data(), v.size());
    value.hashed_added_keys.push_back(KeyHash{key_hash});
    root_hasher.update(key_hash.data(), key_hash.size());
    root_hasher.update(val_hash.data(), val_hash.size());
  }

  for (const auto& k : updates.deletes) {
    auto key_hash = kv_hasher.digest(k.data(), k.size());
    value.hashed_deleted_keys.push_back(KeyHash{key_hash});
    root_hasher.update(key_hash.data(), key_hash.size());
  }
  value.root_hash = root_hasher.finish();
  return value;
}

MerkleUpdatesInfo updatesDataToUpdatesInfo(const MerkleUpdatesData& updates) {
  MerkleUpdatesInfo info{};
  for (const auto& kv : updates.kv) {
    info.keys.emplace(kv.first, MerkleKeyFlag{false});
  }
  for (const auto& key : updates.deletes) {
    info.keys.emplace(key, MerkleKeyFlag{true});
  }
  return info;
}

MerkleCategory::MerkleCategory(const std::shared_ptr<storage::rocksdb::NativeClient>& db)
    : db_{db}, tree_{std::make_shared<Reader>(*db_)} {
  createColumnFamilyIfNotExisting(MERKLE_INTERNAL_NODES_CF, *db);
  createColumnFamilyIfNotExisting(MERKLE_LEAF_NODES_CF, *db);
  createColumnFamilyIfNotExisting(MERKLE_STALE_CF, *db);
  createColumnFamilyIfNotExisting(MERKLE_KEY_VERSIONS_CF, *db);
  createColumnFamilyIfNotExisting(MERKLE_KEYS_CF, *db);
}

MerkleUpdatesInfo MerkleCategory::add(BlockId block_id, MerkleUpdatesData&& updates, NativeWriteBatch& batch) {
  auto merkle_value = hashUpdate(updates);
  auto root_hash = merkle_value.root_hash;
  auto key_versions = getKeyVersions(merkle_value.hashed_added_keys, merkle_value.hashed_deleted_keys);
  auto stale_keys =
      putKeys(batch, block_id, merkle_value.hashed_added_keys, merkle_value.hashed_deleted_keys, updates, key_versions);

  auto block_key = serialize(BlockKey{block_id});
  auto merkle_key = Sliver{(const char*)block_key.data(), block_key.size()};
  auto ser_value = serialize(merkle_value);
  auto ser_value_sliver = Sliver{(const char*)ser_value.data(), ser_value.size()};
  auto tree_update_batch = tree_.update(SetOfKeyValuePairs{{merkle_key, ser_value_sliver}});

  auto tree_version = tree_update_batch.stale.stale_since_version.value();
  putStale(batch, block_key, std::move(tree_update_batch.stale), std::move(stale_keys));
  putMerkleNodes(batch, std::move(tree_update_batch));
  putKeyVersions(batch, key_versions);

  auto info = updatesDataToUpdatesInfo(updates);
  info.root_hash = root_hash;
  info.state_root_version = tree_version;
  return info;
}

std::optional<Value> MerkleCategory::get(const std::string& key, BlockId block_id) const { return std::nullopt; }

std::optional<Value> MerkleCategory::getUntilBlock(const std::string& key, BlockId max_block_id) const {
  return std::nullopt;
}

std::optional<Value> getLatest(const std::string& key) { return std::nullopt; }

bool keyExists(const std::string& key, BlockId start, BlockId end) { return true; }

// TODO: Use multiget once its implemented in NativeClient
std::map<Hash, KeyVersions> MerkleCategory::getKeyVersions(std::vector<KeyHash>& added_keys,
                                                           std::vector<KeyHash>& deleted_keys) {
  // Writing sorted keys to rocksdb is faster than unsorted
  std::map<Hash, KeyVersions> versions;
  for (auto& key : added_keys) {
    auto key_versions = KeyVersions{};
    const auto serialized = db_->get(MERKLE_KEY_VERSIONS_CF, key.value);
    if (serialized) {
      deserialize(*serialized, key_versions);
    }
    versions.emplace(key.value, key_versions);
  }
  for (auto& key : deleted_keys) {
    auto key_versions = KeyVersions{};
    const auto serialized = db_->get(MERKLE_KEY_VERSIONS_CF, key.value);
    if (serialized) {
      deserialize(*serialized, key_versions);
    }
    versions.emplace(key.value, key_versions);
  }
  return versions;
}

void MerkleCategory::putKeyVersions(NativeWriteBatch& batch, const std::map<Hash, KeyVersions>& key_versions) {
  for (const auto& [hash, versions] : key_versions) {
    batch.put(MERKLE_KEY_VERSIONS_CF, hash, serialize(versions));
  }
}

StaleKeys MerkleCategory::putKeys(NativeWriteBatch& batch,
                                  uint64_t block_id,
                                  const std::vector<KeyHash>& hashed_added_keys,
                                  const std::vector<KeyHash>& hashed_deleted_keys,
                                  MerkleUpdatesData& updates,
                                  std::map<Hash, KeyVersions>& versions) {
  StaleKeys stale_keys;
  auto kv_it = updates.kv.begin();
  for (auto key_it = hashed_added_keys.begin(); key_it != hashed_added_keys.end(); key_it++) {
    // Write the versioned key/value pair used for direct key lookup
    batch.put(MERKLE_KEYS_CF, serialize(VersionedKey{*key_it, block_id}), std::move(kv_it->second));

    // Is there a version that was just overwritten? If so, we need to create a stale key.
    auto key_versions = versions.at(key_it->value);
    if (!key_versions.data.empty()) {
      auto last_version = key_versions.data[key_versions.data.size() - 1].data;
      if (auto block_key = std::get_if<BlockKey>(&last_version)) {
        stale_keys.keys.push_back(VersionedKey{*key_it, block_key->block_id});
      }
    }

    // Add the new block_id to the key versions
    key_versions.data.emplace_back(KeyVersion{BlockKey{block_id}});
    kv_it++;
  }

  for (auto key_it = hashed_deleted_keys.begin(); key_it != hashed_deleted_keys.end(); key_it++) {
    // Is there a version that was just deleted? If so, we need to create a stale key.
    auto key_versions = versions.at(key_it->value);
    if (!key_versions.data.empty()) {
      auto last_version = key_versions.data[key_versions.data.size() - 1].data;
      if (auto block_key = std::get_if<BlockKey>(&last_version)) {
        stale_keys.keys.push_back(VersionedKey{*key_it, block_key->block_id});
      }
    }

    // Add the new tombstone to the key versions
    key_versions.data.emplace_back(KeyVersion{Tombstone{block_id}});
  }

  return stale_keys;
}

void MerkleCategory::putStale(NativeWriteBatch& batch,
                              const std::vector<uint8_t>& block_key,
                              sparse_merkle::StaleNodeIndexes&& staleNodes,
                              StaleKeys&& stale_keys) {
  StaleBlockNodes stale_block_nodes{};
  for (auto&& k : staleNodes.internal_keys) {
    stale_block_nodes.internal_keys.emplace_back(BatchedInternalNodeKey{
        k.version().value(), NibblePath{static_cast<uint8_t>(k.path().length()), k.path().data()}});
  }
  for (auto&& k : staleNodes.leaf_keys) {
    stale_block_nodes.leaf_keys.emplace_back(VersionedKey{KeyHash{k.hash().dataArray()}, k.version().value()});
  }
  StaleData stale_data{std::move(stale_block_nodes), std::move(stale_keys)};
  batch.put(MERKLE_STALE_CF, block_key, serialize(stale_data));
}

std::vector<uint8_t> MerkleCategory::serializeBatchedInternalNode(sparse_merkle::BatchedInternalNode&& node) {
  BatchedInternalNode cmf_node{};
  cmf_node.bitmask = 0;
  const auto& children = node.children();
  for (auto i = 0u; i < children.size(); ++i) {
    const auto& child = children[i];
    if (child) {
      cmf_node.bitmask |= (1 << i);
      if (auto leaf_child = std::get_if<sparse_merkle::LeafChild>(&child.value())) {
        cmf_node.children.emplace_back(BatchedInternalNodeChild{
            LeafChild{leaf_child->hash.dataArray(),
                      VersionedKey{KeyHash{leaf_child->key.hash().dataArray()}, leaf_child->key.version().value()}}});
      } else {
        auto internal_child = std::get<sparse_merkle::InternalChild>(child.value());
        cmf_node.children.emplace_back(
            BatchedInternalNodeChild{InternalChild{internal_child.hash.dataArray(), internal_child.version.value()}});
      }
    }
  }
  return serialize(cmf_node);
}  // namespace concord::kvbc::categorization::detail

void MerkleCategory::putMerkleNodes(NativeWriteBatch& batch, sparse_merkle::UpdateBatch&& update_batch) {
  for (const auto& [leaf_key, leaf_val] : update_batch.leaf_nodes) {
    auto ser_key = serialize(VersionedKey{KeyHash{leaf_key.hash().dataArray()}, leaf_key.version().value()});
    batch.put(MERKLE_LEAF_NODES_CF, ser_key, leaf_val.value.string_view());
  }

  for (auto& [internal_key, internal_node] : update_batch.internal_nodes) {
    auto ser_key = serialize(BatchedInternalNodeKey{
        internal_key.version().value(), NibblePath{(uint8_t)internal_key.path().length(), internal_key.path().data()}});
    batch.put(MERKLE_INTERNAL_NODES_CF, ser_key, serializeBatchedInternalNode(std::move(internal_node)));
  }
}

}  // namespace concord::kvbc::categorization::detail
