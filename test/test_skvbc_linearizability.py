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

import unittest
import skvbc
import skvbc_linearizability

from bft_test_exceptions import(
    ConflictingBlockWriteError,
    StaleReadError,
    NoConflictError,
    InvalidReadError
)

class TestCompleteHistories(unittest.TestCase):
    """Test histories where no blocks are unknown"""

    def setUp(self):
        self.tracker = skvbc_linearizability.SkvbcTracker()

    def test_sucessful_write(self):
        """
        A single request results in a single successful reply.
        The checker finds no errors.
        """
        client_id = 0
        seq_num = 0
        readset = set()
        read_block_id = 0
        writeset = {'a': 'a'}
        self.tracker.send_write(
                client_id, seq_num, readset, writeset, read_block_id)
        reply = skvbc.WriteReply(success=True, last_block_id=1)
        self.tracker.handle_write_reply(client_id, seq_num, reply)
        self.tracker.verify()

    def test_failed_write(self):
        """
        A single request results in a single failed reply.
        The checker finds a NoConflictError, because there were no concurrent
        requests that would have caused the request to fail explicitly.
        """
        client_id = 0
        seq_num = 0
        readset = set()
        read_block_id = 0
        writeset = {'a': 'a'}
        self.tracker.send_write(
                client_id, seq_num, readset, writeset, read_block_id)
        reply = skvbc.WriteReply(success=False, last_block_id=1)
        self.tracker.handle_write_reply(client_id, seq_num, reply)

        with self.assertRaises(NoConflictError) as err:
            self.tracker.verify()

        self.assertEqual(0, err.exception.causal_state.req_index)

    def test_contentious_writes_one_success_one_fail(self):
        """
        Two writes contend for the same key. Only one should succeed.
        The checker should not see an error.
        """
        # First create an initial block so we can contend with it
        self.test_sucessful_write()
        # Send 2 concurrent writes with the same readset and writeset
        readset = set("a")
        writeset = {'a': 'a'}
        self.tracker.send_write(0, 1, readset, writeset, 1)
        self.tracker.send_write(1, 1, readset, writeset, 1)
        self.tracker.handle_write_reply(0, 1, skvbc.WriteReply(False, 0))
        self.tracker.handle_write_reply(1, 1, skvbc.WriteReply(True, 2))
        self.tracker.verify()

    def test_contentious_writes_both_succeed(self):
        """
        Two writes contend for the same key. Both succeed.
        The checker should raise a StaleReadError, since only one should have
        succeeded.
        """
        # First create an initial block so we can contend with it
        self.test_sucessful_write()
        # Send 2 concurrent writes with the same readset and writeset
        readset = set("a")
        writeset = {'a': 'a'}
        self.tracker.send_write(0, 1, readset, writeset, 1)
        self.tracker.send_write(1, 1, readset, writeset, 1)
        self.tracker.handle_write_reply(1, 1, skvbc.WriteReply(True, 2))
        self.tracker.handle_write_reply(0, 1, skvbc.WriteReply(True, 3))

        with self.assertRaises(StaleReadError) as err:
            self.tracker.verify()

        # Check the exception detected the error correctly
        self.assertEqual(err.exception.readset_block_id, 1)
        self.assertEqual(err.exception.block_with_conflicting_writeset, 2)
        self.assertEqual(err.exception.block_being_checked, 3)

    def test_contentious_writes_both_succeed_same_block(self):
        """
        Two writes contend for the same key. Both succeed, apparently
        creating the same block.

        The checker should raise a ConflictingBlockWriteError, since two
        requests cannot create the same block.
        """
        # First create an initial block so we can contend with it
        self.test_sucessful_write()
        # Send 2 concurrent writes with the same readset and writeset
        readset = set("a")
        writeset = {'a': 'a'}
        self.tracker.send_write(0, 1, readset, writeset, 1)
        self.tracker.send_write(1, 1, readset, writeset, 1)
        self.tracker.handle_write_reply(1, 1, skvbc.WriteReply(True, 2))

        with self.assertRaises(ConflictingBlockWriteError) as err:
            self.tracker.handle_write_reply(0, 1, skvbc.WriteReply(True, 2))

        # The conflicting block id was block 2
        self.assertEqual(err.exception.block_id, 2)

    def test_read_between_2_successful_writes(self):
        """
        Send 2 concurrent writes that don't conflict, but overwrite the same
        value, along with a concurrent read. The read sees the first overwrite,
        and should linearize after the first overwritten value correctly.
        """
        # A non-concurrent write
        self.test_sucessful_write()
        writeset_1 = {'a': 'b'}
        writeset_2 = {'a': 'c'}
        read_block_id = 1
        client0 = 0
        client1 = 1
        client2 = 2

        # 2 concurrent writes and a concurrent read
        self.tracker.send_write(client0, 1, set(), writeset_1, read_block_id)
        self.tracker.send_write(client1, 1, set(), writeset_2, read_block_id)
        self.tracker.send_read(client2, 1, ["a"])

        # block2/ writeset_2
        self.tracker.handle_write_reply(client1, 1, skvbc.WriteReply(True, 2))
        # block3/ writeset_1
        self.tracker.handle_write_reply(client0, 1, skvbc.WriteReply(True, 3))
        self.tracker.handle_read_reply(client2, 1, {"a":"b"})
        self.tracker.verify()

    def test_read_does_not_linearize_no_concurrent_writes(self):
        """
        Read a value that doesn't exist.
        This should raise an exception.
        """
        # state = {'a':'a'}
        self.test_sucessful_write()

        self.tracker.send_read(1, 0, ["a"])
        self.tracker.handle_read_reply(1, 0, {"a":"b"})
        with self.assertRaises(InvalidReadError) as err:
            self.tracker.verify()

        self.assertEqual(0, len(err.exception.concurrent_requests))

    def test_read_does_not_linearize_two_concurrent_writes(self):
        """
        Read a value that doesn't exist when concurrent with 2 reads.
        This should raise an exception.
        """
        self.test_sucessful_write()
        writeset_1 = {'a': 'b'}
        writeset_2 = {'a': 'c'}
        read_block_id = 1
        client0 = 0
        client1 = 1
        client2 = 2

        # 2 concurrent writes and a concurrent read
        self.tracker.send_write(client0, 1, set(), writeset_1, read_block_id)
        self.tracker.send_write(client1, 1, set(), writeset_2, read_block_id)
        self.tracker.send_read(client2, 1, ["a"])

        # block2/ writeset_2
        self.tracker.handle_write_reply(client1, 1, skvbc.WriteReply(True, 2))
        # block3/ writeset_1
        self.tracker.handle_write_reply(client0, 1, skvbc.WriteReply(True, 3))

        # Read a value that was never written
        self.tracker.handle_read_reply(client2, 1, {"a":"d"})

        with self.assertRaises(InvalidReadError) as err:
            self.tracker.verify()

        self.assertEqual(2, len(err.exception.concurrent_requests))

    def test_read_is_stale_with_concurrent_writes(self):
        """
        Read a value that doesn't exist when concurrent with 2 reads.
        This should raise an exception.
        """
        self.test_sucessful_write()
        writeset_1 = {'a': 'b'}
        writeset_2 = {'a': 'c'}
        read_block_id = 1
        client0 = 0
        client1 = 1
        client2 = 2

        # Another successful write commits.
        # All subsequent ops should see at least state {'a':'b'}
        self.tracker.send_write(client0, 1, set(), writeset_1, read_block_id)
        self.tracker.handle_write_reply(client0, 1, skvbc.WriteReply(True, 2))

        # A concurrent write and a concurrent read
        self.tracker.send_write(client1, 1, set(), writeset_2, read_block_id)
        self.tracker.send_read(client2, 1, ["a"])

        # block3/ writeset_2
        self.tracker.handle_write_reply(client1, 1, skvbc.WriteReply(True, 3))

        # Read a stale value. Key "a" was updated to "b" before the read was
        # sent. "a" can only be "b" or "c".
        self.tracker.handle_read_reply(client2, 1, {"a":"a"})

        with self.assertRaises(InvalidReadError) as err:
            self.tracker.verify()

        self.assertEqual(1, len(err.exception.concurrent_requests))


class TestPartialHistories(unittest.TestCase):
    """
    Test histories where some writes don't return replies.

    In these cases, blocks may be generated from the write, but the tracker
    doesn't know what the values are until after the test iteration, when the
    tester informs it of the missing blocks values.
    """
    def setUp(self):
        self.tracker = skvbc_linearizability.SkvbcTracker()

    def test_sucessful_write(self):
        """
        A single request results in a single successful reply.
        The checker finds no errors.
        """
        client_id = 0
        seq_num = 0
        readset = set()
        read_block_id = 0
        writeset = {'a': 'a'}
        self.tracker.send_write(
                client_id, seq_num, readset, writeset, read_block_id)
        reply = skvbc.WriteReply(success=True, last_block_id=1)
        self.tracker.handle_write_reply(client_id, seq_num, reply)
        self.tracker.verify()

    def test_2_concurrent_writes_one_missing_success_two_concurrent_reads(self):
        """
        Two concurrent writes are sent with two concurrent reads. One write does
        not respond.

        One read should linearize correctly based on the write that returned,
        and the other based on the one that didn't return.
        """
        # A non-concurrent write
        self.test_sucessful_write()
        writeset_1 = {'a': 'b'}
        writeset_2 = {'a': 'c'}
        read_block_id = 1
        client0 = 0
        client1 = 1
        client2 = 2
        client3 = 3

        # 2 concurrent unconditional writes and a concurrent read
        self.tracker.send_write(client0, 1, set(), writeset_1, read_block_id)
        self.tracker.send_write(client1, 1, set(), writeset_2, read_block_id)
        self.tracker.send_read(client2, 1, ["a"])
        self.tracker.send_read(client3, 1, ["a"])

        # writeset_2 written at block 3
        self.tracker.handle_write_reply(client1, 1, skvbc.WriteReply(True, 3))
        self.tracker.handle_read_reply(client2, 1, writeset_1)
        self.tracker.handle_read_reply(client3, 1, writeset_2)

        self.assertEqual(3, self.tracker.last_known_block)

        # The test should call these methods when it stops sending
        # operations.
        # The test must ask the tracker what blocks are missing
        missing_block_id = 2
        last_block = 3
        missing_blocks = self.tracker.get_missing_blocks(last_block)
        self.assertSetEqual(set([missing_block_id]), missing_blocks)

        self.assertEqual(3, self.tracker.last_known_block)

        # The test must retrieve the missing blocks from skvbc TesterReplicas
        # using the clients and them inform the tracker. This is the call that
        # informs the tracker.
        self.tracker.fill_missing_blocks({missing_block_id: writeset_1})

        # The tracker now has all the information it needs. Tell it to verify
        # that it's read responses linearize.
        self.tracker.verify()

    def test_2_concurrent_writes_one_missing_failure_two_concurrent_reads(self):
        """
        Two concurrent writes are sent with two concurrent reads. One write does
        not respond.

        One read should linearize correctly based on the write that returned,
        and the other based on the one that didn't return.
        """
        # A non-concurrent write
        self.test_sucessful_write()
        writeset_1 = {'a': 'b'}
        writeset_2 = {'a': 'c'}
        read_block_id = 1
        client0 = 0
        client1 = 1
        client2 = 2
        client3 = 3

        # 2 concurrent unconditional writes and a concurrent read
        self.tracker.send_write(client0, 1, set("a"), writeset_1, read_block_id)
        self.tracker.send_write(client1, 1, set("a"), writeset_2, read_block_id)
        self.tracker.send_read(client2, 1, ["a"])
        self.tracker.send_read(client3, 1, ["a"])

        # writeset_2 written at block 3
        self.tracker.handle_write_reply(client1, 1, skvbc.WriteReply(True, 2))
        self.tracker.handle_read_reply(client2, 1, writeset_1)
        self.tracker.handle_read_reply(client3, 1, writeset_2)

        self.assertEqual(2, self.tracker.last_known_block)

        # the last block is 2 because one of the writes conflicted
        last_block = 2
        missing_blocks = self.tracker.get_missing_blocks(last_block)
        self.assertSetEqual(set(), missing_blocks)

        with self.assertRaises(InvalidReadError) as err:
            self.tracker.verify()

        self.assertEqual(3, len(err.exception.concurrent_requests))
        # The read of writeset_1 fails, since that was the failed request that
        # never returned.
        self.assertEqual(writeset_1, err.exception.read.kvpairs)

if __name__ == '__main__':
    unittest.main()

