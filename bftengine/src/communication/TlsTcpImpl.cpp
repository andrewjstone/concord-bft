// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the
// LICENSE file.

#include <boost/filesystem.hpp>

#include "assertUtils.hpp"
#include "TlsTcpImpl.h"

namespace bftEngine {

void TlsTCPCommunication::TlsTcpImpl::Start() {
  std::lock_guard<std::mutex> l(startStopGuard_);
  if (io_thread_) return;

  // Start the io_thread_;
  io_thread_.reset(new std::thread([this]() {
    LOG_INFO(logger_, "io thread starting: isReplica() = " << isReplica());
    if (isReplica()) {
      listen();
      accept();
    }
    connect();
    startConnectTimer();

    // We must start connecting and accepting before we start the io_service, so that it has some
    // work to do. This is what prevents the io_service event loop from exiting immediately.
    io_service_.run();
  }));
}

void TlsTCPCommunication::TlsTcpImpl::Stop() {
  std::lock_guard<std::mutex> l(startStopGuard_);
  if (!io_thread_) return;
  io_service_.stop();
  if (io_thread_->joinable()) {
    io_thread_->join();
    io_thread_.reset(nullptr);
  }

  acceptor_.close();
  accepting_socket_.close();
  for (auto& [_, sock] : connecting_) {
    (void)_;  // unused variable hack
    sock.close();
  }
  for (auto& [_, conn] : connected_waiting_for_handshake_) {
    (void)_;  // unused variable hack
    syncCloseConnection(conn);
  }

  for (auto& [_, conn] : connected_waiting_for_handshake_) {
    (void)_;  // unused variable hack
    syncCloseConnection(conn);
  }

  for (auto& [_, conn] : connections_) {
    (void)_;  // unused variable hack
    syncCloseConnection(conn);
  }
}

void TlsTCPCommunication::TlsTcpImpl::startConnectTimer() {
  connect_timer_.expires_from_now(CONNECT_TICK);
  connect_timer_.async_wait([this](const boost::system::error_code& ec) {
    if (ec) {
      if (ec == boost::asio::error::operation_aborted) {
        // We are shutting down the system. Just return.
        return;
      }
      LOG_FATAL(logger_, "Connect timer wait failure: " << ec.message());
      abort();
    }
    connect();
    startConnectTimer();
  });
}

void TlsTCPCommunication::TlsTcpImpl::listen() {
  try {
    auto endpoint = syncResolve();
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(LISTEN_BACKLOG);
    LOG_INFO(logger_, "TLS server listening at " << endpoint);
  } catch (const boost::system::system_error& e) {
    LOG_FATAL(logger_,
              "Faield to start TlsTCPImpl acceptor at " << config_.listenHost << ":" << config_.listenPort
                                                        << " for node " << config_.selfId << ": " << e.what());
    abort();
  }
}

bool TlsTCPCommunication::TlsTcpImpl::isRunning() const {
  std::lock_guard<std::mutex> l(startStopGuard_);
  return io_thread_.get() != nullptr;
}

void TlsTCPCommunication::TlsTcpImpl::setReceiver(NodeNum nodeId, IReceiver* receiver) {
  // We don't allow setting a receiver after startup
  Assert(!isRunning());
  receiver_ = receiver;
}

void TlsTCPCommunication::TlsTcpImpl::sendAsyncMessage(const NodeNum destination, const char* msg, const size_t len) {
  std::lock_guard<std::mutex> lock(connectionsGuard_);
  auto temp = connections_.find(destination);
  if (temp != connections_.end()) {
    std::vector<char> owned(len + AsyncTlsConnection::MSG_HEADER_SIZE);
    std::memcpy(owned.data(), &len, AsyncTlsConnection::MSG_HEADER_SIZE);
    std::memcpy(owned.data() + AsyncTlsConnection::MSG_HEADER_SIZE, msg, len);
    temp->second->send(std::move(owned));
  } else {
    LOG_DEBUG(logger_, "Connection NOT found, from: " << config_.selfId << ", to: " << destination);
  }
}  // namespace bftEngine

void setSocketOptions(boost::asio::ip::tcp::socket& socket) { socket.set_option(boost::asio::ip::tcp::no_delay(true)); }

void TlsTCPCommunication::TlsTcpImpl::closeConnection(NodeNum id) {
  std::lock_guard<std::mutex> lock(connectionsGuard_);
  auto conn = std::move(connections_.at(id));
  connections_.erase(id);
  closeConnection(std::move(conn));
}

void TlsTCPCommunication::TlsTcpImpl::closeConnection(std::shared_ptr<AsyncTlsConnection> conn) {
  conn->getSocket().lowest_layer().cancel();
  conn->getSocket().async_shutdown([this, conn](const auto& ec) {
    if (ec) {
      LOG_WARN(logger_, "SSL shutdown failed: " << ec.message());
    }
    conn->getSocket().lowest_layer().close();
  });
}

void TlsTCPCommunication::TlsTcpImpl::syncCloseConnection(std::shared_ptr<AsyncTlsConnection>& conn) {
  boost::system::error_code _;
  conn->getSocket().shutdown(_);
  conn->getSocket().lowest_layer().close();
}

void TlsTCPCommunication::TlsTcpImpl::onConnectionAuthenticated(std::shared_ptr<AsyncTlsConnection> conn) {
  // Move the connection into the accepted connections map If. there is an existing connection
  // discard it. In this case it was likely that connecting end of the connection thinks there is
  // something wrong. This is a vector for a denial of service attack on the accepting side. We can
  // track the number of connections from the node and mark it malicious if necessary.
  std::lock_guard<std::mutex> lock(connectionsGuard_);
  auto it = connections_.find(conn->getPeerId().value());
  if (it != connections_.end()) {
    closeConnection(std::move(it->second));
  }
  connections_.insert({conn->getPeerId().value(), conn});
  conn->readMsgSizeHeader();
}

void TlsTCPCommunication::TlsTcpImpl::onServerHandshakeComplete(const boost::system::error_code& ec,
                                                                size_t accepted_connection_id) {
  auto conn = std::move(accepted_waiting_for_handshake_.at(accepted_connection_id));
  accepted_waiting_for_handshake_.erase(accepted_connection_id);
  if (ec) {
    auto peer_str = conn->getPeerId().has_value() ? std::to_string(conn->getPeerId().value()) : "Unknown";
    LOG_ERROR(logger_, "Server handshake failed for peer " << peer_str << ": " << ec.message());
    return closeConnection(std::move(conn));
  }
  LOG_INFO(logger_, "Server handshake succeeded for peer " << conn->getPeerId().value());
  onConnectionAuthenticated(std::move(conn));
}

void TlsTCPCommunication::TlsTcpImpl::onClientHandshakeComplete(const boost::system::error_code& ec,
                                                                NodeNum destination) {
  auto conn = std::move(connected_waiting_for_handshake_.at(destination));
  connected_waiting_for_handshake_.erase(destination);
  if (ec) {
    LOG_ERROR(logger_, "Client handshake failed for peer " << conn->getPeerId().value() << ": " << ec.message());
    return closeConnection(std::move(conn));
  }
  LOG_INFO(logger_, "Client handshake succeeded for peer " << conn->getPeerId().value());
  onConnectionAuthenticated(std::move(conn));
}

void TlsTCPCommunication::TlsTcpImpl::startServerSSLHandshake(boost::asio::ip::tcp::socket&& socket) {
  auto connection_id = total_accepted_connections_;
  auto conn = AsyncTlsConnection::create(io_service_, std::move(socket), receiver_, *this);
  accepted_waiting_for_handshake_.insert({connection_id, conn});
  conn->getSocket().async_handshake(
      boost::asio::ssl::stream_base::server,
      [this, connection_id](const boost::system::error_code& ec) { onServerHandshakeComplete(ec, connection_id); });
}

void TlsTCPCommunication::TlsTcpImpl::startClientSSLHandshake(boost::asio::ip::tcp::socket&& socket,
                                                              NodeNum destination) {
  auto conn = AsyncTlsConnection::create(io_service_, std::move(socket), receiver_, *this, destination);
  connected_waiting_for_handshake_.insert({destination, conn});
  conn->getSocket().async_handshake(
      boost::asio::ssl::stream_base::client,
      [this, destination](const boost::system::error_code& ec) { onClientHandshakeComplete(ec, destination); });
}

void TlsTCPCommunication::TlsTcpImpl::accept() {
  acceptor_.async_accept(accepting_socket_, [this](boost::system::error_code ec) {
    if (ec) {
      LOG_WARN(logger_, "async_accept failed: " << ec.message());
      // When io_service is stopped, the handlers are destroyed and when the
      // io_service dtor runs they will be invoked with operation_aborted error.
      // In this case we dont want to accept again.
      if (ec == boost::asio::error::operation_aborted) return;
    }
    total_accepted_connections_++;
    setSocketOptions(accepting_socket_);
    LOG_INFO(logger_, "Accepted connection " << total_accepted_connections_);
    startServerSSLHandshake(std::move(accepting_socket_));
    accept();
  });
}

void TlsTCPCommunication::TlsTcpImpl::resolve(NodeNum i) {
  resolving_.insert(i);
  auto node = config_.nodes.at(i);
  boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), node.host, std::to_string(node.port));
  resolver_.async_resolve(query, [this, node, i, query](const auto& error_code, auto results) {
    if (error_code) {
      LOG_WARN(
          logger_,
          "Failed to resolve node " << i << ": " << node.host << ":" << node.port << " : " << error_code.message());
      return;
    }
    boost::asio::ip::tcp::endpoint endpoint = *results;
    LOG_INFO(logger_, "Resolved node " << i << ": " << node.host << ":" << node.port << " to " << endpoint);
    resolving_.erase(i);
    connect(i, endpoint);
  });
}

void TlsTCPCommunication::TlsTcpImpl::connect(NodeNum i, boost::asio::ip::tcp::endpoint endpoint) {
  auto [it, inserted] = connecting_.emplace(i, boost::asio::ip::tcp::socket(io_service_));
  Assert(inserted);
  it->second.async_connect(endpoint, [this, i, endpoint](const auto& error_code) {
    if (error_code) {
      LOG_WARN(logger_, "Failed to connect to node " << i << ": " << endpoint);
      connecting_.at(i).close();
      connecting_.erase(i);
      return;
    }
    LOG_INFO(logger_, "Connected to node " << i << ": " << endpoint);
    auto connected_socket = std::move(connecting_.at(i));
    connecting_.erase(i);
    startClientSSLHandshake(std::move(connected_socket), i);
  });
}

void TlsTCPCommunication::TlsTcpImpl::connect() {
  std::lock_guard<std::mutex> lock(connectionsGuard_);
  auto end = config_.selfId == 0 ? 0 : std::min<size_t>(config_.selfId - 1, config_.maxServerId);
  for (auto i = 0u; i < end; i++) {
    if (connections_.count(i) == 0 && connecting_.count(i) == 0 && resolving_.count(i) == 0 &&
        connected_waiting_for_handshake_.count(i) == 0) {
      resolve(i);
    }
  }
}

boost::asio::ip::tcp::endpoint TlsTCPCommunication::TlsTcpImpl::syncResolve() {
  // TODO: When upgrading to boost 1.66 or later, when query is deprecated,
  // this should be changed to call the resolver.resolve overload that takes a
  // u, host, and service directly, instead of a query object. That
  // overload is not yet available in boost 1.64, which we're using today.
  boost::asio::ip::tcp::resolver::query query(
      boost::asio::ip::tcp::v4(), config_.listenHost, std::to_string(config_.listenPort));
  boost::asio::ip::tcp::resolver::iterator results = resolver_.resolve(query);
  boost::asio::ip::tcp::endpoint endpoint = *results;
  LOG_INFO(logger_, "Resolved " << config_.listenHost << ":" << config_.listenPort << " to " << endpoint);
  return endpoint;
}

int TlsTCPCommunication::TlsTcpImpl::getMaxMessageSize() { return config_.bufferLength; }

ConnectionStatus TlsTCPCommunication::TlsTcpImpl::getCurrentConnectionStatus(const NodeNum id) const {
  std::lock_guard<std::mutex> lock(connectionsGuard_);
  if (connections_.count(id) == 1) {
    return ConnectionStatus::Connected;
  }
  return ConnectionStatus::Disconnected;
}

}  // namespace bftEngine