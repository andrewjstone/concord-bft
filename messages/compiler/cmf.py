#!/usr/bin/python

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

def isScalar(val) {
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
    def __init__(self, id, fields):
        assert fields is list
        self.id = id
        self.fields = fields
}

class SymbolTable:
    def __init__(self):
    # messages keyed by name
    messages = dict{}
