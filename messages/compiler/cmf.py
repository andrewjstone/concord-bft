#!/usr/bin/python

import argparse


class CmfException(Exception):
    def __init__(self, msg):
        self.message = msg

# Data Types


class Bool:
    def __init__(self, val):
        assert val is bool
        self.val = val


class Uint8:
    def __init__(self, val):
        assert val is int and val >= 0 and val <= 256
        self.val = val


class Uint16:
    def __init__(self, val):
        assert val is int and val >= 0 and val <= pow(2, 16)
        self.val = val


class Uint32:
    def __init__(self, val):
        assert val is int and val >= 0 and val <= pow(2, 32)
        self.val = val


class Uint64:
    def __init__(self, val):
        assert val is int and val >= 0 and val <= pow(2, 64)
        self.val = val


class Sint8:
    def __init__(self, val):
        assert val is int and val >= -128 and val <= 127
        self.val = val


class Sint16:
    def __init__(self, val):
        assert val is int and val >= -pow(2, 15) and val <= (pow(2, 15) - 1)
        self.val = val


class Sint32:
    def __init__(self, val):
        assert val is int and val >= -pow(2, 31) and val <= (pow(2, 31) - 1)
        self.val = val


class Sint64:
    def __init__(self, val):
        assert val is int and val >= -pow(2, 63) and val <= (pow(2, 63) - 1)
        self.val = val


class String:
    def __init__(self, val):
        assert val is string
        self.val = val


class Bytes:
    def __init__(self, val):
        assert val is bytes
        self.val = val


def isScalar(val):
    return val is Bool or val is Uint8 or val is Uint16 or val is Uint32 or val is Uint64 or val is Sint8 or val is Sint16 or val is Sint32 or val is Sint64 or val is String or val is Bytes


class KVPair:
    def __init__(self, key, val):
        assert isScalar(key)
        self.key = key
        self.val = val


class List:
    def __init__(self, val):
        assert val is list
        self.val = val


class Map:
    def __init__(self, val):
        """ A map is a sorted list of KV pairs """
        assert val is list
        self.val = val


class Oneof:
    def __init__(self, val):
        """ A oneof is a list of message names """
        assert val is list
        self.val = val


class Message:
    """ A message is a structure with a unique id and a set of fields. Each field is primitive or compound type, or another Message. """

    def __init__(self, line_no, id):
        self.line_no = line_no
        self.id = id
        self.fields = list()


class SymbolTable:
    """ All parsed messages live as ASTs in the symbol table """

    def __init__(self):
        # Messages keyed by name
        self.messages = dict()


def parse_msg_definition(symbol_table, line_no, line):
    """ Parse a one line Msg definition and return a Message """
    tokens = str.split(line)
    if len(tokens) != 4:
        errmsg = 'Error: Expected Message Definition of format: "Msg <MsgName> <MsgId> {{" on line {}'.format(
            line_no)
        raise CmfException(errmsg)

    if tokens[0] != "Msg":
        raise CmfException(
            'Error: Expected a new message. Missing "Msg" token on line {}'.format(line_no))

    name = tokens[1]
    try:
        id = int(tokens[2])
    except ValueError:
        raise CmfException(
            "Error: Message Id must be an unsigned 32-bt integer on line {}".format(line_no))

    if id < 0 or id > pow(2, 32):
        raise CmfException(
            "Error: Message Id must be an unsigned 32-bt integer on line {}".format(line_no))

    if tokens[3] != '{':
        raise CmfException('Error: Missing opening brace. Expected Message Definition of format: "Msg < MsgName > <MsgId > {{" on line {}'.format(
            line_no))

    if name in symbol_table.messages:
        raise CmfException("Error: {} already defined at line {}. Duplicate definition on line {}.".format(
            name, symbol_table.messages[name].line_no, line_no))

    return Message(line_no, id)


def skip_comments(lines):
    count = 0
    for _, line in lines:
        if line.split()[0][0] == '#':
            count += 1
        else:
            return lines[count:]


def parse(filename):
    """
    Parse a cmf file and return a SymbolTable

    We parse line by line so that we can report parse errors with line numbers
    """
    symbol_table = SymbolTable()
    msg = None
    with open(filename) as f:
        lines = list(enumerate(f, start=1))
        lines = skip_comments(lines)
        msg = parse_msg_definition(symbol_table, *lines[0])
        lines = lines[1:]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Compile a concord message format file")
    parser.add_argument('--input', nargs=1,
                        help='The input filename', required=True)
    parser.add_argument('--output', nargs=1,
                        help='The output filename', required=True)
    parser.add_argument('--language', nargs=1,
                        help='The output language', choices=['cpp'], required=True)
    args = parser.parse_args()
    parse(args.input[0])
