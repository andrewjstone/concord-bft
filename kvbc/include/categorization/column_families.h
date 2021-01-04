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

inline const auto IMMUTABLE_KV_CF_SUFFIX = std::string{"_immutable"};

inline const auto BLOCKS_CF = std::string{"blocks"};
inline const auto ST_CHAIN_CF = std::string{"st_chain"};

<<<<<<< HEAD
inline const auto CAT_ID_TYPE_CF = std::string{"cat_id_type"};
=======
inline const auto MERKLE_INTERNAL_NODES_CF = std::string{"merkle_internal_nodes"};
inline const auto MERKLE_LEAF_NODES_CF = std::string{"merkle_leaf_nodes"};
inline const auto MERKLE_LATEST_KEY_VERSION = std::string{"merkle_latest_key_version"};
inline const auto MERKLE_KEYS_CF = std::string{"merkle_keys"};
>>>>>>> wip

}  // namespace concord::kvbc::categorization::detail
