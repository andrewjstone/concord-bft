# Concord
#
# Copyright (c) 2019 VMware, Inc. All Rights Reserved.
#
# This product is licensed to you under the Apache 2.0 license (the "License").
# You may not use this product except in compliance with the Apache 2.0 License.
#
# This product may include a number of subcomponents with separate copyright
# notices and license terms. Your use of these subcomponents is subject to the
# terms and conditions of the subcomponent's license, as noted in the LICENSE
# file.

import time
from enum import Enum

from bft_test_exceptions import(
    ConflictingBlockWriteError,
    StaleReadError,
    NoConflictError,
    InvalidReadError
)

class SkvbcWriteRequest:
    """
    A write request sent to an Skvbc cluster. A request may or may not complete.
    """
    def __init__(self, client_id, seq_num, readset, writeset, read_block_id=0):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num
        self.readset = readset
        self.writeset = writeset
        self.read_block_id = read_block_id

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  timestamp={self.timestamp}\n'
           f'  client_id={self.client_id}\n'
           f'  seq_num={self.seq_num}\n'
           f'  readset={self.readset}\n'
           f'  writeset={self.writeset}\n'
           f'  read_block_id={self.read_block_id}\n')

    def writeset_keys(self):
        return set(self.writeset.keys())

class SkvbcReadRequest:
    """
    A read request sent to an Skvbc cluster. A request may or may not complete.
    """
    def __init__(self, client_id, seq_num, readset, read_block_id=0):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num
        self.readset = readset
        self.read_block_id = read_block_id

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  timestamp={self.timestamp}\n'
           f'  client_id={self.client_id}\n'
           f'  seq_num={self.seq_num}\n'
           f'  readset={self.readset}\n'
           f'  read_block_id={self.read_block_id}\n')

class SkvbcGetLastBlockReq:
    """
    A GET_LAST_BLOCK request sent to an skvbc cluster. A request may or may not
    complete.
    """
    def __init__(self, client_id, seq_num):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  timestamp={self.timestamp}\n'
           f'  client_id={self.client_id}\n'
           f'  seq_num={self.seq_num}\n')

class SkvbcWriteReply:
    """A reply to an outstanding write request sent to an Skvbc cluster."""
    def __init__(self, client_id, seq_num, reply):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num
        self.reply = reply

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  timestamp={self.timestamp}\n'
           f'  client_id={self.client_id}\n'
           f'  seq_num={self.seq_num}\n'
           f'  reply={self.reply}\n')

class SkvbcReadReply:
    """A reply to an outstanding read request sent to an Skvbc cluster."""
    def __init__(self, client_id, seq_num, kvpairs):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num
        self.kvpairs = kvpairs

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  timestamp={self.timestamp}\n'
           f'  client_id={self.client_id}\n'
           f'  seq_num={self.seq_num}\n'
           f'  kvpairs={self.kvpairs}\n')

class SkvbcGetLastBlockReply:
    """
    A reply to an outstanding get last block request sent to an Skvbc cluster.
    """
    def __init__(self, client_id, seq_num, reply):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num
        self.reply = reply

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  timestamp={self.timestamp}\n'
           f'  client_id={self.client_id}\n'
           f'  seq_num={self.seq_num}\n'
           f'  reply={self.reply}\n')

class Result(Enum):
   """
   Whether an operation succeeded, failed, or the result is unknown
   """
   WRITE_SUCCESS = 1
   WRITE_FAIL = 2
   UNKNOWN_WRITE = 3
   UNKNOWN_READ = 4
   READ_REPLY = 5

class CompletedRead:
    def __init__(self, causal_state, kvpairs):
        self.causal_state = causal_state
        self.kvpairs = kvpairs

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  causal_state={self.causal_state}\n'
           f'  kvpairs={self.kvpairs}\n')

class CausalState:
    """Relevant state of the tracker before a request is started"""
    def __init__(
            self, req_index, last_known_block, last_consecutive_block, kvpairs):
        self.req_index = req_index
        self.last_known_block = last_known_block
        self.last_consecutive_block = last_consecutive_block

        # KV pairs contain the value up keys up until last_consecutive_block
        self.kvpairs = kvpairs

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'    last_known_block={self.last_known_block}\n'
           f'    last_consecutive_block={self.last_consecutive_block}\n'
           f'    kvpairs={self.kvpairs}\n')

class ConcurrentValue:
    """Track the state for a request / reply in self.concurrent"""
    def __init__(self, is_read):
        if is_read:
            self.result = Result.UNKNOWN_READ
        else:
            self.result = Result.UNKNOWN_WRITE

        # Only used in writes
        # Set only when writes succeed.
        self.written_block_id = None

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  result={self.result}\n'
           f'  written_block_id={self.written_block_id}\n')

class Block:
    def __init__(self, kvpairs, req_index=None):
        self.kvpairs = kvpairs
        self.req_index = req_index

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  kvpairs={self.kvpairs}\n'
           f'  req_index={self.req_index}\n')

class SkvbcTracker:
    """
    Track requests, expected and actual responses from SimpleKVBC test
    clusters.
    """
    def __init__(self):
        # Last block_id received in a response
        self.last_block_id = 0

        # A partial order of all requests (SkvbcWriteRequest | SkvbcReadRequest)
        # issued against SimpleKVBC.  History tracks requests and responses. A
        # happens-before relationship exists between responses and requests
        # launched after those responses.
        self.history = []

        # All currently outstanding requests:
        # (client_id, seq_num) -> CausalState
        self.outstanding = {}

        # All completed reads by index into history -> CompletedRead
        self.completed_reads = {}

        # All failed writes by index into history -> CausalState
        self.failed_writes = {}

        # A set of all concurrent requests and their results for each request in
        # history
        # index -> dict{index, ConcurrentValue}
        self.concurrent = {}

        # All blocks and their kv data based on responses
        # Each known block is mapped from block id to a Block
        self.blocks = {}

        # The value of all keys at last_consecutive_block
        self.kvpairs = {}
        self.last_consecutive_block = 0

        self.last_known_block = 0

    def send_write(self, client_id, seq_num, readset, writeset, read_block_id):
        """Track the send of a write request"""
        req = SkvbcWriteRequest(
                client_id, seq_num, readset, writeset, read_block_id)
        self._send_req(req, is_read=False)

    def send_read(self, client_id, seq_num, readset):
        """
        Track the send of a read request.

        Always get the latest value. We are trying to linearize requests, so we
        want a real-time ordering which requires getting the latest values.
        """
        req = SkvbcReadRequest(client_id, seq_num, readset)
        self._send_req(req, is_read=True)

    def _send_req(self, req, is_read):
        self.history.append(req)
        index = len(self.history) - 1
        self._update_concurrent_requests(index, is_read)
        cs = CausalState(index,
                         self.last_known_block,
                         self.last_consecutive_block,
                         self.kvpairs.copy())
        self.outstanding[(req.client_id, req.seq_num)] = cs

    def handle_write_reply(self, client_id, seq_num, reply):
        """
        Match a write reply with its outstanding request.
        Check for consistency violations and raise an exception if found.
        """
        rpy = SkvbcWriteReply(client_id, seq_num, reply)
        self.history.append(rpy)
        req, req_index = self._get_matching_request(rpy)
        if reply.success:
            if reply.last_block_id in self.blocks:
                # This block_id has already been written!
                block = self.blocks[reply.last_block_id]
                raise ConflictingBlockWriteError(reply.last_block_id, block, req)
            else:
                self._record_concurrent_write_success(req_index,
                                                      rpy,
                                                      reply.last_block_id)
                self.blocks[reply.last_block_id] = Block(req.writeset, req_index)

                if reply.last_block_id > self.last_known_block:
                    self.last_known_block = reply.last_block_id

                # Update consecutive kvpairs
                if reply.last_block_id == self.last_consecutive_block + 1:
                    self.last_consecutive_block += 1
                    for k,v in req.writeset.items():
                        self.kvpairs[k] = v
        else:
            self._record_concurrent_write_failure(req_index, rpy)

    def handle_read_reply(self, client_id, seq_num, kvpairs):
        """
        Get a read reply and ensure that it linearizes with the current known
        concurrent replies.
        """
        rpy = SkvbcReadReply(client_id, seq_num, kvpairs)
        req, req_index = self._get_matching_request(rpy)
        self.history.append(rpy)
        self._record_read_reply(req_index, rpy)

    def get_missing_blocks(self, last_block_id):
        """
        Retrieve the set of missing blocks.

        This is called during synchronization before continuing with a test,
        so that the tester can retrieve these blocks and call
        self.fill_in_missing_blocks().

        After missing blocks are filled in, successful reads can be linearized.
        """
        missing_blocks = set([i for i in range(self.last_consecutive_block + 1,
                                               self.last_known_block)])
        # Include last_block_id if not already known
        for i in range(self.last_known_block + 1, last_block_id + 1):
            missing_blocks.add(i)
        return missing_blocks

    def fill_missing_blocks(self, missing_blocks):
        """
        Add all missing blocks to self.blocks

        Note that these blocks will not have a matching req_index since we never
        received a reply for the request that created it. In some histories it's
        not possible to identify an unambiguous request, since there may be
        multiple possible requests that could have correctly generated the
        block. Rather than trying to match the requests, to the missing blocks,
        we just assume the missing blocks are correct for now, and use the full
        block history to verify successful conditional writes and reads.
        """
        for block_id, kvpairs in missing_blocks.items():
            self.blocks[block_id] = Block(kvpairs)
            if block_id > self.last_known_block:
                self.last_known_block = block_id

    def verify(self):
        self._verify_successful_writes()
        self._linearize_reads()

    def _verify_successful_writes(self):
        for i in range(1, self.last_known_block+1):
            req_index = self.blocks[i].req_index
            if req_index != None:
                # A reply was received for this request that created the block
                req = self.history[req_index]
                self._verify_successful_write(i, req)

    def _linearize_reads(self):
        """
        At this point, we should know the kv pairs of all blocks.

        Attempt to find linearization points for all reads in
        self.completed_reads.

        If a read cannot be linearized, then raise an exception.
        """
        for req_index, completed_read  in self.completed_reads.items():
            cs = completed_read.causal_state
            kv = cs.kvpairs
            num_intermediate_blocks = (cs.last_known_block
                                      - cs.last_consecutive_block)
            # We must check that the read linearizes after
            # causal_state.last_known_block, since it must have started after
            # that. Build up the kv state until last_known_block.
            for block_id in range(cs.last_consecutive_block + 1,
                                  cs.last_known_block + 1):
                kv.update(self.blocks[block_id].kvpairs)
            num_concurrent = self._max_possible_concurrent_writes(req_index)

            # Any missing intermediate block is by definition concurrent, so we
            # need to subtract it as well.
            blocks_remaining = (self.last_known_block
                                - cs.last_known_block
                                - num_intermediate_blocks)
            blocks_to_check = min(num_concurrent, blocks_remaining)

            success = False
            for i in range(cs.last_known_block,
                           cs.last_known_block + blocks_to_check + 1):
                if i != cs.last_known_block:
                    kv.update(self.blocks[i].kvpairs)
                if self._read_is_valid(kv, completed_read.kvpairs):
                    # The read linearizes here
                    success = True
                    break

            if not success:
                raise InvalidReadError(completed_read,
                                       self.concurrent[req_index])

    def _read_is_valid(self, kv_state, read_kvpairs):
        """Return if a read of read_kvpairs is possible given kv_state."""
        for k, v in read_kvpairs.items():
            if kv_state.get(k) != v:
                return False
        return True

    def _max_possible_concurrent_writes(self, req_index):
        """
        Return the maximum possible number of concurrrent writes.
        This includes writes that returned successfully but also writes with no
        return that could have generated missing blocks.
        """
        count = 0
        for val in self.concurrent[req_index].values():
            if val.result == Result.WRITE_SUCCESS or \
               val.result == Result.UNKNOWN_WRITE:
                   count += 1
        return count

    def _update_concurrent_requests(self, index, is_read):
        """
        Set the concurrent requests for this request to a dictionary of all
        indexes into history in self.outstanding mapped to a ConcurrentValue.

        Also update all concurrent requests to include this request in their
        concurrent dicts.
        """
        self.concurrent[index] = {}
        for causal_state in self.outstanding.values():
            i = causal_state.req_index
            is_read_outstanding = isinstance(self.history[i], SkvbcReadRequest)
            # Add the outstanding request to this request's concurrent dicts
            self.concurrent[index][i] = ConcurrentValue(is_read_outstanding)
            # Add this request to the concurrent dicts of each outstanding req
            self.concurrent[i][index] = ConcurrentValue(is_read)

    def _verify_successful_write(self, written_block_id, req):
        """
        Ensure that the block at written_block_id should have been written by
        req.

        Check that for each key in the readset, there have been no writes to
        those keys for each block after the block version in the conditional
        write up to, but not including this block. An example of failure is:

          * We read block id = X
          * We write block id = X + 2
          * We notice that block id X + 1 has written a key in the readset of
            this request that created block X + 2.

        Note that we may have unknown blocks due to missing responses.  We just
        skip these blocks, as we can't tell if there's a conflict or not. We
        have to assume there isn't a conflict in this case.

        If there is a conflicting block then there is a bug in the consensus
        algorithm, and we raise a StaleReadError.
        """
        for i in range(req.read_block_id + 1, written_block_id):
            if i not in self.blocks:
                # Ensure we have learned about this block.
                # Move on if we have not.
                continue
            block = self.blocks[i]

            # If the writeset of the request that created intermediate blocks
            # intersects the readset of this request, then we have a conflict.
            if len(req.readset.intersection(set(block.kvpairs.keys()))) != 0:
                raise StaleReadError(req.read_block_id, i, written_block_id)

    def _verify_failed(self, failed_req, failed_req_index):
        """
        Ensure that this request shouldn't have succeeded.

        It should have succeeded if any blocks written as a result of successful
        concurrent requests after failed_req.read_block_id don't contain
        writesets interesecting with the readset in failed_req. If *all* blocks
        that result from *all* successful concurrent writes don't conflict, and
        there are no UNKNOWN_WRITE results, then there is a bug in consensus.

        *** --- CHECKER CONSTRAINTS --- ***

        1. We are only considering explicit failures returned from the
           SimpleKVBC TesterReplica here, and not timeouts caused by lack of
           response.

        2. This strategy only works if we assume in the absence of
           concurrent writes with contention that all writes will succeed. In
           other words, the readset should always be the latest block for the
           given keys.  This is easy to guarantee in tests, since we can just
           always set the block_id of the request to the latest block_id before
           sending concurrent requests. If we didn't want this restriction, then
           the verification procedure would become more expensive. We'd have to
           check all blocks from the aribtrarily early block id for the readset
           up until the latest successfull block write of the concurrent
           request. This could be a lot of blocks in long histories if we
           randomly pick a block to read from. By constraining ourselves to only
           failing due to concurrent writes we can limit our search and as far
           as I can tell don't lose anything with regards to testing for
           consistency anomalies.
        """
        for i, val in self.concurrent[failed_req_index].items():
            # If there are any unknown results, it's possible that there was a
            # conflict. Therefore we must assume that failed_req should have
            # failed.
            if val.result == Result.UNKNOWN_WRITE:
                return

            if val.result == Result.WRITE_SUCCESS:
               if val.written_block_id > failed_req.read_block_id:
                   req = self.history[i]
                   if len(failed_req.readset.intersection(req.writeset_keys())) != 0:
                       # We found a concurrent request that conflicts. We must
                       # assume that failed_req was failed correctly.
                       return

        # We didn't find any unknown results or conflicting concurrent requests.
        # failed_req should have succeeded!
        raise NoConflictError(failed_req, self.concurrent[failed_req_index])

    def _record_concurrent_write_success(self, req_index, rpy, block_id):
        """Inform all concurrent requests that this request succeeded."""
        # We don't need the causal state for verification on write successes
        del self.outstanding[(rpy.client_id, rpy.seq_num)]

        val = ConcurrentValue(is_read=False)
        val.result = Result.WRITE_SUCCESS
        val.written_block_id = block_id
        for i in self.concurrent[req_index].keys():
            self.concurrent[i][req_index] = val

    def _record_concurrent_write_failure(self, req_index, rpy):
        """Inform all concurrent requests that this request failed."""
        causal_state = self.outstanding.pop((rpy.client_id, rpy.seq_num))
        self.failed_writes[req_index] = causal_state

        val = ConcurrentValue(is_read=False)
        val.result = Result.WRITE_FAIL
        for i in self.concurrent[req_index].keys():
            self.concurrent[i][req_index] = val

    def _record_read_reply(self, req_index, rpy):
        """Inform all concurrent requests about a read reply"""
        causal_state = self.outstanding.pop((rpy.client_id, rpy.seq_num))
        self.completed_reads[req_index] = CompletedRead(causal_state,
                                                        rpy.kvpairs)

        for i in self.concurrent[req_index].keys():
            self.concurrent[i][req_index].result = Result.READ_REPLY

    def _get_matching_request(self, rpy):
        """
        Return the request that matches rpy along with its index into
        self.history.
        """
        causal_state = self.outstanding[(rpy.client_id, rpy.seq_num)]
        index = causal_state.req_index
        return (self.history[index], index)

