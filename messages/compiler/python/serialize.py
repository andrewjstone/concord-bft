# Concord
#
# Copyright (c) 2020 VMware, Inc. All Rights Reserved.
#
# This product is licensed to you under the Apache 2.0 license (the 'License').
# You may not use this product except in compliance with the Apache 2.0 License.
#
# This product may include a number of subcomponents with separate copyright
# notices and license terms. Your use of these subcomponents is subject to the
# terms and conditions of the subcomponent's license, as noted in the LICENSE
# file.

import struct
from functools import partialmethod


def is_primitive(s):
    return s in [
        'bool', 'string', 'bytes', 'uint8', 'uint16', 'uint32', 'uint64',
        'int8', 'int16', 'int32', 'int64'
    ]


class CmfSerializeError(Exception):
    def __init__(self, msg):
        self.message = f'CMFSerializationError: {msg}'

    def __str__(self):
        return self.message


class CMFSerializer():
    def __init__(self):
        self.buf = bytearray()

    def validate_int(self, val, min, max):
        if not type(val) is int:
            raise CmfSerializeError(f'Expected integer value, got {type(val)}')
        if val < min:
            raise CmfSerializeError(
                f'Expected integer value less than {min}, got {val}')
        if val > max:
            raise CmfSerializeError(
                f'Expected integer value less than {max}, got {val}')

    def serialize(self, val, serializers):
        '''
        Serialize any nested types in by applying the methods in `serializers` at each level.
        This method interacts with those below in a mutually recursive manner for nested types.
        '''
        s = serializers[0]
        if s in ['kvpair', 'list', 'map', 'optional']:
            getattr(self, s)(val, serializers[1:])
        elif type(s) is tuple and len(s) == 2 and s[0] == 'oneof' and type(
                s[1]) is dict:
            self.oneof(val, s[1])
        elif is_primitive(s) or s == 'msg':
            getattr(self, s)(val)
        else:
            raise CmfSerializeError(f'Invalid serializer: {s}, val = {val}')

    ###
    # Serialization functions for types that compose fields
    ###
    def bool(self, val):
        if not type(val) is bool:
            raise CmfSerializeError(f'Expected bool, got {type(val)}')
        if val:
            self.buf.append(1)
        else:
            self.buf.append(0)

    def uint8(self, val):
        self.validate_int(val, 0, 255)
        self.buf.append(val)

    def uint16(self, val):
        self.validate_int(val, 0, 65536)
        self.buf.extend(struct.pack('<H', val))

    def uint32(self, val):
        self.validate_int(val, 0, 4294967296)
        self.buf.extend(struct.pack('<I', val))

    def uint64(self, val):
        self.validate_int(val, 0, 18446744073709551616)
        self.buf.extend(struct.pack('<Q', val))

    def int8(self, val):
        self.validate_int(val, -128, 127)
        self.buf.append(val)

    def int16(self, val):
        self.validate_int(val, -32768, 32767)
        self.buf.extend(struct.pack('<h', val))

    def int32(self, val):
        self.validate_int(val, -2147483648, 2147483647)
        self.buf.extend(struct.pack('<i', val))

    def int64(self, val):
        self.validate_int(val, -9223372036854775808, 9223372036854775807)
        self.buf.extend(struct.pack('<q', val))

    def string(self, val):
        if not type(val) is str:
            raise CmfSerializeError(f'Expected string, got {type(val)}')
        self.uint32(len(val))
        self.buf.extend(bytes(val, 'utf-8'))

    def bytes(self, val):
        if not type(val) is bytes:
            raise CmfSerializeError(f'Expected bytes, got {type(val)}')
        self.uint32(len(val))
        self.buf.extend(val)

    def msg(self, msg, serializers):
        self.buf.extend(msg.serialize())

    def kvpair(self, pair, serializers):
        self.serialize(pair[0], serializers)
        self.serialize(pair[1], serializers[1:])

    def list(self, items, serializers):
        self.uint32(len(items))
        for val in items:
            self.serialize(val, serializers)

    def map(self, dictionary, serializers):
        self.uint32(len(dictionary))
        for k, v in dictionary.items():
            self.serialize(k, serializers)
            self.serialize(v, serializers[1:])

    def optional(self, val, serializers):
        if val is None:
            self.bool(False)
        else:
            self.bool(True)
            self.serialize(val, serializers)

    def oneof(self, val, msgs):
        if val.__class__.__name__ in msgs.keys():
            self.uint32(val.id)
            self.buf.extend(val.serialize())
