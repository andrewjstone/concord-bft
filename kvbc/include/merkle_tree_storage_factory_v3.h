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
#pragma once

#include "storage_factory_interface.h"
#include "kv_types.hpp"

#include <optional>
#include <string>
#include <unordered_set>

namespace concord::kvbc::v3MerkleTree {

class RocksDBStorageFactory : public IStorageFactory {
 public:
  RocksDBStorageFactory(
      const std::string& dbPath,
      const std::unordered_set<concord::kvbc::Key>& nonProvableKeySet = std::unordered_set<concord::kvbc::Key>{});

 public:
  DatabaseSet newDatabaseSet() const override;
  std::unique_ptr<storage::IMetadataKeyManipulator> newMetadataKeyManipulator() const override;
  std::unique_ptr<storage::ISTKeyManipulator> newSTKeyManipulator() const override;

 private:
  const std::string dbPath_;
  const std::unordered_set<concord::kvbc::Key> nonProvableKeySet_;
};

}  // namespace concord::kvbc::v3MerkleTree

#endif
