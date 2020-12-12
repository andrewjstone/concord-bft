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

#include <string>

namespace concord::kvbc::categorization::detail {

inline const auto SHARED_KV_DATA_CF = std::string{"shared_kv_data"};
inline const auto SHARED_KV_KEY_VERSIONS_CF = std::string{"shared_kv_key_versions"};

inline const auto BLOCKS_CF = std::string{"blocks"};

inline const auto MERKLE_INTERNAL_NODES_CF = std::string{"merkle_internal_nodes"};
inline const auto MERKLE_LEAF_NODES_CF = std::string{"merkle_leaf_nodes"};
inline const auto MERKLE_STALE_CF = std::string{"merkle_stale"};
inline const auto MERKLE_KEY_VERSIONS_CF = std::string{"merkle_key_versions"};
inline const auto MERKLE_KEYS_CF = std::string{"merkle_keys"};

}  // namespace concord::kvbc::categorization::detail
