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

class SkvbcWriteRequest:
    """
    A write request sent to an Skvbc cluster. A request may or may not complete.
    """
    def __init__(self, client_id, seq_num, readset, writeset, block_id=0):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.client_seq_num = seq_num
        self.readset = readset
        self.writeset = writeset
        self.block_id = block_id

class SkvbcReadRequest:
    """
    A read request sent to an Skvbc cluster. A request may or may not complete.
    """
    def __init__(self, client_id, seq_num, readset, block_id=0):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.client_seq_num = seq_num
        self.readset = readset
        self.block_id = block_id

class SkvbcGetLastBlockReq:
    """
    A GET_LAST_BLOCK request sent to an skvbc cluster. A request may or may not
    complete.
    """
    def __init__(self, client_id, seq_num):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.client_seq_num = seq_num


class SkvbcWriteReply:
    """A reply to an outstanding write request sent to an Skvbc cluster."""
    def __init__(self, client_id, seq_num, reply):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.client_seq_num = seq_num
        self.reply = reply

class SkvbcReadReply:
    """A reply to an outstanding read request sent to an Skvbc cluster."""
    def __init__(self, client_id, seq_num, reply):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.client_seq_num = seq_num
        self.reply = reply

class SkvbcGetLastBlockReply:
    """
    A reply to an outstanding get last block request sent to an Skvbc cluster.
    """
    def __init__(self, client_id, seq_num, reply):
        self.timestamp = time.monotonic()
        self.client_id = client_id
        self.client_seq_num = seq_num
        self.reply = reply

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
        self.outstanding = {}

        # A set of all concurrent requests for each request in history
        # index -> set(index)
        self.concurrent = {}

        # All blocks and their kv data based on responses
        # Each known block is mapped from block id to request index in
        # self.history
        self.blocks = {}

        self.last_known_block = -1

    def send_write(self, client_id, seq_num, request):
        req = SkvbcWriteRequest(client_id, seq_num, request)
        self.history.append(req)
        index = len(self.history) - 1
        self.update_concurrent_requests(index)
        self.outstanding[(client_id, seq_num)] = index

    def update_concurrent_requests(self, index):
        # Set the concurrent requests for this request to all indexes in
        # self.outstanding.
        self.concurrent[index] = set(self.outstanding.values())
        # Add this index to the concurrent sets of each outstanding request
        for i in self.outstanding.values():
            self.concurrent[i].add(index)

    def handle_write_reply(self, client_id, seq_num, reply):
        rpy = SkvbcWriteReply(client_id, seq_num, reply),
        self.history.apend(rpy)
        req = self.get_matching_request(rpy)
        if reply.success:
            if reply.last_block_id in self.blocks:
                # This block_id has already been written!
                orig_req = self.blocks[reply.last_block_id]
                raise ConflictingBlockWrite(reply.last_block_id, orig_req, req)
            else:
                self.blocks[reply.last_block_id] = req
                self.verify_successful_write(reply.last_block_id, req)

                if reply.last_block_id > self.last_known_block:
                    self.last_known_block = reply.last_block_id
                else:
                    self.verify_blocks_after(reply.last_block_id, req)

                # TODO: Check that failed concurrent replies shouldn't have
                # succeeded
                # This is the same check done below for failing requests, but
                # for each failed concurrent request.
        else:
            # This request has failed.
            # TODO: Ensure that this request shouldn't have succeeded
            # It should have succeeded if any blocks written as a result of
            # concurrent requests after the block version in the conditional
            # write don't contain keys conflicting with the readset in this
            # request. If *all* blocks that result from *all* concurrent writes
            # don't conflict, then there is a bug in consensus.

    def get_missing_blocks(self):
        """
        Retrieve all unknown blocks from the replicas. Unknown blocks are blocks
        less than self.last_known_block that don't exist in self.blocks.

        This method should only be called when there are no in flight requests.

        When there are network partitions or nodes crash, some requests will not
        get responses. This method allows us to fill in the data for unknown
        blocks, so when we continue sending concurrent requests we can be sure
        that we have the whole commit history up to this point.

        First we retrieve all missing blocks and put them in self.blocks, but we
        keep them as provisional during the verification step below.

        When we retrieve a missing block, we want to make sure that it makes
        sense given what we already know from our history and from known blocks.
        Specifically we want to do the following, for each missing block:

          1. Find *all* outstanding conditional write requests that never received a
          response and that has a writeset that matches the keys and values in the
          retrieved block.
          2. Ensure that the readset for those requests doesn't have any conflicts
          with blocks later than the conditional write block version, but less
          than this block.
          3. Keep a list of all possible conditional write requests that satisfy
          this block.
          4. If there is exactly one conditional write requests that satisfies
          the block, then create a corresponding fake reply, and insert it at
          the end of the history. Remove this request from outstanding requests
          so it's not used in any other block verifications. Also check if this
          request is in any possibilities for other blocks (see step 6), and
          remove it as it can only satisfy one block. If this leaves only one
          possibility for that block then repeat step 4 for that block.
          5. If there are no outstanding conditional write that satisfies step 1
          and 2, then there is a bug in the consensus algorithm.
          6. If there are more than one possible conditional writes, then mark
          all off them as possibilities, and go onto the next block.

        If all missing blocks are either satisfied or have multiple
        possibilities, try to find a possibility for each block such that all
        blocks are satisfied. If this can be achieved, then we assume that is
        what actually occurred. There can be multiple successful linearizations,
        so we just pick one and pretend that's what occurred. We then go ahead
        and take all remaining outstanding requests and create corresponding
        failure replies and append them to the history. We now have a complete
        linearizable history and we can move onto the next batch of requests. We
        *may* also want to check the complete block history at this point to
        ensure that all our blocks match what is recorded in the blockchain.
        Note that any mismatch is a bug in consensus, as the retrieved blocks in
        the blockchain would contradict the replies from conditional writes.

        """
        pass

    def verify_successful_write(self, written_block_id, req):
        """
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
        algorithm, and we raise a StaleReadInSuccessfulWrite error.
        """
        for i in range(req.block_id + 1, written_block_id):
            if i not in self.blocks:
                # Ensure we have learned about this block.
                # Move on if we have not.
                continue
            intermediate_req = self.blocks[i]

            # If the writeset of the request that created intermediate blocks
            # intersects the readset of this request, then we have a conflict.
            if len(req.readset.intersection(intermediate_req.writeset)) != 0:
                raise StaleReadInSuccessfulWrite(req.block_id,
                                                 i,
                                                 written_block_id)

    def verify_blocks_after(self, written_block_id, req):
        """
        There were concurrent requests that have already responded with written
        blocks later than this one. They would have seen unkown blocks when
        checking their readset in verify_successful_write.

        For every block up until self.last_known_block, check that the written
        values in this block don't conflict with the readsets in the later
        blocks. Conflict means that the block version in the conditional write
        for requests that created blocks after this one had a version less than
        the block_id here.

        If there is a conflicting block then there is a bug in the consensus
        algorithm.
        """
        for i in range(written_block_id, self.last_known_block+1):
            if i not in self.blocks:
                # Ensure we have learned about this block.
                # Move on if we have not.
                continue
            later_block = self.blocks[i]

            # Is there a possible conflict between this block and the later
            # block? A possible conflict exists if the readset block_id in the
            # later block is less than the block_id for this block.
            if later_block.block_id < written_block_id:
                # If the writeset of this request intersects the readset of the
                # later block, then we have a conflict.
                if len(later_block.readset.intersection(req.writeset)) != 0:
                    raise StaleReadInSuccessfulWrite(later_block.block_id,
                                                     written_block_id,
                                                     i)


    def get_matching_request(self, rpy):
        """Return the request that matches rpy"""
        index = self.outstanding[(rpy.client_id, rpy.seq_num)]
        return self.history[index]

