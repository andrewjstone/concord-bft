// Concord
//
// Copyright (c) 2018-2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the sub-component's license, as noted in the LICENSE
// file.

#include "TimeServiceManager.hpp"
#include "ReplicaConfig.hpp"
#include "gtest/gtest.h"
#include "serialize.hpp"
#include <chrono>

using namespace bftEngine;

struct ReservedPagesMock : public IReservedPages {
  mutable bool is_first_load = true;
  std::string page_ = std::string(sizeof(ConsensusTime::rep), 0);
  ReservedPagesMock() { ReservedPagesClientBase::setReservedPages(this); }
  ~ReservedPagesMock() { ReservedPagesClientBase::setReservedPages(nullptr); }
  virtual uint32_t numberOfReservedPages() const { return 1; };
  virtual uint32_t sizeOfReservedPage() const { return page_.size(); };
  virtual bool loadReservedPage(uint32_t reservedPageId, uint32_t copyLength, char* outReservedPage) const {
    if (is_first_load) {
      is_first_load = false;
      return false;
    }
    (void)reservedPageId;
    memcpy(outReservedPage, page_.c_str(), copyLength);
    return true;
  };
  virtual void saveReservedPage(uint32_t reservedPageId, uint32_t copyLength, const char* inReservedPage) {
    (void)reservedPageId;
    page_ = std::string(inReservedPage, inReservedPage + copyLength);
  };
  virtual void zeroReservedPage(uint32_t reservedPageId) {
    (void)reservedPageId;
    page_ = std::string(sizeof(ConsensusTime::rep), 0);
  };
};

struct FakeClock {
  static std::chrono::milliseconds current_time;
  static std::chrono::system_clock::time_point now() { return std::chrono::system_clock::time_point{current_time}; }
};

std::chrono::milliseconds FakeClock::current_time = std::chrono::milliseconds::min();

TEST(TimeServiceManager, TimeWithinLimits) {
  ReservedPagesMock m;
  auto& config = ReplicaConfig::instance();
  config.timeServiceEnabled = true;
  config.timeServiceEpsilonMillis = std::chrono::milliseconds{1};
  config.timeServiceHardLimitMillis = std::chrono::seconds{3};
  config.timeServiceSoftLimitMillis = std::chrono::milliseconds{500};

  const auto now = ConsensusTime{1000};
  FakeClock::current_time = now;

  auto manager = TimeServiceManager<FakeClock>{};
  const auto msg = manager.createClientRequestMsg();

  EXPECT_TRUE(manager.isPrimarysTimeWithinBounds(*msg));
}

TEST(TimeServiceManager, TimeOutOfHardLimits) {
  ReservedPagesMock m;
  auto& config = ReplicaConfig::instance();
  config.timeServiceEnabled = true;
  config.timeServiceEpsilonMillis = std::chrono::milliseconds{1};
  config.timeServiceHardLimitMillis = std::chrono::seconds{3};
  config.timeServiceSoftLimitMillis = std::chrono::milliseconds{500};

  const auto now = ConsensusTime{1000};
  FakeClock::current_time = now;

  auto manager = TimeServiceManager<FakeClock>{};
  const auto msg = manager.createClientRequestMsg();

  FakeClock::current_time = now + config.timeServiceHardLimitMillis + std::chrono::milliseconds{1};
  EXPECT_FALSE(manager.isPrimarysTimeWithinBounds(*msg));

  FakeClock::current_time = now - config.timeServiceHardLimitMillis - std::chrono::milliseconds{1};
  EXPECT_FALSE(manager.isPrimarysTimeWithinBounds(*msg));
}

TEST(TimeServiceManager, TimeOnTheEdgeOfHardLimits) {
  ReservedPagesMock m;
  auto& config = ReplicaConfig::instance();
  config.timeServiceEnabled = true;
  config.timeServiceEpsilonMillis = std::chrono::milliseconds{1};
  config.timeServiceHardLimitMillis = std::chrono::seconds{3};
  config.timeServiceSoftLimitMillis = std::chrono::milliseconds{500};

  const auto now = ConsensusTime{1000};
  FakeClock::current_time = now;

  auto manager = TimeServiceManager<FakeClock>{};
  const auto msg = manager.createClientRequestMsg();

  FakeClock::current_time = now + config.timeServiceHardLimitMillis;
  EXPECT_TRUE(manager.isPrimarysTimeWithinBounds(*msg));

  FakeClock::current_time = now - config.timeServiceHardLimitMillis;
  EXPECT_TRUE(manager.isPrimarysTimeWithinBounds(*msg));
}

TEST(TimeServiceManager, TimeOutOfSoftLimits) {
  ReservedPagesMock m;
  auto& config = ReplicaConfig::instance();
  config.timeServiceEnabled = true;
  config.timeServiceEpsilonMillis = std::chrono::milliseconds{1};
  config.timeServiceHardLimitMillis = std::chrono::seconds{3};
  config.timeServiceSoftLimitMillis = std::chrono::milliseconds{500};

  const auto now = ConsensusTime{1000};
  FakeClock::current_time = now;

  auto manager = TimeServiceManager<FakeClock>{};
  const auto msg = manager.createClientRequestMsg();

  FakeClock::current_time = now + config.timeServiceSoftLimitMillis + std::chrono::milliseconds{1};
  EXPECT_TRUE(manager.isPrimarysTimeWithinBounds(*msg));

  FakeClock::current_time = now - config.timeServiceSoftLimitMillis - std::chrono::milliseconds{1};
  EXPECT_TRUE(manager.isPrimarysTimeWithinBounds(*msg));
}

TEST(TimeServiceManager, TimeOnTheEdgeOfSoftLimits) {
  ReservedPagesMock m;
  auto& config = ReplicaConfig::instance();
  config.timeServiceEnabled = true;
  config.timeServiceEpsilonMillis = std::chrono::milliseconds{1};
  config.timeServiceHardLimitMillis = std::chrono::seconds{3};
  config.timeServiceSoftLimitMillis = std::chrono::milliseconds{500};

  const auto now = ConsensusTime{1000};
  FakeClock::current_time = now;

  auto manager = TimeServiceManager<FakeClock>{};
  const auto msg = manager.createClientRequestMsg();

  FakeClock::current_time = now + config.timeServiceSoftLimitMillis;
  EXPECT_TRUE(manager.isPrimarysTimeWithinBounds(*msg));

  FakeClock::current_time = now - config.timeServiceSoftLimitMillis;
  EXPECT_TRUE(manager.isPrimarysTimeWithinBounds(*msg));
}

TEST(TimeServiceManager, CompareAndSwap) {
  ReservedPagesMock m;
  auto& config = ReplicaConfig::instance();
  config.timeServiceEnabled = true;
  config.timeServiceEpsilonMillis = std::chrono::milliseconds{1};
  config.timeServiceHardLimitMillis = std::chrono::seconds{3};
  config.timeServiceSoftLimitMillis = std::chrono::milliseconds{500};

  const auto now = ConsensusTime{1000};
  auto manager = TimeServiceManager{};

  // check that if now does not move, the manager increases it by epsilon
  for (size_t i = 0U; i < 10; ++i) {
    EXPECT_EQ(now + (config.timeServiceEpsilonMillis * i), manager.compareAndSwap(now));
  }
}

TEST(TimeServiceManager, CreateClientRequestMsg) {
  ReservedPagesMock m;
  auto& config = ReplicaConfig::instance();
  config.timeServiceEnabled = true;
  config.timeServiceEpsilonMillis = std::chrono::milliseconds{1};
  config.timeServiceHardLimitMillis = std::chrono::seconds{3};
  config.timeServiceSoftLimitMillis = std::chrono::milliseconds{500};

  const auto now = ConsensusTime{1000};
  FakeClock::current_time = now;

  auto manager = TimeServiceManager<FakeClock>{};

  const auto msg = manager.createClientRequestMsg();
  EXPECT_EQ(now,
            concord::util::deserialize<ConsensusTime>(msg->requestBuf(), msg->requestBuf() + msg->requestLength()));
  EXPECT_EQ(MsgFlag::TIME_SERVICE_FLAG, msg->flags());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
