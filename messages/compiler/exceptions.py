class CmfParseError(Exception):
    def __init__(self, parseinfo, msg):
        self.message = '({}:{}) Error: {}'.format(
            parseinfo.line, parseinfo.pos, msg)

    def __str__(self):
        return self.message
