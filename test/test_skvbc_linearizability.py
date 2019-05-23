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
    StaleReadInSuccessfulWriteError,
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
        readset = []
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
        readset = []
        read_block_id = 0
        writeset = [("a", "a")]
        self.tracker.send_write(
                client_id, seq_num, readset, writeset, read_block_id)
        reply = skvbc.WriteReply(success=False, last_block_id=1)

        with self.assertRaises(NoConflictError) as err:
            self.tracker.handle_write_reply(client_id, seq_num, reply)
        self.assertEqual(err.exception.concurrent_requests, {})

if __name__ == '__main__':
    unittest.main()


