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

#include <list>
#include <optional>
#include <unordered_map>

#include "sparse_merkle/internal_node.h"

namespace concord::kvbc::v2MerkleTree {

// This class caches the *LATEST* version of each node. It does not cache earlier versions, as this
// can be done well by system/db caches.
//
// In the common case, the RocksDB LRU cache cannot properly cache our get workload, because all tree nodes are
// versioned. Every time a branch of a tree is updated, the old nodes are no longer used for updates. Only the new
// latest version are used, and each time we access them we create a new valid latest version. Therefore, on each update
// we only read a specific versioned node once making the RocksDB cache ineffective. Since the top levels of the tree
// are shared, and similar nodes are almost always updated in a write, an LRU cache works well for limiting DB accesses
// to newly written data that is going to be immediately needed.
class ReaderLRUCache {
 public:
  ReaderLRUCache(size_t max_nodes) : capacity_(max_nodes) { map_.reserve(max_nodes); }

  // Add a new node to the cache. This makes it the most recent node.
  void add(const sparse_merkle::NibblePath& key, const sparse_merkle::BatchedInternalNode& node) {
    stats_.adds++;
    queue_.push_front(key);
    auto iter = map_.find(key);
    if (iter != map_.end()) {
      queue_.erase(iter->second.second);
      iter->second = {node, queue_.begin()};
    } else {
      map_.insert({key, {node, queue_.begin()}});
    }

    if (queue_.size() == capacity_) {
      auto& key = queue_.back();
      map_.erase(key);
      queue_.pop_back();
    }
  }

  // Get the latest version of the BatchedInternalNode at the given path
  std::optional<sparse_merkle::BatchedInternalNode> get(const sparse_merkle::NibblePath& key) const {
    auto iter = map_.find(key);
    if (iter == map_.end()) {
      stats_.misses++;
      return std::nullopt;
    } else {
      stats_.hits++;
      return iter->second.first;
    }
  }

  struct Stats {
    mutable size_t hits = 0;
    mutable size_t misses = 0;
    mutable size_t adds = 0;
  };
  Stats getStats() { return stats_; }

 private:
  size_t capacity_;

  using Key = sparse_merkle::NibblePath;
  using Val = std::pair<sparse_merkle::BatchedInternalNode, std::list<Key>::iterator>;

  std::list<Key> queue_;
  std::unordered_map<Key, Val> map_;

  Stats stats_;
};

}  // namespace concord::kvbc::v2MerkleTree
