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

#include <regex>

#include <boost/filesystem.hpp>

#include "assertUtils.hpp"
#include "TlsTcpImpl.h"

namespace bftEngine {

void TlsTCPCommunication::TlsTcpImpl::Start() {
  std::lock_guard<std::mutex> l(startStopGuard_);
  if (io_thread_) return;

  // Start the io_thread_;
  io_thread_.reset(new std::thread([this]() {
    // All replicas must listen
    if (config_.selfId <= (size_t)config_.maxServerId) {
      try {
        auto endpoint = resolve();
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(LISTEN_BACKLOG);
      } catch (const boost::system::system_error& e) {
        LOG_FATAL(logger_,
                  "Faield to start TlsTCPImpl acceptor at " << config_.listenHost << ":" << config_.listenPort
                                                            << " for node " << config_.selfId << ": " << e.what());
        Assert(false);
      }
      accept();
    }
    connect();

    // We must start connecting and accepting before we start the io_service, so that it has some
    // work to do. This is what prevents the io_service event loop from exiting immediately.
    io_service_.run();
  }));
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
    std::vector<char> owned(msg, msg + len);
    temp->second.send(std::move(owned));
  } else {
    LOG_DEBUG(logger_, "Connection NOT found, from: " << config_.selfId << ", to: " << destination);
  }
}  // namespace bftEngine

void setSocketOptions(boost::asio::ip::tcp::socket& socket) { socket.set_option(boost::asio::ip::tcp::no_delay(true)); }

boost::asio::ssl::context TlsTCPCommunication::TlsTcpImpl::createServerSSLContext() {
  boost::asio::ssl::context context(boost::asio::ssl::context::tlsv12_server);
  context.set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert);
  context.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                      boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::no_tlsv1 |
                      boost::asio::ssl::context::no_tlsv1_1 | boost::asio::ssl::context::single_dh_use);

  boost::system::error_code ec;
  auto accepted_connection_id = total_accepted_connections_;
  context.set_verify_callback(
      [this, accepted_connection_id](auto preverified, auto& ctx) -> bool {
        return verifyCertificateServer(preverified, ctx, accepted_connection_id);
      },
      ec);
  if (ec) {
    LOG_ERROR(logger_, "Unable to set server verify callback" << ec.message());
    abort();
  }

  namespace fs = boost::filesystem;
  auto path = fs::path(config_.certificatesRootPath) / fs::path(std::to_string(config_.selfId)) / fs::path("server");
  context.use_certificate_chain_file((path / fs::path("server.cert")).string());
  context.use_private_key_file((path / fs::path("pk.pem")).string(), boost::asio::ssl::context::pem);

  EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_secp384r1);
  if (!ecdh) {
    LOG_ERROR(logger_, "Unable to create EC");
    abort();
  }

  if (1 != SSL_CTX_set_tmp_ecdh(context.native_handle(), ecdh)) {
    LOG_ERROR(logger_, "Unable to set temp EC params");
    abort();
  }

  // As OpenSSL does reference counting, it should be safe to free the key.
  // However, there is no explicit info on this point in the openssl docs.
  // This info is from various online sources and examples
  EC_KEY_free(ecdh);

  // Only allow using the strongest cipher suites.
  SSL_CTX_set_cipher_list(context.native_handle(), config_.cipherSuite.c_str());
  return context;
}

boost::asio::ssl::context TlsTCPCommunication::TlsTcpImpl::createClientSSLContext(NodeNum destination) {
  boost::asio::ssl::context context(boost::asio::ssl::context::tlsv12_client);
  context.set_verify_mode(boost::asio::ssl::verify_peer);

  namespace fs = boost::filesystem;
  auto path = fs::path(config_.certificatesRootPath) / fs::path(std::to_string(config_.selfId)) / "client";
  auto serverPath = fs::path(config_.certificatesRootPath) / fs::path(std::to_string(destination)) / "server";

  boost::system::error_code ec;
  context.set_verify_callback(
      [this, destination](auto preverified, auto& ctx) -> bool {
        return verifyCertificateClient(preverified, ctx, destination);
      },
      ec);
  if (ec) {
    LOG_ERROR(logger_, "Unable to set client verify callback" << ec.message());
    abort();
  }

  context.use_certificate_chain_file((path / "client.cert").string());
  context.use_private_key_file((path / "pk.pem").string(), boost::asio::ssl::context::pem);

  // Only allow using the strongest cipher suites.
  SSL_CTX_set_cipher_list(context.native_handle(), config_.cipherSuite.c_str());
  return context;
}

std::pair<bool, NodeNum> TlsTCPCommunication::TlsTcpImpl::checkCertificate(X509* receivedCert,
                                                                           std::string connectionType,
                                                                           std::string subject,
                                                                           std::optional<NodeNum> expectedPeerId) {
  // First, perform a basic sanity test, in order to eliminate a disk read if the certificate is
  // unknown.
  //
  // The certificate must have a node id, as we put it in `OU` field on creation.
  //
  // Since we use pinning we must know who the remote peer is.
  // `peerIdPrefixLength` stands for the length of 'OU=' substring
  int peerIdPrefixLength = 3;
  std::regex r("OU=\\d*", std::regex_constants::icase);
  std::smatch sm;
  regex_search(subject, sm, r);
  if (sm.length() <= peerIdPrefixLength) {
    LOG_ERROR(logger_, "OU not found or empty: " << subject);
    return std::make_pair(false, 0);
  }

  auto remPeer = sm.str().substr(peerIdPrefixLength, sm.str().length() - peerIdPrefixLength);
  if (0 == remPeer.length()) {
    LOG_ERROR(logger_, "OU empty " << subject);
    return std::make_pair(false, 0);
  }

  NodeNum remotePeerId;
  try {
    remotePeerId = stoul(remPeer, nullptr);
  } catch (const std::invalid_argument& ia) {
    LOG_ERROR(logger_, "cannot convert OU, " << subject << ", " << ia.what());
    return std::make_pair(false, 0);
  } catch (const std::out_of_range& e) {
    LOG_ERROR(logger_, "cannot convert OU, " << subject << ", " << e.what());
    return std::make_pair(false, 0);
  }

  // If the server has been verified, check that the peers match.
  if (expectedPeerId) {
    if (remotePeerId != expectedPeerId) {
      LOG_ERROR(logger_, "Peers don't match, expected: " << expectedPeerId.value() << ", received: " << remPeer);
      return std::make_pair(false, remotePeerId);
    }
  }

  // the actual pinning - read the correct certificate from the disk and
  // compare it to the received one
  namespace fs = boost::filesystem;
  auto path = fs::path(config_.certificatesRootPath) / std::to_string(remotePeerId) / connectionType /
              std::string(connectionType + ".cert");

  FILE* fp = fopen(path.c_str(), "r");
  if (!fp) {
    LOG_ERROR(logger_, "Certificate file not found, path: " << path);
    return std::make_pair(false, remotePeerId);
  }

  X509* localCert = PEM_read_X509(fp, NULL, NULL, NULL);
  if (!localCert) {
    LOG_ERROR(logger_, "Cannot parse certificate, path: " << path);
    fclose(fp);
    return std::make_pair(false, remotePeerId);
  }

  // this is actual comparison, compares hash of 2 certs
  int res = X509_cmp(receivedCert, localCert);
  X509_free(localCert);
  fclose(fp);
  if (res == 0) {
    LOG_INFO(logger_,
             "Connection authenticated at node: " << config_.selfId << ", type: " << connectionType
                                                  << ", peer: " << remotePeerId);
    return std::make_pair(true, remotePeerId);
  }
  return std::make_pair(false, remotePeerId);
}

void TlsTCPCommunication::TlsTcpImpl::setVerifiedPeerId(size_t accepted_connection_id, NodeNum peer_id) {
  accepted_waiting_for_handshake_.at(accepted_connection_id).setPeerId(peer_id);
}

bool TlsTCPCommunication::TlsTcpImpl::verifyCertificateServer(bool preverified,
                                                              boost::asio::ssl::verify_context& ctx,
                                                              size_t accepted_connection_id) {
  char subject[512];
  X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
  if (!cert) {
    LOG_ERROR(logger_, "No certificate from client");
    return false;
  } else {
    X509_NAME_oneline(X509_get_subject_name(cert), subject, 512);
    auto [valid, peer_id] = checkCertificate(cert, "client", std::string(subject), std::nullopt);
    setVerifiedPeerId(accepted_connection_id, peer_id);
    return valid;
  }
}

bool TlsTCPCommunication::TlsTcpImpl::verifyCertificateClient(bool preverified,
                                                              boost::asio::ssl::verify_context& ctx,
                                                              NodeNum expected_dest_id) {
  char subject[256];
  X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
  if (!cert) {
    LOG_ERROR(logger_, "no certificate from server");
    return false;
  } else {
    X509_NAME_oneline(X509_get_subject_name(cert), subject, 256);
    LOG_DEBUG(_logger, "Verifying server: " << subject << ", " << preverified);

    bool res = checkCertificate(cert, "server", string(subject), expected_dest_id);
    LOG_DEBUG(_logger, "Manual verifying server: " << subject << ", authenticated: " << res);
    return res;
  }
}

void TlsTCPCommunication::TlsTCPImpl::closeConnection(AsyncTlsConnection&& conn) {
  conn.lowest_layer().cancel();
  conn.getSocket().async_shutdown([this, sock = std::move(conn.getSocket())](const auto& ec) {
    if (ec) {
      LOG_WARN(logger_, "SSL shutdown failed: " << ec.message());
    }
    sock.lowest_layer().close();
  });
}

void TlsTcpCommunication::TlsTcpImpl::onConnectionAuthenticated(AsyncTlsConnection&& conn) {
  // Move the connection into the accepted connections map If there is an existing connection
  // discard it. In this case it was likely that connecting end of the connection thinks there is
  // something wrong. This is a vector for a denial of service attack on the accepting side. We can
  // track the number of connections from the node and mark it malicious if necessary.
  std::lock_guard<std::mutex> lock(connectionsGuard_);
  auto it = connections_.find(conn.getPeerId());
  if (it != connections_.end()) {
    closeConnection(std::move(*it));
  }
  connections_.emplace({conn.getPeerId(), std::move(conn)});
}

void TlsTCPCommunication::TlsTcpImpl::onServerHandshakeComplete(const boost::system::error_code& ec,
                                                                size_t accepted_connection_id) {
  // The handshake either succeeded or failed. The accepted connection is no longer
  // waiting, so remove it from the waiting map.
  auto conn = std::move(accepted_waiting_for_handshake_.at(connection_id));
  accepted_waiting_for_handshake_.erase(connection_id);
  if (ec) {
    LOG_ERROR(logger_,
              "Server handshake failed for peer " << conn.getPeerId() ? conn.getPeerId()
                                                                      : "Unknown"
                                                                            << ": " << ec.message());
    return closeConnection(std::move(conn));
  }
  onConnectionAuthenticated(std::move(conn));
}

void TlsTCPCommunication::TlsTcpImpl::startSSLHandshake(boost::asio::ip::tcp::socket&& socket) {
  auto ssl_context = createServerSSLContext();
  auto connection_id = total_accepted_connections_;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket(std::move(socket), ssl_context);
  accepted_waiting_for_handshake_.emplace(total_accepted_connections_,
                                          AsyncTlsConnection(std::move(ssl_socket), receiver_));
  ssl_socket->async_handshake(
      boost::asio::ssl::stream_base::server,
      [this, connection_id](const boost::system::error_code& ec) { onServerHandshakeComplete(ec, connection_id); });
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
    startSSLHandshake(std::move(accepting_socket_));
    accept();
  });
}

boost::asio::ip::tcp::endpoint TlsTCPCommunication::TlsTcpImpl::resolve() {
  // TODO: When upgrading to boost 1.66 or later, when query is deprecated,
  // this should be changed to call the resolver.resolve overload that takes a
  // protocol, host, and service directly, instead of a query object. That
  // overload is not yet available in boost 1.64, which we're using today.
  boost::asio::ip::tcp::resolver::query query(
      boost::asio::ip::tcp::v4(), config_.listenHost, std::to_string(config_.listenPort));
  boost::asio::ip::tcp::resolver resolver(io_service_);
  boost::asio::ip::tcp::resolver::iterator results = resolver.resolve(query);
  boost::asio::ip::tcp::endpoint endpoint = *results;
  LOG_INFO(logger_, "Resolved " << config_.listenHost << ":" << config_.listenPort << " to " << endpoint);
  return endpoint;
}

}  // namespace bftEngine