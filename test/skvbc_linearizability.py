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
    PhantomKeysError
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
        return set([k for (k, _) in self.writeset])

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

class SkvbcGetLastBlockReq:
    """
    A GET_LAST_BLOCK request sent to an skvbc cluster. A request may or may not
    complete.
    """
    def __init__(self, client_id, seq_num):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num


class SkvbcWriteReply:
    """A reply to an outstanding write request sent to an Skvbc cluster."""
    def __init__(self, client_id, seq_num, reply):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num
        self.reply = reply

class SkvbcReadReply:
    """A reply to an outstanding read request sent to an Skvbc cluster."""
    def __init__(self, client_id, seq_num, kvpairs):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num
        self.kvpairs = kvpairs

class SkvbcGetLastBlockReply:
    """
    A reply to an outstanding get last block request sent to an Skvbc cluster.
    """
    def __init__(self, client_id, seq_num, reply):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.seq_num = seq_num
        self.reply = reply

class Result(Enum):
   """
   Whether an operation succeeded, failed, or the result is unknown
   """
   WRITE_SUCCESS = 1
   WRITE_FAIL = 2
   UNKNOWN_WRITE = 3
   UNKNOWN_READ = 4
   READ_REPLY = 5

class CausalState:
    """Relevant state of the tracker before a request is started"""
    def __init__(self, last_known_block, last_consecutive_block, kvpairs):
        self.last_known_block = 0
        self.last_consecutive_block = last_consecutive_block

        # KV pairs contain the value up keys up until last_consecutive_block
        self.kvpairs = kvpairs

    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  last_known_block={self.last_known_block}\n'
           f'  last_consecutive_block={self.last_consecutive_block}\n'
           f'  kvpairs={self.kvpairs}\n')

class ConcurrentValue:
    """Track the state for a request / reply in self.concurrent"""
    def __init__(self, is_read, causal_state=None,):
        if is_read:
            self.result = Result.UNKNOWN_READ
        else:
            self.result = Result.UNKNOWN_WRITE

        # Only used in writes
        # Set only when writes succeed.
        self.written_block_id = 0

        # Only used in reads
        self.causal_state = causal_state
        self.reply = None


    def __repr__(self):
        return (f'{self.__class__.__name__}:\n'
           f'  result={self.result}\n'
           f'  written_block_id={self.written_block_id}\n'
           f'  causal_state={self.causal_state}\n')

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
        # (client_id, seq_num) -> index into self.history
        #
        # This gets cleared after the batch of concurrent requests is completed
        # with the call to self.linearize().
        self.outstanding = {}

        # A set of all concurrent requests and their results for each request in
        # history
        # index -> dict{index, ConcurrentValue}
        #
        # This gets cleared after the batch of concurrent requests is completed
        # with the call to self.linearize().
        self.concurrent = {}

        # All blocks and their kv data based on responses
        # Each known block is mapped from block id to a tuple containing the
        # request itself, and the request index into self.history.
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
        self.outstanding[(req.client_id, req.seq_num)] = index

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
                orig_req, _ = self.blocks[reply.last_block_id]
                raise ConflictingBlockWriteError(reply.last_block_id, orig_req, req)
            else:
                self._record_concurrent_write_success(req_index,
                                                      rpy,
                                                      reply.last_block_id)
                self.blocks[reply.last_block_id] = (req, req_index)
                self._verify_successful_write(reply.last_block_id, req)

                if reply.last_block_id > self.last_known_block:
                    self.last_known_block = reply.last_block_id
                else:
                    self._verify_blocks_after(reply.last_block_id, req)

                # Check that failed concurrent replies shouldn't have succeeded.
                # This is the same check done below for failing requests, but
                # for each failed concurrent request.
                self._verify_concurrent_requests_failed_correctly(req_index)

                # Update consecutive kvpairs
                if reply.last_block_id == self.last_consecutive_block + 1:
                    self.last_consecutive_block += 1
                    for k,v in req.writeset:
                        self.kvpairs[k] = v
        else:
            self._record_concurrent_write_failure(req_index, rpy)
            self._verify_failed(req, req_index)

    def handle_read_reply(self, client_id, seq_num, kvpairs):
        """
        Get a read reply and ensure that it linearizes with the current known
        concurrent replies.
        """
        rpy = SkvbcReadReply(client_id, seq_num, kvpairs)
        req, req_index = self._get_matching_request(rpy)
        self.history.append(rpy)
        self._record_read_reply(req_index, rpy)

    def linearize(self):
        """
        Take outstanding requests, and reads and see if their is a total
        order they can be placed in that matches the state of the system.

        This method should be called when no more requests have been issued and
        all outstanding requests have resulted in timeouts at clients.
        """
        pass

    def _update_concurrent_requests(self, index, is_read):
        # Set the concurrent requests for this request to a dictionary of all
        # indexes into history in self.outstanding mapped to
        # Result.(UNKNOWN_READ | UNKNOWN_WRITE) since a response hasn't been
        # learned yet.
        val = ConcurrentValue(is_read)
        if is_read:
            cs = CausalState(self.last_known_block,
                             self.last_consecutive_block,
                             self.kvpairs.copy())
            val = ConcurrentValue(is_read, cs)

        concurrent_indexes = self.outstanding.values()
        self.concurrent[index] = dict(
            zip(concurrent_indexes,
                [val for _ in range(0, len(concurrent_indexes))]))

        # Add this index to the concurrent sets of each outstanding request
        for i in self.outstanding.values():
            self.concurrent[i][index] = val

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
            intermediate_req, _ = self.blocks[i]

            # If the writeset of the request that created intermediate blocks
            # intersects the readset of this request, then we have a conflict.
            if len(req.readset.intersection(intermediate_req.writeset_keys())) != 0:
                raise StaleReadError(req.read_block_id, i, written_block_id)

    def _verify_blocks_after(self, written_block_id, req):
        """
        Ensure that blocks written after this block should have been written.

        There were concurrent requests that have already responded with written
        blocks later than this one. They would have seen unknown blocks when
        checking their readset in _verify_successful_write.

        For every block after written_block_id, up until self.last_known_block,
        check that the written values in this block don't conflict with the
        readsets in the later blocks. Conflict means that the block version in
        the conditional write for requests that created blocks after this one
        had a version less than written_block_id and a readset that intersected
        with the writeset of this block.

        If there is a conflicting block then there is a bug in the consensus
        algorithm.
        """
        for i in range(written_block_id, self.last_known_block+1):
            if i not in self.blocks:
                # Ensure we have learned about this block.
                # Move on if we have not.
                continue
            later_block, _ = self.blocks[i]

            # Is there a possible conflict between this block and the later
            # block? A possible conflict exists if the readset block_id in the
            # later block is less than the block_id for this block.
            if later_block.read_block_id < written_block_id:
                # If the writeset of this request intersects the readset of the
                # later block, then we have a conflict.
                if len(later_block.readset.intersection(req.writeset_keys())) != 0:
                    raise StaleReadError(later_block.read_block_id,
                                         written_block_id,
                                         i)

    def _verify_concurrent_requests_failed_correctly(self, req_index):
        """
        Ensure that all requests concurrent with the request at req_index that
        failed should not have succeeded.

        This method is needed because some failed requests can reply before
        concurrent requests that are successful. We can only detect that failed
        requests should have succeeded if we have received responses for all
        concurrent requests. Therefore everytime a request succeeds we have to
        check all its failed concurrent_requests.

        TODO: We may be able to optimize how frequently this runs by keeping
        track of when a request has received replies for all concurrent requests
        and only running when it has.
        """
        for i, val in self.concurrent[req_index].items():
            if val.result == Result.WRITE_FAIL:
                failed_req = self.history[i]
                self._verify_failed(failed_req, i)

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
        del self.outstanding[(rpy.client_id, rpy.seq_num)]

        val = ConcurrentValue(is_read=False)
        val.result = Result.WRITE_SUCCESS
        val.written_block_id = block_id
        for i in self.concurrent[req_index].keys():
            self.concurrent[i][req_index] = val

    def _record_concurrent_write_failure(self, req_index, rpy):
        """Inform all concurrent requests that this request failed."""
        del self.outstanding[(rpy.client_id, rpy.seq_num)]

        val = ConcurrentValue(is_read=False)
        val.result = Result.WRITE_FAIL
        for i in self.concurrent[req_index].keys():
            self.concurrent[i][req_index] = val

    def _record_read_reply(self, req_index, rpy):
        """Inform all concurrent requests about a read reply"""
        del self.outstanding[(rpy.client_id, rpy.seq_num)]

        for i in self.concurrent[req_index].keys():
            self.concurrent[i][req_index].result = Result.READ_REPLY
            self.concurrent[i][req_index].reply = rpy

    def _get_matching_request(self, rpy):
        """
        Return the request that matches rpy along with its index into
        self.history.
        """
        index = self.outstanding[(rpy.client_id, rpy.seq_num)]
        return (self.history[index], index)

