#!/usr/bin/env python

import argparse
import tatsu
from pprint import pprint


class CmfParseError(Exception):
    def __init__(self, parseinfo, msg):
        self.message = '({}:{}) Error: {}'.format(
            parseinfo.line, parseinfo.pos, msg)

    def __str__(self):
        return self.message


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


def parse(grammar, cmf):
    parser = tatsu.compile(grammar)
    symbol_table = SymbolTable()
    try:
        ast = parser.parse(cmf, semantics=CmfSemantics(
            symbol_table), parseinfo=True)
        pprint(ast, width=20, indent=4)
    except Exception as ex:
        print(ex)
        exit(-1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Compile a concord message format file")
    parser.add_argument('--input', nargs=1,
                        help='The concord message format (CMF) input filename', required=True)
   #    parser.add_argument('--output', nargs=1,
   #                        help='The output filename', required=True)
   #    parser.add_argument('--language', nargs=1,
   #                        help='The output language', choices=['cpp'], required=True)
    args = parser.parse_args()
    with open('./grammar.ebnf') as f:
        grammar = f.read()
        with open(args.input[0]) as f2:
            cmf = f2.read()
        parse(grammar, cmf)
