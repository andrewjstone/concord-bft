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

    def serialize_field(self, val, serializers):
        ''' Walk any nested types in val, and apply the functions in serializers at each level '''
        if type(val) in [int, str, bytes]:
            # Primitive type
            assert len(serializers) == 1
            getattr(self, serializers[0])(val)

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
