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

#include "AsyncTlsConnection.h"
#include "TlsTcpImpl.h"

namespace bftEngine {

void AsyncTlsConnection::readMsgSizeHeader() {
  auto self = shared_from_this();
  async_read(socket_, boost::asio::buffer(read_size_buf_), [this, self](const auto& error_code, auto _) {
    if (error_code) {
      if (error_code == boost::asio::error::operation_aborted || disposed_) {
        // The socket has already been cleaned up and any references are invalid. Just return.
        return;
      } else {
        // Remove the connection as it is no longer valid, and then close it, cancelling any ongoing operations.
        LOG_ERROR(tlsTcpImpl_.logger_,
                  "Reading message size header failed for node " << peer_id_.value() << ": " << error_code.message());
        return dispose();
      }
    }
    // The message size header was read successfully.
    if (getReadMsgSize() > tlsTcpImpl_.config_.bufferLength) {
      LOG_ERROR(tlsTcpImpl_.logger_,
                "Message Size: " << getReadMsgSize() << " exceeds maximum: " << tlsTcpImpl_.config_.bufferLength
                                 << " for node " << peer_id_.value());
      return dispose();
    }
    readMsg();
  });
}

void AsyncTlsConnection::readMsg() {
  auto msg_size = getReadMsgSize();
  read_msg_ = std::vector<char>(msg_size, 0);
  auto self = shared_from_this();
  async_read(socket_, boost::asio::buffer(read_msg_), [this, self](const auto& error_code, auto _) {
    if (error_code) {
      if (error_code == boost::asio::error::operation_aborted || disposed_) {
        // The socket has already been cleaned up and any references are invalid. Just return.
        return;
      } else {
        // Remove the connection as it is no longer valid, and then close it, cancelling any ongoing operations.
        LOG_ERROR(tlsTcpImpl_.logger_,
                  "Reading message of size <<" << getReadMsgSize() << " failed for node " << peer_id_.value() << ": "
                                               << error_code.message());
        return dispose();
      }
      // Read succeeded.
      receiver_->onNewMessage(peer_id_.value(), read_msg_.data(), read_msg_.size());
      boost::system::error_code _;
      read_timer_.cancel(_);
      readMsgSizeHeader();
    }
  });
}

void AsyncTlsConnection::startReadTimer() {
  auto self = shared_from_this();
  read_timer_.expires_from_now(READ_TIMEOUT);
  read_timer_.async_wait([this, self](const boost::system::error_code& ec) {
    if (ec) {
      if (ec == boost::asio::error::operation_aborted || disposed_) {
        // The socket has already been cleaned up and any references are invalid. Just return.
        return;
      }
      LOG_WARN(tlsTcpImpl_.logger_, "Read timeout from node " << peer_id_.value() << ": " << ec.message());
      dispose();
    }
  });
}

void AsyncTlsConnection::dispose() {
  LOG_ERROR(tlsTcpImpl_.logger_, "Closing connection to node " << peer_id_.value());
  disposed_ = true;
  tlsTcpImpl_.closeConnection(peer_id_.value());
}

uint32_t AsyncTlsConnection::getReadMsgSize() {
  uint32_t size = read_size_buf_[3] << 24 | read_size_buf_[2] << 16 | read_size_buf_[1] << 8 | read_size_buf_[0];
  return size;
}

}  // namespace bftEngine
