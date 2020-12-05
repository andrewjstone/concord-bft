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
#include <mutex>

class AsyncTlsConnection;

namespace bft::communication {

// Any message attempted to be put on the queue that causes the total size of the queue to exceed
// this value will be dropped. This is to prevent indefinite backups and useless stale messages.
// The number is very large right now so as not to affect current setups. In the future we will
// have better admission control.
static constexpr size_t MAX_QUEUE_SIZE_IN_BYTES = 1024 * 1024 * 1024;  // 1 GB
static constexpr size_t MSG_HEADER_SIZE = 4;

struct OutgoingMsg {
  OutgoingMsg(const char* msg, const size_t len)
      : msg(len + MSG_HEADER_SIZE), send_time(std::chrono::steady_clock::now()) {
    uint32_t msg_size = htonl(static_cast<uint32_t>(len));
    std::memcpy(msg.data(), &msg_size, MSG_HEADER_SIZE);
    std::memcpy(msg.data() + MSG_HEADER_SIZE, msg, len);
  }
  std::vector<char> msg;
  std::chrono::steady_clock::time_point send_time;
};

// A write queue is a buffer of messages for a single socket. Messages should only be put on the
// queue if there is an active connection associated with it. By separating the queue fom the
// connection however, we can allow optimistic pushes and not have to worry about locking the map of
// connnections. This increase concurrency as a lock is only held when a message is pushed or popped
// on a queue for a single connection, rather than an additional lock across all conenctions.
// Furthermore, we only have to hold the lock when a request is pushed or popped, which is very
// short.
class WriteQueue {
 public:
  WriteQueue(NodeNum destination) : destination_(destination) {}

  void connect(const std::shared_ptr<AsyncTlsConnection>& conn) {
    std::lock_guard<std::mutex> guard(lock_);
    conn_ = conn;
    connected_ = true;
  }

  void disconnect() {
    std::lock_guard<std::mutex> guard(lock_);
    conn_.reset();
    connected_ = false;
  }

  bool connected() const { return connected_; }

  // Only add onto the queue if there is an active connection. Return the size of the queue after
  // the push completes or std::nullopt if there is no connection, or the queue is full.
  std::option<size_t> push(const char* raw_msg, const size_t len) {
    if (!connected_) {
      return std::nullopt;
    }
    auto msg = OutgoingMsg(raw_msg, len);
    std::lock_guard<std::mutex> guard(lock_);
    if (queued_size_in_bytes > MAX_QUEUE_SIZE_IN_BYTES) {
      // TODO: Log this as a warning
      return std::nullopt;
    }
    queued_size_in_bytes_ += msg.size();
    msgs_.push_back(std::move(msg));
  }

  std::option<OutgoingMsg> pop() {
    std::lock_guard<std::mutex> guard(lock_);
    if (msgs_.empty()) {
      return std::nullopt;
    }
    auto msg = std::move(msgs_.front());
    msgs_.pop_front();
    queued_size_in_bytes -= msg.size();
    return msg;
  }

  void clear() {
    std::lock_guard<std::mutex> guard(lock_);
    msgs_.clear();
    queued_size_in_bytes_ = 0;
  }

  size_t size() const {
    std::lock_guard<std::mutex> guard(lock_);
    return msgs_.size();
  }

  size_t sizeInBytes() const {
    std::lock_guard<std::mutex> guard(lock_);
    return queued_size_in_bytes_;
  }

  std::shared_ptr<AsyncTlsConnection> getConn() {
    std::lock_guard<std::mutex> guard(lock_);
    return conn_;
  }

 private:
  // Protects `msgs_`, `queued_size_in_bytes_`, and `conn_`
  mutable std::mutex lock_;
  std::mutex std::deque<OutgoingMsg> msgs_;
  size_t queued_size_in_bytes_ = 0;
  std::shared_ptr<AsyncTlsConnection> > conn_;

  // We purposefully do not take any locks to check this. It is an optimistic check. It's ok to drop
  // a message if a connection just occurred. It's also simultaneously ok to put a message onto the
  // queue if a connection has just dropped.
  std::atomic_bool connected_ = false;
};

}  // namespace bft::communication
