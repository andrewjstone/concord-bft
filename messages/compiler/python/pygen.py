# Concord
#
# Copyright (c) 2020 VMware, Inc. All Rights Reserved.
#
# This product is licensed to you under the Apache 2.0 license (the "License").
# You may not use this product except in compliance with the Apache 2.0 License.
#
# This product may include a number of subcomponents with separate copyright
# notices and license terms. Your use of these subcomponents is subject to the
# terms and conditions of the subcomponent's license, as noted in the LICENSE
# file.

import os
from exceptions import CmfParseError
from python.py_visitor import PyVisitor
from walker import Walker


def translate(ast, namespace=None):
    """
    Walk concord message format(CMF) AST and generate Python code.

    Return Python code as a string.
    """
    with open(os.path.join(os.path.dirname(__file__), "serialize.py")) as f:
        base_serializers = f.read() + '\n'
    visitor = PyVisitor()
    walker = Walker(ast, visitor)
    walker.walk()
    return base_serializers + visitor.output
