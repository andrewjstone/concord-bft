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

from visitor import Visitor


def class_start(name, id):
    return f'''
class {name}():
    \'\'\' A CMF message for {name} \'\'\'
    id = {id}

    def __init__(self):
'''


def serialize_start():
    return f'''
    def serialize(self) -> bytes:
        \'\'\' Serialize this message in CMF format \'\'\'
        serializer = CMFSerializer()
'''


def deserialize_start():
    return f'''
    @classmethod
    def deserialize(cls, buf):
        \'\'\' Take bytes of a serialized CMF message, deserialize it, and return a new instance of this class. \'\'\'
        deserializer = CMFDeserializer(buf)
        obj = cls()
'''


def serialize_field(field_name, serializers):
    return f'''\
        serializer.serialize(self.{field_name}, {serializers})
'''


def deserialize_field(field_name, serializers):
    return f'''\
        obj.{field_name} = deserializer.deserialize({serializers})
'''


class PyVisitor(Visitor):
    ''' A visitor that generates Python code. '''
    def __init__(self):
        # All output currently constructed
        self.output = ''

        # The current message being processed
        self.msg_name = ''

        # The current field name being processed
        self.field_name = ''

        # Messages are represented as Classes in python
        self.msg_class = ''

        # A list of string names of serialization functions for the current field. It's a list
        # because types can be nested. The same function name is used for serialization and
        # deserialization via getattr.
        self.serializers = []

        # The 'serialize' method for the current message
        self.serialize = ''

        # The 'deserialize' method for the current message
        self.deserialize = ''

    def _reset(self):
        # output accumulates across messages
        output = self.output
        self.__init__()
        self.output = output

    def _reset_field(self):
        self.field_name = ''
        self.serializers = []

    def msg_start(self, name, id):
        self.msg_name = name
        self.msg_class = class_start(name, id)
        self.serialize = serialize_start()
        self.deserialize = deserialize_start()

    def msg_end(self):
        self.serialize += '        return serializer.buf'
        self.deserialize += '        return obj, deserializer.pos\n'
        self.output += '\n'.join(
            [self.msg_class, self.serialize, self.deserialize])
        self._reset()

    def field_start(self, name, type):
        self.field_name = name
        self.msg_class += f'         self.{name} = None\n'

    def field_end(self):
        self.serialize += serialize_field(self.field_name, self.serializers)
        self.deserialize += deserialize_field(self.field_name,
                                              self.serializers)
        self._reset_field()

    def bool(self):
        self.serializers.append('bool')

    def uint8(self):
        self.serializers.append('uint8')

    def uint16(self):
        self.serializers.append('uint16')

    def uint32(self):
        self.serializers.append('uint32')

    def uint64(self):
        self.serializers.append('uint64')

    def int8(self):
        self.serializers.append('int8')

    def int16(self):
        self.serializers.append('int16')

    def int32(self):
        self.serializers.append('int32')

    def int64(self):
        self.serializers.append('int64')

    def string(self):
        self.serializers.append('string')

    def bytes(self):
        self.serializers.append('bytes')

    def msgname_ref(self, name):
        self.serializers.append(('msg', name))

    def kvpair_start(self):
        self.serializers.append('kvpair')

    def kvpair_key_end(self):
        pass

    def kvpair_end(self):
        pass

    def list_start(self):
        self.serializers.append('list')

    def list_end(self):
        pass

    def map_start(self):
        self.serializers.append('map')

    def map_key_end(self):
        pass

    def map_end(self):
        pass

    def optional_start(self):
        self.serializers.append('optional')

    def optional_end(self):
        pass

    def oneof(self, msgs):
        self.serializers.append(('oneof', msgs))
