// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "SourceSelector.hpp"

namespace bftEngine {
namespace SimpleBlockchainStateTransfer {
namespace impl {

bool SourceSelector::hasSource() const {
  return currentReplica_ != NO_REPLICA && sourceSelectionTimeMilli_ > 0;
}

void SourceSelector::setSourceSelectionTime(uint64_t currTimeMilli) {
  sourceSelectionTimeMilli_ = currTimeMilli;
}

void SourceSelector::removeCurrentReplica() {
  preferredReplicas_.erase(currentReplica_);
  currentReplica_ = NO_REPLICA;
}

void SourceSelector::setAllReplicasAsPreferred() { preferredReplicas_ = allOtherReplicas_; }

void SourceSelector::reset() {
  preferredReplicas_.clear();
  currentReplica_ = NO_REPLICA;
  sourceSelectionTimeMilli_ = 0;
}

bool SourceSelector::isReset() const {
  return preferredReplicas_.empty() && currentReplica_ == NO_REPLICA && sourceSelectionTimeMilli_ == 0;
}

bool SourceSelector::retransmissionTimeoutExpired(uint64_t currTimeMilli) const {
  // TODO(GG): TBD - compute dynamically
  return timeSinceSourceSelectedMilli(currTimeMilli) > retransmissionTimeoutMilli_;
}

uint64_t SourceSelector::timeSinceSourceSelectedMilli(uint64_t currTimeMilli) const {
  return ((currentReplica_ == NO_REPLICA) || (currTimeMilli < sourceSelectionTimeMilli_))
             ? 0
             : (currTimeMilli - sourceSelectionTimeMilli_);
}

// Replace the source if necessary.
//
// Return true if the source was updated, false otherwise.
bool SourceSelector::updateSource(bool badDataFromCurrentSource, uint64_t currTimeMilli) {
  if (shouldReplaceSource(currTimeMilli, badDataFromCurrentSource)) {
    if (currentReplica_ != NO_REPLICA) {
      preferredReplicas_.erase(currentReplica_);
    }
    if (preferredReplicas_.size() == 0) {
      preferredReplicas_ = allOtherReplicas_;
    }
    selectSource(currTimeMilli);
    return true;
  }
  return false;
}

// Create a list of ids of the form "0, 1, 4"
std::string SourceSelector::preferredReplicasToString() const {
  std::ostringstream oss;
  for (auto it = preferredReplicas_.begin(); it != preferredReplicas_.end(); ++it) {
    if (it == preferredReplicas_.begin()) {
      oss << *it;
    } else {
      oss << ", " << *it;
    }
  }
  return oss.str();
}

bool SourceSelector::shouldReplaceSource(uint64_t currTimeMilli, bool badDataFromCurrentSource) const {
  return currentReplica_ == NO_REPLICA || badDataFromCurrentSource ||
         timeSinceSourceSelectedMilli(currTimeMilli) > sourceReplacementTimeoutMilli_;
}

void SourceSelector::selectSource(uint64_t currTimeMilli) {
  const size_t size = preferredReplicas_.size();
  Assert(size > 0);

  auto i = preferredReplicas_.begin();
  if (size > 1) {
    // TODO(GG): can be optimized
    unsigned int c = randomGen_() % size;
    while (c > 0) {
      c--;
      i++;
    }
  }
  currentReplica_ = *i;
  sourceSelectionTimeMilli_ = currTimeMilli;
}
}  // namespace impl
}  // namespace SimpleBlockchainStateTransfer
}  // namespace bftEngine
