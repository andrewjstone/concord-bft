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
//

#ifdef USE_ROCKSDB
#include "merkle_tree_storage_factory_v3.h"

#include "merkle_tree_db_adapter_v3.h"
#include "memorydb/client.h"
#include "storage/merkle_tree_key_manipulator.h"
#include "rocksdb/client.h"

namespace concord::kvbc::v3MerkleTree {

RocksDBStorageFactory::RocksDBStorageFactory(const std::string& dbPath,
                                             const std::unordered_set<concord::kvbc::Key>& nonProvableKeySet)
    : dbPath_{dbPath}, nonProvableKeySet_{nonProvableKeySet} {}

IStorageFactory::DatabaseSet RocksDBStorageFactory::newDatabaseSet() const {
  auto ret = IStorageFactory::DatabaseSet{};
  ret.dataDBClient = std::make_shared<storage::rocksdb::Client>(dbPath_);
  ret.dataDBClient->init();
  ret.metadataDBClient = ret.dataDBClient;
  ret.dbAdapter = std::make_unique<DBAdapter>(ret.dataDBClient, true, nonProvableKeySet_);
  return ret;
}

std::unique_ptr<storage::IMetadataKeyManipulator> RocksDBStorageFactory::newMetadataKeyManipulator() const {
  // Reuse the v2 key manipulators for now
  // TODO: Likely get rid of the whole idea of key manipulators, and just use column families
  return std::make_unique<storage::v2MerkleTree::MetadataKeyManipulator>();
}

std::unique_ptr<storage::ISTKeyManipulator> RocksDBStorageFactory::newSTKeyManipulator() const {
  // Reuse the v2 key manipulators for now
  // TODO: Likely get rid of the whole idea of key manipulators, and just use column families
  return std::make_unique<storage::v2MerkleTree::STKeyManipulator>();
}

}  // namespace concord::kvbc::v3MerkleTree

#endif
