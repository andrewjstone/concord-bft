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

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <set>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>

#include "CommDefs.hpp"
#include "Logger.hpp"
#include "AsyncTlsConnection.h"

#pragma once

namespace bftEngine {

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> SSL_SOCKET;

/**
 * Implementation class. Is reponsible for creating listener on given port,
 * outgoing connections to the lower Id peers and accepting connections from
 *  higher ID peers.
 *
 *  This is default behavior given the clients will always have higher IDs
 *  from the replicas. In this way we assure that clients will not connect to
 *  each other.
 *
 *  We will have to revisit this connection strategy once we have dynamic reconfiguration.
 *
 */
class TlsTCPCommunication::TlsTcpImpl {
  static constexpr size_t LISTEN_BACKLOG = 5;
  static constexpr std::chrono::seconds CONNECT_TICK = std::chrono::seconds(1);

  friend class AsyncTlsConnection;

 public:
  TlsTcpImpl(const TlsTcpConfig &config)
      : logger_(concordlogger::Log::getLogger("concord-bft.tls")),
        config_(config),
        acceptor_(io_service_),
        resolver_(io_service_),
        accepting_socket_(io_service_),
        connect_timer_(io_service_) {}

  //
  // Methods required by ICommuncication
  // Called as part of pImpl idiom
  //
  void Start();
  void Stop();
  bool isRunning() const;
  ConnectionStatus getCurrentConnectionStatus(const NodeNum node) const;
  void setReceiver(NodeNum nodeId, IReceiver *receiver);
  void sendAsyncMessage(const NodeNum destination, const char *const msg, const size_t msg_len);
  int getMaxMessageSize();

 private:
  // Open a socket, configure it, bind it, and start listening on a given host/pot.
  void listen();

  // Start asynchronously accepting connections.
  void accept();

  // Perform synhronous DNS resolution. This really only works for the listen socket, since after that we want to do a
  // new lookup on every connect operation in case the underlying IP of the DNS address changes.
  //
  // Throws a boost::system::system_error if it fails to resolve
  boost::asio::ip::tcp::endpoint syncResolve();

  // Starts async dns resolution on a tcp socket.
  //
  // Every call to connect will call resolve before asio async_connect call.
  void resolve(NodeNum destination);

  // Connet to other nodes where there is not a current connection.
  //
  // Replicas only connect to replicas with smaller node ids.
  // This method is called at startup and also on a periodic timer tick.
  void connect();
  void connect(NodeNum, boost::asio::ip::tcp::endpoint);

  // Start a steady_timer in order to trigger needed `connect` operations.
  void startConnectTimer();

  // In order to create a boost::asio::ssl::stream we need to create an SSL context with the
  // appropriate verification callbacks used for certificate pinning. Thse functions create the
  // contexts for both incoming and outgoing connections.
  boost::asio::ssl::context createServerSSLContext();
  boost::asio::ssl::context createClientSSLContext(NodeNum destination);

  // Callbacks triggered by asio to verify certificates. These callbacks are registered in the
  // create<Server|Client>SSLContext functions.
  bool verifyCertificateServer(bool preverified, boost::asio::ssl::verify_context &ctx, size_t accepted_connection_id);
  bool verifyCertificateClient(bool preverified, boost::asio::ssl::verify_context &ctx, NodeNum destination);

  // Trigger the asio async_hanshake calls.
  void startServerSSLHandshake(boost::asio::ip::tcp::socket &&);
  void startClientSSLHandshake(boost::asio::ip::tcp::socket &&socket, NodeNum destination);

  // Callbacks triggered when asio async_hanshake completes for an incoming or outgoing connection.
  void onServerHandshakeComplete(const boost::system::error_code &ec, size_t accepted_connection_id);
  void onClientHandshakeComplete(const boost::system::error_code &ec, NodeNum destination);

  // If onServerHandshake completed successfully, this function will get called and add the AsyncTlsConnection to
  // `connections_`.
  void onConnectionAuthenticated(std::shared_ptr<AsyncTlsConnection> conn);

  // When a certificate is validated on an accepted connection, this function is called in order to
  // save the learned NodeNum from the certificate so this replica knows who it is connected to.
  void setVerifiedPeerId(size_t accepted_connection_id, NodeNum peer_id);

  // Synchronously close connections
  // This is only used during shutdown after the io_service is stopped.
  void syncCloseConnection(std::shared_ptr<AsyncTlsConnection> &conn);

  // Asynchronously shutdown an SSL connection and then close the underlying TCP socket when the shutdown has completed.
  void closeConnection(NodeNum);
  void closeConnection(std::shared_ptr<AsyncTlsConnection> conn);

  // Certificate pinning
  //
  // Check for a specific certificate and do not rely on the chain authentication.
  //
  // Return true along with the actual node id if verification succeeds, (false, 0) if not.
  std::pair<bool, NodeNum> checkCertificate(X509 *cert,
                                            std::string connectionType,
                                            std::string subject,
                                            std::optional<NodeNum> expected_peer_id);

  bool isReplica() { return config_.selfId <= static_cast<size_t>(config_.maxServerId); }

  concordlogger::Logger logger_;
  TlsTcpConfig config_;

  // The lifetime of the receiver must be at least as long as the lifetime of this object
  IReceiver *receiver_;

  // This thread runs the asio io_service. It's the main thread of the TlsTcpImpl.
  std::unique_ptr<std::thread> io_thread_;

  // io_thread_ lifecycle management
  mutable std::mutex startStopGuard_;

  // Use io_context when we upgrade boost, as io_service is deprecated
  // https://stackoverflow.com/questions/59753391/boost-asio-io-service-vs-io-context
  boost::asio::io_service io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;

  // We store a single resolver so that it doesn't go out of scope during async_resolve calls.
  boost::asio::ip::tcp::resolver resolver_;

  // Every async_accept call for asio requires us to pass it an existing socket to write into. Later versions do not
  // require this. This socket will be filled in by an accepted connection. We'll then move it into an
  // AsyncTlsConnection when the async_accept handler returns.
  boost::asio::ip::tcp::socket accepting_socket_;

  // For each accepted connection, we bump this value. We use it as a key into a map to find any in
  // progress connections when cert validation completes
  size_t total_accepted_connections_ = 0;

  // This timer is called periodically to trigger connections as needed.
  boost::asio::steady_timer connect_timer_;

  // This tracks outstanding attempts at DNS resolution for outgoing connections.
  std::set<NodeNum> resolving_;

  // Sockets that are in progress of connecting.
  // When these connections complete, an AsyncTlsConnection will be created and moved into
  // `connected_waiting_for_handshake_`.
  std::map<NodeNum, boost::asio::ip::tcp::socket> connecting_;

  // Connections that are in progress of waiting for a handshake to complete.
  // When the handshake completes these will be moved into `connections_`.
  std::map<NodeNum, std::shared_ptr<AsyncTlsConnection>> connected_waiting_for_handshake_;

  // Connections that have been accepted, but where the handshake has not been completed.
  // When the handshake completes these will be moved into `connections_`.
  std::map<size_t, std::shared_ptr<AsyncTlsConnection>> accepted_waiting_for_handshake_;

  // Connections are manipulated from multiple threads. The io_service thread creates them and runs callbacks on them.
  // Senders find a connection through this map and push data onto the outQueue.
  mutable std::mutex connectionsGuard_;
  std::map<NodeNum, std::shared_ptr<AsyncTlsConnection>> connections_;
};

}  // namespace bftEngine
