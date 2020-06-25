#!/usr/bin/env python

import random
import sys

sys.path.append("..")
import cppgen
import cmfc
from exceptions import CmfParseError


def randint():
    return random.randint(0, 100)


def randstring():
    return '"' + random.choice(["a", "b", "c", "aa" "bb" "cc" "abcdef"]) + '"'


def byteExample():
    return "{0,1,2,3,4,5}"


class InstanceGen:
    def __init__(self, size, msgmap):
        self.size = size
        self.msgmap = msgmap

    def instancestr(self, type):
        """
        Return an instance of a given type. Do this recursively for nested types like lists and maps.
        """
        if not isinstance(type, dict):
            # A primitive type
            if cppgen.is_int(type):
                return str(randint())
            if type == "string":
                return randstring()
            if type == "bytes":
                return byteExample()
            if type == "bool":
                return random.choice(["true", "false"])
            # The type must be the name of a message
            return self.inlineStructInstanceStr(self.msgmap[type])
        elif self.size == 0:
            return "{}"

        # TODO: Support multiple values in a type according to size
        elif "list" in type:
            return "{" + self.instancestr(type.list.type) + "}"
        elif "kvpair" in type:
            return (
                "{"
                + self.instancestr(type.kvpair.key)
                + ", "
                + self.instancestr(type.kvpair.value)
                + "}"
            )
        elif "map" in type:
            # Maps require an initializer list of kv pairs, which themselves are represented as initializer lists. That's the reason for the double bracket.
            return (
                "{{"
                + self.instancestr(type.map.key)
                + ", "
                + self.instancestr(type.map.value)
                + "}}"
            )
        elif "optional" in type:
            return "{" + self.instancestr(type.optional.type) + "}"
        elif "oneof" in type:
            return self.inlineStructInstanceStr(
                self.msgmap[random.choice(type.oneof.msg_names)]
            )
        raise CmfParseError(type.parseinfo, "Invalid field type")

    def inlineStructInstanceStr(self, msg):
        """
        Take an AST for a message and serialize an instance of the struct.

        This is for use during initialization inside other types.
        """
        s = "{} {{".format(msg.name)
        for field in msg.fields:
            s += self.instancestr(field.type) + ","
        s += "}"
        return s

    def structInstanceVariableStr(self, msg):
        """
        Take an AST for a message and serialize an instance of the struct with a named variable.
        """
        s = "{} __{}__{}{{".format(msg.name, msg.name, self.size)
        for field in msg.fields:
            s += self.instancestr(field.type) + ","
        s += "};"
        return s


def file_header(namespace):
    header = """/***************************************
 Autogenerated by test_cppgen.py. Do not modify.
***************************************/

#include "example.h"

"""

    if namespace:
        header += "namespace {} {{\n\n".format(namespace)
    return header


def file_trailer(namespace):
    if namespace:
        return "\n}} // namespace {}\n".format(namespace)
    return "\n"


def generate_code_and_tests(ast):
    """ Walk concord message format(CMF) AST and generate C++ code and C++ tests"""
    namespace = "cmf::test"
    max_size = 5
    code, msgmap = cppgen.translate(ast, namespace)
    test_code = file_header(namespace)
    for msg in ast.msgs:
        # We generate `max_size` msg instances for tests
        gens = [InstanceGen(i, msgmap) for i in range(0, max_size)]
        for g in gens:
            test_code += g.structInstanceVariableStr(msg) + "\n\n"
    return code, test_code + file_trailer(namespace)

    # def test_serialization():
    #    """
    #    1. Generate C++ code for messages from example.cmf and write it to example.h.
    #    2. Generate instances of the messages as well as tests that round trip serialize and deserialize them.
    #    3. Compile that C++ code via cmake
    #    4. Run the compiled C++ code as a test
    #    """


from pprint import pprint

if __name__ == "__main__":
    try:
        with open("../grammar.ebnf") as f:
            grammar = f.read()
            with open("../../example.cmf") as f2:
                cmf = f2.read()
            ast, symbol_table = cmfc.parse(grammar, cmf)
            # Uncomment to show the generated AST for debugging purposes
            pprint(ast)
            code, tests = generate_code_and_tests(ast)
            with open("example.h", "w") as f3:
                f3.write(code)
            with open("test_serialization.cpp", "w") as f4:
                f4.write(tests)
    except Exception as ex:
        print(ex)
        exit(-1)
