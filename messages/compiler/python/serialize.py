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


class CmfDeserializeError(Exception):
    def __init__(self, msg):
        self.message = f'CMFDeserializationError: {msg}'

    def __str__(self):
        return self.message


class NoDataLeftError(CmfDeserializeError):
    def __init__(self):
        super().__init__(
            'Data left in buffer is less than what is needed for deserialization'
        )


class BadDataError(CmfDeserializeError):
    def __init__(self, expected, actual):
        super().__init__(f'Expected {expected}, got {actual}')


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
        self.buf.extend(struct.pack('B', val))

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
        self.buf.extend(struct.pack('b', val))

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


class CMFDeserializer():
    def __init__(self, buf):
        self.buf = buf
        self.pos = 0

    def deserialize(self, serializers):
        '''
        Recursively deserialize `self.buf` using `serializers`
        '''
        s = serializers[0]
        if s in ['kvpair', 'list', 'map', 'optional']:
            return getattr(self, s)(serializers[1:])
        elif type(s) is tuple and len(s) == 2 and s[0] == 'oneof' and type(
                s[1]) is dict:
            return self.oneof(s[1])
        elif is_primitive(s) or s == 'msg':
            return getattr(self, s)()
        else:
            raise CmfDeserializeError(f'Invalid serializer: {s}')

    def bool(self):
        if self.pos + 1 > len(self.buf):
            raise NoDataLeftError()
        val = self.buf[self.pos]
        if val == 1:
            self.pos += 1
            return True
        elif val == 0:
            self.pos += 1
            return False
        raise BadDataError('0 or 1', val)

    def uint8(self):
        if self.pos + 1 > len(self.buf):
            raise NoDataLeftError()
        val = struct.unpack_from('B', self.buf, self.pos)
        self.pos += 1
        return val

    def uint16(self):
        if self.pos + 2 > len(self.buf):
            raise NoDataLeftError()
        val = struct.unpack_from('<H', self.buf, self.pos)
        self.pos += 2
        return val

    def uint32(self):
        if self.pos + 4 > len(self.buf):
            raise NoDataLeftError()
        val = struct.unpack_from('<I', self.buf, self.pos)
        self.pos += 4
        return val

    def uint64(self):
        if self.pos + 8 > len(self.buf):
            raise NoDataLeftError()
        val = struct.unpack_from('<Q', self.buf, self.pos)
        self.pos += 8
        return val

    def int8(self):
        if self.pos + 1 > len(self.buf):
            raise NoDataLeftError()
        val = struct.unpack_from('b', self.buf, self.pos)
        self.pos += 1
        return val

    def int16(self):
        if self.pos + 2 > len(self.buf):
            raise NoDataLeftError()
        val = struct.unpack_from('<h', self.buf, self.pos)
        self.pos += 2
        return val

    def int32(self):
        if self.pos + 4 > len(self.buf):
            raise NoDataLeftError()
        val = struct.unpack_from('<i', self.buf, self.pos)
        self.pos += 4
        return val

    def int64(self):
        if self.pos + 8 > len(self.buf):
            raise NoDataLeftError()
        val = struct.unpack_from('<q', self.buf, self.pos)
        self.pos += 8
        return val

    def string(self):
        if self.pos + 4 > len(self.buf):
            raise NoDataLeftError()
        size = self.uint32()
        self.pos += 4
        if self.pos + size > len(self.buf):
            raise NoDataLeftError()
        val = str(self.buf[self.pos:self.pos + size], 'utf-8')
        self.pos += size
        return val
