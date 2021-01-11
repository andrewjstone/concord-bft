// Concord
//
// Copyright (c) 2020-2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the
// "License").  You may not use this product except in compliance with the
// Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "categorization/block_merkle_category.h"
#include "kv_types.hpp"
#include "rocksdb/native_client.h"
#include "storage/test/storage_test_common.h"
#include "categorization/column_families.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <variant>

namespace {

using namespace ::testing;
using namespace concord::storage::rocksdb;
using namespace concord::kvbc;
using namespace concord::kvbc::categorization;
using namespace concord::kvbc::categorization::detail;
using namespace std::literals;

class block_merkle_category : public Test {
  void SetUp() override {
    cleanup();
    db = TestRocksDb::createNative();
    cat = BlockMerkleCategory{db};
  }
  void TearDown() override { cleanup(); }

 protected:
  auto add(BlockId block_id, BlockMerkleInput &&update) {
    auto batch = db->getBatch();
    auto output = cat.add(block_id, std::move(update), batch);
    db->write(std::move(batch));
    return output;
  }

 protected:
  std::shared_ptr<NativeClient> db;
  BlockMerkleCategory cat;
  std::string key1 = "key1"s;
  std::string key2 = "key2"s;
  std::string key3 = "key3"s;
  std::string key4 = "key4"s;
  std::string key5 = "key5"s;
  std::string val1 = "val1"s;
  std::string val2 = "val2"s;
  std::string val3 = "val3"s;
  std::string val4 = "val4"s;
  std::string val5 = "val5"s;
  Hash hashed_key1 = hash(key1);
  Hash hashed_key2 = hash(key2);
  Hash hashed_key3 = hash(key3);
  Hash hashed_key4 = hash(key4);
  Hash hashed_key5 = hash(key5);
};

TEST_F(block_merkle_category, empty_updates) {
  auto update = BlockMerkleInput{};
  auto batch = db->getBatch();
  const auto output = cat.add(1, std::move(update), batch);

  // A new root index, an internal node, a leaf node, and stale index node are created.
  ASSERT_EQ(batch.count(), 4);
}

TEST_F(block_merkle_category, put_and_get) {
  auto update = BlockMerkleInput{{{key1, val1}}};
  auto batch = db->getBatch();
  auto block_id = 1u;
  const auto output = cat.add(block_id, std::move(update), batch);

  // A new root index, an internal node, a leaf node, and stale index node are created.
  // Additionally, a key and its value are written.
  ASSERT_EQ(batch.count(), 6);
  ASSERT_EQ(1, output.state_root_version);
  ASSERT_EQ(false, output.keys.find(key1)->second.deleted);

  db->write(std::move(batch));

  auto expected = MerkleValue{{block_id, val1}};

  // Get by key works
  ASSERT_EQ(expected, cat.getLatest(key1).value());
  // Get by specific block works
  ASSERT_EQ(expected, cat.get(key1, 1));

  // Get by hash works
  ASSERT_EQ(expected, cat.get(hashed_key1, block_id).value());

  // Getting the latest version by key and hash works
  ASSERT_EQ(block_id, cat.getLatestVersion(key1)->encode());
  ASSERT_EQ(block_id, cat.getLatestVersion(hashed_key1)->encode());

  // Getting the key at the wrong block fails
  ASSERT_EQ(false, cat.get(key1, block_id + 1).has_value());
  ASSERT_EQ(false, cat.get(hashed_key1, block_id + 1).has_value());

  // Trying to get non-existant keys returns std::nullopt
  ASSERT_EQ(false, cat.getLatest(key2).has_value());
  ASSERT_EQ(false, cat.get(key2, block_id).has_value());
  ASSERT_EQ(false, cat.get(hashed_key2, block_id).has_value());
}

TEST_F(block_merkle_category, multiget) {
  auto update = BlockMerkleInput{{{key1, val1}, {key2, val2}, {key3, val3}}};
  BlockId block_id = 1;
  const auto output = add(block_id, std::move(update));

  // We can get all keys individually
  ASSERT_EQ(val1, cat.getLatest(key1).value().data);
  ASSERT_EQ(val2, cat.getLatest(key2).value().data);
  ASSERT_EQ(val3, cat.getLatest(key3).value().data);

  // We can get all 3 with a multiget
  auto keys = std::vector<std::string>{key1, key2, key3};
  auto versions = std::vector<BlockId>{1u, 1u, 1u};
  auto values = std::vector<std::optional<MerkleValue>>{};
  cat.multiGet(keys, versions, values);
  ASSERT_EQ(keys.size(), values.size());
  ASSERT_EQ((MerkleValue{{block_id, val1}}), *values[0]);
  ASSERT_EQ((MerkleValue{{block_id, val2}}), *values[1]);
  ASSERT_EQ((MerkleValue{{block_id, val3}}), *values[2]);

  // Retrieving a key with the non-existant version causes a nullopt for that key only
  versions[1] = 2u;
  cat.multiGet(keys, versions, values);
  ASSERT_EQ(keys.size(), values.size());
  ASSERT_EQ((MerkleValue{{block_id, val1}}), *values[0]);
  ASSERT_EQ(false, values[1].has_value());
  ASSERT_EQ((MerkleValue{{block_id, val3}}), *values[2]);

  // Retrieving a non-existant key causes a nullopt for that key only
  keys.push_back("key4");
  versions.push_back(1u);
  cat.multiGet(keys, versions, values);
  ASSERT_EQ(keys.size(), values.size());
  ASSERT_EQ((MerkleValue{{block_id, val1}}), *values[0]);
  ASSERT_EQ(false, values[1].has_value());
  ASSERT_EQ((MerkleValue{{block_id, val3}}), *values[2]);
  ASSERT_EQ(false, values[3].has_value());

  // Get latest versions
  auto out_versions = std::vector<std::optional<TaggedVersion>>{};
  cat.multiGetLatestVersion(keys, out_versions);
  ASSERT_EQ(1, out_versions[0]->encode());
  ASSERT_EQ(1, out_versions[1].has_value());
  ASSERT_EQ(1, out_versions[2]->version);
  ASSERT_EQ(false, out_versions[3].has_value());

  // Get latest values
  cat.multiGetLatest(keys, values);
  ASSERT_EQ((MerkleValue{{block_id, val1}}), *values[0]);
  ASSERT_EQ((MerkleValue{{block_id, val2}}), *values[1]);
  ASSERT_EQ((MerkleValue{{block_id, val3}}), *values[2]);
  ASSERT_EQ(false, values[3].has_value());
}

TEST_F(block_merkle_category, overwrite) {
  auto update = BlockMerkleInput{{{key1, val1}, {key2, val2}, {key3, val3}}};
  auto _ = add(1, std::move(update));

  // Overwrite key 2 and add key4
  _ = add(2, BlockMerkleInput{{{key2, "new_val"s}, {key4, val4}}});
  auto expected1 = MerkleValue{{1, val1}};
  auto expected2 = MerkleValue{{2, "new_val"s}};
  auto expected3 = MerkleValue{{1, val3}};
  auto expected4 = MerkleValue{{2, val4}};
  ASSERT_EQ(expected1, *cat.getLatest(key1));
  ASSERT_EQ(expected2, *cat.getLatest(key2));
  ASSERT_EQ(expected3, *cat.getLatest(key3));
  ASSERT_EQ(expected4, *cat.getLatest(key4));

  auto keys = std::vector<std::string>{key1, key2, key3, key4};
  auto values = std::vector<std::optional<MerkleValue>>{};
  cat.multiGetLatest(keys, values);
  ASSERT_EQ(expected1, *values[0]);
  ASSERT_EQ(expected2, *values[1]);
  ASSERT_EQ(expected3, *values[2]);
  ASSERT_EQ(expected4, *values[3]);

  // Multiget will find the old value of key2 at 1, but not key4 since it was written at block 2
  const auto versions = std::vector<BlockId>{1u, 1u, 1u, 1u};
  cat.multiGet(keys, versions, values);
  ASSERT_EQ(expected1, *values[0]);
  ASSERT_EQ((MerkleValue{{1, val2}}), *values[1]);
  ASSERT_EQ(expected3, *values[2]);
  ASSERT_EQ(false, values[3].has_value());
}

TEST_F(block_merkle_category, updates_with_deleted_keys) {
  auto update = BlockMerkleInput{{{key1, val1}, {key2, val2}, {key3, val3}, {key4, val4}, {key5, val5}}};
  auto _ = add(1, std::move(update));

  // Overwrite key 2, delete key 3 and key 5
  _ = add(2, BlockMerkleInput{{{key2, "new_val"s}}, {{key3, key5}}});
  auto expected1 = MerkleValue{{1, val1}};
  auto expected2 = MerkleValue{{2, "new_val"s}};
  auto expected4 = MerkleValue{{1, val4}};
  ASSERT_EQ(expected1, *cat.getLatest(key1));
  ASSERT_EQ(expected2, *cat.getLatest(key2));
  ASSERT_FALSE(cat.getLatest(key3).has_value());
  ASSERT_EQ(expected4, *cat.getLatest(key4));
  ASSERT_FALSE(cat.getLatest(key5).has_value());

  auto keys = std::vector<std::string>{key1, key2, key3, key4, key5};
  auto values = std::vector<std::optional<MerkleValue>>{};
  cat.multiGetLatest(keys, values);
  ASSERT_EQ(expected1, *values[0]);
  ASSERT_EQ(expected2, *values[1]);
  ASSERT_FALSE(values[2].has_value());
  ASSERT_EQ(expected4, *values[3]);
  ASSERT_FALSE(values[4].has_value());

  // Multiget will find the old values of key2, key3 and key5 at block 1;
  const auto versions = std::vector<BlockId>{1u, 1u, 1u, 1u, 1u};
  cat.multiGet(keys, versions, values);
  ASSERT_EQ(expected1, *values[0]);
  ASSERT_EQ((MerkleValue{{1, val2}}), *values[1]);
  ASSERT_EQ((MerkleValue{{1, val3}}), *values[2]);
  ASSERT_EQ(expected4, *values[3]);
  ASSERT_EQ((MerkleValue{{1, val5}}), *values[4]);

  // Getting the latest versions shows key3 and key5 as deleted.
  auto expected_v1 = TaggedVersion{false, 1};
  auto expected_v2 = TaggedVersion{false, 2};
  auto expected_v3 = TaggedVersion{true, 2};
  auto expected_v4 = TaggedVersion{false, 1};
  auto expected_v5 = TaggedVersion{true, 2};
  ASSERT_EQ(expected_v1, *cat.getLatestVersion(key1));
  ASSERT_EQ(expected_v2, *cat.getLatestVersion(key2));
  ASSERT_EQ(expected_v3, *cat.getLatestVersion(key3));
  ASSERT_EQ(expected_v4, *cat.getLatestVersion(key4));
  ASSERT_EQ(expected_v5, *cat.getLatestVersion(key5));
  auto tagged_versions = std::vector<std::optional<TaggedVersion>>{};
  cat.multiGetLatestVersion({key1, key2, key3, key4, key5}, tagged_versions);
  ASSERT_EQ(expected_v1, tagged_versions[0]);
  ASSERT_EQ(expected_v2, tagged_versions[1]);
  ASSERT_EQ(expected_v3, tagged_versions[2]);
  ASSERT_EQ(expected_v4, tagged_versions[3]);
  ASSERT_EQ(expected_v5, tagged_versions[4]);
}

TEST_F(block_merkle_category, stale_node_creation_and_deletion) {
  // Create the first block and read its stale keys
  auto update = BlockMerkleInput{{{key1, val1}, {key2, val2}, {key3, val3}, {key4, val4}, {key5, val5}}};
  const auto block_out1 = add(1, std::move(update));
  auto ser_stale = db->get(BLOCK_MERKLE_STALE_CF, serialize(TreeVersion{1}));
  auto stale = StaleKeys{};
  deserialize(*ser_stale, stale);

  // There's no stale keys on the first block creation, since nothing existed before the first block.
  ASSERT_EQ(0, stale.internal_keys.size());
  ASSERT_EQ(0, stale.leaf_keys.size());

  // Create the second block and read its stale keys
  const auto block_out2 = add(2, BlockMerkleInput{{{key2, "new_val"s}}, {{key3, key5}}});
  ser_stale = db->get(BLOCK_MERKLE_STALE_CF, serialize(TreeVersion{2}));
  stale = StaleKeys{};
  deserialize(*ser_stale, stale);

  // Adding a new block adds stale internal keys, but no stale leaf keys, as blocks aren't deleted.
  ASSERT_LT(0, stale.internal_keys.size());
  ASSERT_EQ(0, stale.leaf_keys.size());

  // There have been no pruned blocks yet.
  ASSERT_EQ(0, cat.getLastDeletedTreeVersion());
  ASSERT_EQ(2, cat.getLatestTreeVersion());

  // Pruning the first block causes key2, key3, and key5 from version 1 to be deleted.
  // Keys 1, 4, 5 are still active at version 1 and so remain in the database.

  // No tree nodes will get deleted though, as none are stale.
  auto batch = db->getBatch();
  cat.deleteGenesisBlock(1, block_out1, batch);
  db->write(std::move(batch));
  ASSERT_FALSE(cat.get(key2, 1));
  ASSERT_FALSE(cat.get(key3, 1));
  ASSERT_FALSE(cat.get(key5, 1));
  ASSERT_TRUE(cat.get(key1, 1));
  ASSERT_TRUE(cat.get(key4, 1));
  ASSERT_EQ(1, cat.getLastDeletedTreeVersion());

  // A new tree version gets created as a result of the block deletion.
  ASSERT_EQ(3, cat.getLatestTreeVersion());

  // There are stale leaf keys as of block deletion
  ser_stale = db->get(BLOCK_MERKLE_STALE_CF, serialize(TreeVersion{3}));
  stale = StaleKeys{};
  deserialize(*ser_stale, stale);
  ASSERT_LT(0, stale.internal_keys.size());
  ASSERT_LT(0, stale.leaf_keys.size());

  // Deleting Block2 causes there to still be active keys.
  batch = db->getBatch();
  cat.deleteGenesisBlock(2, block_out2, batch);
  db->write(std::move(batch));
  ASSERT_TRUE(cat.getLatest(key1));
  ASSERT_TRUE(cat.getLatest(key2));
  ASSERT_FALSE(cat.getLatest(key3));
  ASSERT_TRUE(cat.getLatest(key4));
  ASSERT_FALSE(cat.getLatest(key5));
  ASSERT_EQ(2, cat.getLastDeletedTreeVersion());

  // A new tree version gets created as a result of the block deletion.
  ASSERT_EQ(4, cat.getLatestTreeVersion());

  // Let's delete the last remaining keys, with a new block addition.
  const auto block_out3 = add(3, BlockMerkleInput{{}, {{key1, key2, key4}}});
  ASSERT_FALSE(cat.getLatest(key1));
  ASSERT_FALSE(cat.getLatest(key2));
  ASSERT_FALSE(cat.getLatest(key3));
  ASSERT_FALSE(cat.getLatest(key4));
  ASSERT_FALSE(cat.getLatest(key5));
  ASSERT_EQ(2, cat.getLastDeletedTreeVersion());
  ASSERT_EQ(5, cat.getLatestTreeVersion());

  // We still see tombstones for keys 1, 2, 3
  ASSERT_TRUE(cat.getLatestVersion(key1)->deleted);
  ASSERT_TRUE(cat.getLatestVersion(key2)->deleted);
  ASSERT_TRUE(cat.getLatestVersion(key4)->deleted);

  // We can still access those keys at their old versions
  auto expected1 = MerkleValue{{1, val1}};
  auto expected2 = MerkleValue{{2, "new_val"s}};
  auto expected4 = MerkleValue{{1, val4}};
  ASSERT_EQ(expected1, cat.get(key1, 1));
  ASSERT_EQ(expected2, cat.get(key2, 2));
  ASSERT_EQ(expected4, cat.get(key4, 1));

  // Deleting block 3 triggers all tree versions up to 5 to be removed
  batch = db->getBatch();
  cat.deleteGenesisBlock(3, block_out3, batch);
  db->write(std::move(batch));
  ASSERT_EQ(5, cat.getLastDeletedTreeVersion());
  ASSERT_EQ(6, cat.getLatestTreeVersion());

  // There are no more latest versions for any keys.
  ASSERT_FALSE(cat.getLatestVersion(key1));
  ASSERT_FALSE(cat.getLatestVersion(key2));
  ASSERT_FALSE(cat.getLatestVersion(key3));
  ASSERT_FALSE(cat.getLatestVersion(key4));
  ASSERT_FALSE(cat.getLatestVersion(key5));

  // We can no longer retrieve keys 1, 2, 4 at their old versions
  ASSERT_FALSE(cat.get(key1, 1));
  ASSERT_FALSE(cat.get(key2, 2));
  ASSERT_FALSE(cat.get(key4, 1));
}

}  // namespace

int main(int argc, char *argv[]) {
  ::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
