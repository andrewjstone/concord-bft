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

#pragma once

#include <deque>
#include <vector>
#include <mutex>
#include <optional>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>

#include "Logger.hpp"
#include "CommDefs.hpp"

namespace bftEngine {

class TlsTcpImpl;

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> SSL_SOCKET;

class AsyncTlsConnection : public std::enable_shared_from_this<AsyncTlsConnection> {
 public:
  // Any message attempted to be put on the queue that causes the total size of the queue to exceed
  // this value will be dropped.
  static constexpr uint32_t MSG_HEADER_SIZE = 4;
  static constexpr size_t MAX_QUEUE_SIZE_IN_BYTES = 64 * 1024 * 1024;  // 64 MB
  static constexpr std::chrono::seconds READ_TIMEOUT = std::chrono::seconds(10);
  static constexpr std::chrono::seconds WRITE_TIMEOUT = READ_TIMEOUT;

  // A outgoing message that has been queued for STALE_MESSAGE_TIMEOUT is likely pretty useless. If it takes this long
  // to send the message, the connection or receiver is overloaded, and so we want to drop the message to shed load.
  static constexpr std::chrono::seconds STALE_MESSAGE_TIMEOUT = std::chrono::seconds(5);

  AsyncTlsConnection(std::unique_ptr<SSL_SOCKET>&& socket, IReceiver* receiver, TlsTCPCommunication::TlsTcpImpl& impl)
      : logger_(concordlogger::Log::getLogger("concord-bft.tls.conn")),
        socket_(std::move(socket)),
        receiver_(receiver),
        tlsTcpImpl_(impl),
        read_timer_(socket_->get_io_service()),
        write_timer_(socket_->get_io_service()) {}

  AsyncTlsConnection(std::unique_ptr<SSL_SOCKET>&& socket,
                     IReceiver* receiver,
                     TlsTCPCommunication::TlsTcpImpl& impl,
                     NodeNum peer_id)
      : logger_(concordlogger::Log::getLogger("concord-bft.tls.conn")),
        socket_(std::move(socket)),
        peer_id_(peer_id),
        receiver_(receiver),
        tlsTcpImpl_(impl),
        read_timer_(socket_->get_io_service()),
        write_timer_(socket->get_io_service()) {}

  void send(std::vector<char>&& msg);
  void setPeerId(NodeNum peer_id) { peer_id_ = peer_id; }
  std::optional<NodeNum> getPeerId() { return peer_id_; }
  SSL_SOCKET& getSocket() { return *socket_.get(); }
  std::array<char, MSG_HEADER_SIZE>& getReadSizeBuf() { return read_size_buf_; }

  // Every messsage is preceded by a 4 byte message header that we must read.
  void readMsgSizeHeader();

 private:
  // Clean up the connection
  void dispose();

  // We know the size of the message and that a message should be forthcoming. We start a timer and
  // ensure we read all remaining bytes within a given timeout. If we read the full message we
  // inform the `receiver_`, otherwise we tell the `tlsTcpImpl` that the connection should be
  // removed and closed.
  void readMsg();

  // Return the recently read size header as an integer. Assume little-endian byte order.
  uint32_t getReadMsgSize();

  void startReadTimer();
  void startWriteTimer();

  void write();

  concordlogger::Logger logger_;

  // We can't rely on timer callbacks to actually be cancelled properly and return
  // `boost::asio::error::operation_aborted`. There is a race condition where a callback may already
  // be queued before the timer was cancelled. If we destroy the socket due to a failed write, and
  // then the callback fires, we may have already closed the socket, and we don't want to do that
  // twice.
  bool disposed_ = false;

  std::unique_ptr<SSL_SOCKET> socket_;
  std::optional<NodeNum> peer_id_ = std::nullopt;

  // We assume `receiver_` lives at least as long as each connection.
  IReceiver* receiver_ = nullptr;

  // We assume `tlsTcpImpl_` lives at least as long as each connection
  TlsTCPCommunication::TlsTcpImpl& tlsTcpImpl_;

  boost::asio::steady_timer read_timer_;
  boost::asio::steady_timer write_timer_;

  // On every read, we must read the size of the incoming message first. This buffer stores that size.
  std::array<char, MSG_HEADER_SIZE> read_size_buf_;

  // Last read message
  std::vector<char> read_msg_;

  // We must maintain ownership of the in_flight_message until the asio::buffer wrapping it has actually been sent by
  // the underling io_service. We will know this is the case when the write completion handler gets called.
  struct OutgoingMsg {
    OutgoingMsg(std::vector<char>&& msg) : msg(msg), send_time(std::chrono::steady_clock::now()) {}

    std::vector<char> msg;
    std::chrono::steady_clock::time_point send_time;
  };

  std::mutex write_lock_;
  std::deque<OutgoingMsg> out_queue_;

  // This includes in_flight_message_;
  size_t queued_size_in_bytes_ = 0;
};

}  // namespace bftEngine