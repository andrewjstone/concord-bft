
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
import os.path
import random
import unittest

import trio

from util import skvbc as kvbc
from util.bft import with_trio, with_bft_network, KEY_FILE_PREFIX

def start_replica_cmd(builddir, replica_id):
    """
    Return a command that starts an skvbc replica when passed to
    subprocess.Popen.

    Note each arguments is an element in a list.
    """
    statusTimerMilli = "500"
    path = os.path.join(builddir, "tests", "simpleKVBC", "TesterReplica", "skvbc_replica")
    return [path,
            "-k", KEY_FILE_PREFIX,
            "-i", str(replica_id),
            "-s", statusTimerMilli,
            "-p"]

class SkvbcTest(unittest.TestCase):
    @with_trio
    @with_bft_network(start_replica_cmd)
    async def test_vblock_cache_eviction(self, bft_network):
        """
        Test that state transfer works for kMaxVBlocksInCache+1 checkpoints.

        Virtual blocks (vblocks) get evicted from the cache in an LRU manner
        when the cache reaches maximum size. We want to ensure that blocks
        actually get evicted and an assert doesn't fire because the cache
        grows without bound.

        Stop one node, perform state transfer kMaxVBlocksInCache+1 times to
        generate fill the vblock cache, restart the node the node and verify
        state transfer works as expected. Note that kMaxVblocksInCache is
        currently hardcoded at 28:
        https://github.com/vmware/concord-bft/blob/7e94864ce9719264a960197c9eb47388f33a4e03/bftengine/src/bcstatetransfer/BCStateTran.hpp#L91

        In a 4 node cluster with f=1 we should be able to stop a different
        node after state transfer completes and still operate correctly.

        Note: This is a regression test for https://github.com/vmware/concord-bft/pull/398
        """
        skvbc = kvbc.SimpleKVBCProtocol(bft_network)

        stale_node = random.choice(
            bft_network.all_replicas(without={0}))

        max_vblocks_in_cache = 28

        # Do an initial checkpoint that starts the replicas
        await skvbc.prime_for_state_transfer(
            stale_nodes={stale_node},
            checkpoints_num=1
        )
        print("Starting stale replica" + str(stale_node))
        bft_network.start_replica(stale_node)
        await bft_network.wait_for_state_transfer_to_start()
        await bft_network.wait_for_state_transfer_to_stop(0, stale_node)
        await skvbc.assert_successful_put_get(self)

        print("Starting state transfer loop")
        # Perform another max_vblocks_in_cache checkpoints and state transfers
        initial_nodes = bft_network.all_replicas(without={stale_node})
        for i in range(2, max_vblocks_in_cache + 2):
            bft_network.stop_replica(stale_node)
            print("Fill and wait: " + str(i))
            await skvbc.fill_and_wait_for_checkpoint(initial_nodes=initial_nodes, start=(i-1)*150,
                checkpoint_num=i)
            bft_network.start_replica(stale_node)
            await bft_network.wait_for_state_transfer_to_start()
            await bft_network.wait_for_state_transfer_to_stop(0, stale_node)
            await skvbc.assert_successful_put_get(self)

        random_replica = random.choice(
            bft_network.all_replicas(without={0, stale_node}))
        bft_network.stop_replica(random_replica)
        await skvbc.assert_successful_put_get(self)