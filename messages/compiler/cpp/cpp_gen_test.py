import random
from cpp_gen import is_int


def rand_int():
    return random.randint(0, 100)


def rand_string():
    return random.choice(['a', 'b', 'c', 'aa' 'bb' 'cc' 'abcdef'])


def byte_example():
    return '{0,1,2,3,4,5}'


class InstanceGen:
    def __init__(self, size, msgname_to_ast_node):
        self.size = size
        self.msgname_to_ast_node = msgname_to_ast_node

    def instancestr(self, type):
        """
        Return an instance of a given type. Do this recursively for nested types like lists and maps.
        """
        if not isinstance(type, dict):
            # A primitive type
            if is_int(type):
                return '{' + str(rand_int()) + '}'
            if type == 'string':
                return '{"' + rand_string() + '"}'
            if type == 'bytes':
                return byte_example()
            if type == 'bool':
                return '{' + random.choice(['true', 'false']) + '}'
        elif self.size == 0:
            return '{}'

        # TODO: Support multiple values in a type according to size
        elif 'list' in type:
            return '{' + self.instancestr(type.list.type) + '}'
        elif 'kvpair' in type:
            return '{' + self.instancestr(type.kvpair.key) + ', ' + self.instancestr(type.kvpair.value) + '}'
        elif 'map' in type:
            return '{' + self.instancestr(type.map.key) + ', ' + self.instancestr(type.map.value) + '}'
        elif 'optional' in type:
            return '{' + self.instancestr(type.optional.type) + '}'
        elif 'oneof' in type:
            return self.structInstanceGen(self.msgname_to_ast_node[random.choice(type.oneof.msg_names)], self.size)

    def structInstanceGen(self, msg, size):
        """
        Take an AST for a message and serialize an instance of the struct.

        `size` is a parameter that triggers the growth of lists and maps to determine how many values they have.
        Tests will generate N instances, each with a different size.
        """
        s = '{} __{}__{}{{'.format(msg.name, msg.name, size)
        for field in msg.fields:
            s += self.instancestr(field.type)
        s += '}};'
        return s
