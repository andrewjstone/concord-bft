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
  async_read(*socket_, boost::asio::buffer(read_size_buf_), [this, self](const auto& error_code, auto _) {
    if (error_code) {
      if (error_code == boost::asio::error::operation_aborted || disposed_) {
        // The socket has already been cleaned up and any references are invalid. Just return.
        return;
      } else {
        // Remove the connection as it is no longer valid, and then close it, cancelling any ongoing operations.
        LOG_ERROR(logger_,
                  "Reading message size header failed for node " << peer_id_.value() << ": " << error_code.message());
        return dispose();
      }
    }
    // The message size header was read successfully.
    if (getReadMsgSize() > tlsTcpImpl_.config_.bufferLength) {
      LOG_ERROR(logger_,
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
  startReadTimer();
  async_read(*socket_, boost::asio::buffer(read_msg_), [this, self](const auto& error_code, auto _) {
    if (error_code) {
      if (error_code == boost::asio::error::operation_aborted || disposed_) {
        // The socket has already been cleaned up and any references are invalid. Just return.
        return;
      } else {
        // Remove the connection as it is no longer valid, and then close it, cancelling any ongoing operations.
        LOG_ERROR(logger_,
                  "Reading message of size <<" << getReadMsgSize() << " failed for node " << peer_id_.value() << ": "
                                               << error_code.message());
        return dispose();
      }
      // The Read succeeded.
      boost::system::error_code _;
      read_timer_.cancel(_);
      receiver_->onNewMessage(peer_id_.value(), read_msg_.data(), read_msg_.size());
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
      LOG_WARN(logger_, "Read timeout from node " << peer_id_.value() << ": " << ec.message());
      dispose();
    }
  });
}

void AsyncTlsConnection::startWriteTimer() {
  auto self = shared_from_this();
  write_timer_.expires_from_now(WRITE_TIMEOUT);
  write_timer_.async_wait([this, self](const boost::system::error_code& ec) {
    if (ec) {
      if (ec == boost::asio::error::operation_aborted || disposed_) {
        // The socket has already been cleaned up and any references are invalid. Just return.
        return;
      }
      LOG_WARN(logger_, "Write timeout to node " << peer_id_.value() << ": " << ec.message());
      dispose();
    }
  });
}

uint32_t AsyncTlsConnection::getReadMsgSize() {
  uint32_t size = read_size_buf_[3] << 24 | read_size_buf_[2] << 16 | read_size_buf_[1] << 8 | read_size_buf_[0];
  return size;
}

void AsyncTlsConnection::dispose() {
  assert(!disposed_);
  LOG_ERROR(logger_, "Closing connection to node " << peer_id_.value());
  disposed_ = true;
  boost::system::error_code _;
  read_timer_.cancel(_);
  write_timer_.cancel(_);
  tlsTcpImpl_.closeConnection(peer_id_.value());
}

void AsyncTlsConnection::send(std::vector<char>&& raw_msg) {
  assert(raw_msg.size() > MSG_HEADER_SIZE && raw_msg.size() <= tlsTcpImpl_.config_.bufferLength);

  std::lock_guard<std::mutex> guard(write_lock_);
  if (queued_size_in_bytes_ + raw_msg.size() > MAX_QUEUE_SIZE_IN_BYTES) {
    LOG_WARN(logger_, "Outgoing Queue is full. Dropping message with size: " << raw_msg.size());
    return;
  }

  out_queue_.emplace_back(OutgoingMsg(std::move(raw_msg)));
  if (out_queue_.size() == 1) {
    write();
  }
}

void AsyncTlsConnection::write() {
  std::lock_guard<std::mutex> guard(write_lock_);
  while (!out_queue_.empty() &&
         out_queue_.front().send_time + STALE_MESSAGE_TIMEOUT > std::chrono::steady_clock::now()) {
    out_queue_.pop_front();
  }
  if (out_queue_.empty()) {
    return;
  }

  auto self = shared_from_this();
  startWriteTimer();
  boost::asio::async_write(
      *socket_, boost::asio::buffer(out_queue_.front().msg), [this, self](const boost::system::error_code& ec, auto _) {
        if (ec) {
          if (ec == boost::asio::error::operation_aborted || disposed_) {
            // The socket has already been cleaned up and any references are invalid. Just return.
            return;
          }
          LOG_WARN(logger_,
                   "Write failed to node " << peer_id_.value() << " for message with size "
                                           << out_queue_.front().msg.size() << ": " << ec.message());
          return dispose();
        }
        // The write succeeded.
        write_timer_.cancel();
        std::lock_guard<std::mutex> guard(write_lock_);
        out_queue_.pop_front();
        if (!out_queue_.empty()) {
          write();
        }
      });
}

}  // namespace bftEngine
