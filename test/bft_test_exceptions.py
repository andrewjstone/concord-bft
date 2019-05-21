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

class Error(Exception):
    """Base class for exceptions in this module."""
    pass


##
## Exceptions for bft_tester
##
class AlreadyRunningError(Error):
    def __init__(self, replica):
        self.replica = replica

class AlreadyStoppedError(Error):
    def __init__(self, replica):
        self.replica = replica

class BadReplyError(Error):
    def __init__(self):
        pass

##
## Exceptions for skvbc_linearizability
##
class ConflictingBlockWrite(Error):
    """The same block was already written by a different conditional write"""
    def __init__(self, block_id, original_request, new_request):
        self.block_id = block_id
        self.original_request = original_request
        self.new_request = new_request

class StaleReadInSuccessfulWrite(Error):
    """
    A conditional write did not see that a key in its readset was written after
    the block it was attempting to read from, but before the block the write
    created. As an example, The readset block version was X, an update was made
    to a key in the readset in block X+1, and this write successfully wrote
    block X+2.

    In our example the parameters to the constructor would be set as:

        readset_block_id = X
        block_with_conflicting_writeset = X + 1
        block_being_checked = X + 2

    """
    def __init__(self,
                 readset_block_id,
                 block_with_conflicting_writeset,
                 block_being_checked):
        self.readset_block_id = readset_block_id
        self.block_with_conflicting_writeset = block_with_conflicting_writeset
        self.block_being_checked = block_being_checked
