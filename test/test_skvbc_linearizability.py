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
    NoConflictError
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
        writeset = [("a", "a")]
        self.tracker.send_write(
                client_id, seq_num, readset, writeset, read_block_id)
        reply = skvbc.WriteReply(success=True, last_block_id=1)
        self.tracker.handle_write_reply(client_id, seq_num, reply)
        pass

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
        writeset = [("a", "a")]
        self.tracker.send_write(
                client_id, seq_num, readset, writeset, read_block_id)
        reply = skvbc.WriteReply(success=False, last_block_id=1)

        with self.assertRaises(NoConflictError) as err:
            self.tracker.handle_write_reply(client_id, seq_num, reply)
        self.assertEqual(err.exception.concurrent_requests, {})

    def test_contentious_writes_one_success_one_fail(self):
        """
        Two writes contend for the same key. Only one should succeed.
        The checker should not see an error.
        """
        # First create an initial block so we can contend with it
        self.test_sucessful_write()
        # Send 2 concurrent writes with the same readset and writeset
        readset = set("a")
        writeset = [("a", "a")]
        self.tracker.send_write(0, 1, readset, writeset, 1)
        self.tracker.send_write(1, 1, readset, writeset, 1)
        self.tracker.handle_write_reply(1, 1, skvbc.WriteReply(True, 2))
        self.tracker.handle_write_reply(0, 1, skvbc.WriteReply(False, 0))
        pass

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
        writeset = [("a", "a")]
        self.tracker.send_write(0, 1, readset, writeset, 1)
        self.tracker.send_write(1, 1, readset, writeset, 1)
        self.tracker.handle_write_reply(1, 1, skvbc.WriteReply(True, 2))

        with self.assertRaises(StaleReadError) as err:
            self.tracker.handle_write_reply(0, 1, skvbc.WriteReply(True, 3))

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
        writeset = [("a", "a")]
        self.tracker.send_write(0, 1, readset, writeset, 1)
        self.tracker.send_write(1, 1, readset, writeset, 1)
        self.tracker.handle_write_reply(1, 1, skvbc.WriteReply(True, 2))

        with self.assertRaises(ConflictingBlockWriteError) as err:
            self.tracker.handle_write_reply(0, 1, skvbc.WriteReply(True, 2))

        # The conflicting block id was block 2
        self.assertEqual(err.exception.block_id, 2)

if __name__ == '__main__':
    unittest.main()


