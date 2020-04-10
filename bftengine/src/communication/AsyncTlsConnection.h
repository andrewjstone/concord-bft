// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the
// LICENSE file.

#pragma once

#include <vector>
#include <mutex>
#include <optional>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "CommDefs.hpp"

namespace bftEngine {

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> SSL_SOCKET;

class AsyncTlsConnection {
 public:
  // Any message attempted to be put on the queue that causes the total size of the queue to exceed
  // this value will be dropped.
  static constexpr size_t MAX_QUEUE_SIZE_IN_BYTES = 64 * 1024 * 1024;  // 64 MB

  AsyncTlsConnection(SSL_SOCKET&& socket, IReceiver* receiver) : socket_(std::move(socket)), receiver_(receiver) {}

  void send(std::vector<char>&& msg);
  void setPeerId(NodeNum peer_id) { peer_id_ = peer_id; }
  std::optional<NodeNum> getPeerId() { return peer_id_; }
  SSL_SOCKET& getSocket() { return socket_; }

 private:
  std::optional<NodeNum> peer_id_ = std::nullopt;
  SSL_SOCKET socket_;

  // We assume `receiver_` lives at least as long as each connection.
  IReceiver* receiver_ = nullptr;

  // We must maintain ownership of the in_flight_message until the asio::buffer wrapping it has actually been sent by
  // the underling io_service. We will know this is the case when the write completion handler gets called.
  std::optional<std::vector<char>> in_flight_message;
  size_t queue_size_in_bytes_ = 0;
  std::vector<std::vector<char>> outQueue;
  std::mutex writeLock_;
};

}  // namespace bftEngine