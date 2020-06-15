#!/usr/bin/env python

import argparse
import tatsu
from pprint import pprint
from exceptions import CmfParseError
from semantics import CmfSemantics, SymbolTable


def parse(grammar, cmf):
    parser = tatsu.compile(grammar)
    symbol_table = SymbolTable()
    ast = parser.parse(cmf, semantics=CmfSemantics(
        symbol_table), parseinfo=True)
    return ast, symbol_table


def translate(ast, symbol_table, language):
    if language == 'cpp':
        print("Generating C++ source code")
        import cpp_gen
        return cpp_gen.translate(ast, symbol_table)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Compile a concord message format file")
    parser.add_argument('--input', nargs=1,
                        help='The concord message format (CMF) input filename', required=True)
    parser.add_argument('--output', nargs=1,
                        help='The output filename', required=True)
    parser.add_argument('--language', nargs=1,
                        help='The output language', choices=['cpp'], required=True)
    args = parser.parse_args()
    try:
        with open('./grammar.ebnf') as f:
            grammar = f.read()
            with open(args.input[0]) as f2:
                cmf = f2.read()
            ast, symbol_table = parse(grammar, cmf)
            code = translate(ast, symbol_table, args.language[0])
            # Uncomment to show the generated AST for debugging purposes
            # pprint(ast)
            with open(args.output[0], 'w') as f3:
                f3.write(code)
    except Exception as ex:
        print(ex)
        exit(-1)
