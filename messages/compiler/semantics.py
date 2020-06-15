from exceptions import CmfParseError


class SymbolTable:
    def __init__(self):
        # Used during semantic analysis to detect duplicates
        # Line numbers are stored as values
        self.msg_ids = dict()
        self.msg_names = dict()

        # Used during translation to do further typechecking and ordering of output.
        # Oneofs contain non-circular references to other messages. In most languages those
        # messages need to be declared before the oneof.
        self.current_msg = None
        self.msgs_with_oneofs = []

    def validate_msg_names(self, names, parseinfo):
        diff = set(names).difference(set(self.msg_names.keys()))
        if len(diff) != 0:
            raise CmfParseError(
                parseinfo, "Messages in oneof do not exist: {}".format(diff))


class CmfSemantics(object):
    """ Perform basic type checking and conversion on the AST"""

    def __init__(self, symbol_table):
        self.symbol_table = symbol_table

    def msgid(self, ast):
        """ Check that each message id is unique and fits in a 32 bit integer """
        id = int(ast.id)
        if id < 0 or id > pow(2, 32):
            raise CmfParseError(ast.parseinfo,
                                'Message ID: "{}" must fit in a uint32'.format(id))
        if id in self.symbol_table.msg_ids:
            raise CmfParseError(ast.parseinfo, 'Message ID: "{}" already declared on line {}'.format(
                id, self.symbol_table.msg_ids[id]))
        self.symbol_table.msg_ids[id] = ast.parseinfo.line
        return id

    def msgname(self, ast):
        """ Check that each message name is unique """
        if ast.name in self.symbol_table.msg_names:
            raise CmfParseError(ast.parseinfo, 'Message: "{}" already declared on line {}'.format(
                ast.name, self.symbol_table.msg_names[ast.name]))
        self.symbol_table.msg_names[ast.name] = ast.parseinfo.line
        return ast.name

    def msg(self, ast):
        """ Check that each field in a message has a unique name """
        field_names = set()
        for field in ast.fields:
            if field.name in field_names:
                raise CmfParseError(
                    ast.parseinfo, 'Message: "{}" contains duplicate field: "{}"'.format(ast.name, field.name))
            field_names.add(field.name)
        return ast
