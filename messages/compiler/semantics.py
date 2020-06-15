from exceptions import CmfParseError


class MsgWithOneof:
    def __init__(self, name):
        self.name = name
        self.message_names_in_oneof = set()


class SymbolTable:
    def __init__(self):
        # Used during semantic analysis to detect duplicates
        # Line numbers are stored as values
        self.msg_ids = dict()
        self.msg_names = dict()


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
            raise CmfParseError(ast.parseinfo, 'Message ID: "{}" already defined on line {}'.format(
                id, self.symbol_table.msg_ids[id]))
        # parseinfo.line is zero-based
        self.symbol_table.msg_ids[id] = ast.parseinfo.line + 1
        return id

    def msgname(self, ast):
        """ Check that each message name is unique """
        if ast.name in self.symbol_table.msg_names:
            raise CmfParseError(ast.parseinfo, 'Message: "{}" already defined on line {}'.format(
                ast.name, self.symbol_table.msg_names[ast.name]))
        # parseinfo.line is zero-based
        self.symbol_table.msg_names[ast.name] = ast.parseinfo.line + 1
        return ast.name

    def oneof(self, ast):
        """ Messages must already be defined before being used in a oneof """
        diff = set(ast.msg_names).difference(
            set(self.symbol_table.msg_names.keys()))
        if len(diff) != 0:
            raise CmfParseError(
                ast.parseinfo, "Messages in oneof must be defined before they are referenced: {}".format(diff))
        return ast

    def msg(self, ast):
        """ Check that each field in a message has a unique name """
        field_names = set()
        for field in ast.fields:
            if field.name in field_names:
                raise CmfParseError(
                    ast.parseinfo, 'Message: "{}" contains duplicate field: "{}"'.format(ast.name, field.name))
            field_names.add(field.name)
        return ast
