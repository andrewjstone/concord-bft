#!/usr/bin/env python

import argparse
import tatsu
from pprint import pprint


class CmfException(Exception):
    def __init__(self, msg):
        self.message = msg


def parse(grammar, cmf):
    parser = tatsu.compile(grammar)
    ast = parser.parse(cmf)
    pprint(ast, width=20, indent=4)


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
