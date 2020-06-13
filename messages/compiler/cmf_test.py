import cmf
import pytest


def test_skip_comments():
    lines = list(enumerate(['# ', '#', '  #blah  ', '  #  ', 'Msg'], start=1))
    lines = cmf.skip_comments(lines)
    assert len(lines) == 1
    assert lines[0] == (5, 'Msg')


def test_parse_msg_definition():
    st = cmf.SymbolTable()
    valid_def = 'Msg msg1 1 {'
    line_no = 1

    # Valid definition
    msg = cmf.parse_msg_definition(st, line_no, valid_def)
    assert msg.line_no == line_no
    assert msg.id == 1

    # Badly formatted message definition - not enough tokens
    with pytest.raises(cmf.CmfException) as ex:
        cmf.parse_msg_definition(st, line_no, "Msg")
        assert "format" in ex.message

    # Badly formatted message definition - doesn't start with 'Msg'
    with pytest.raises(cmf.CmfException) as ex:
        cmf.parse_msg_definition(st, line_no, "Message hello 1 {")
        assert 'Missing "Msg" token' in ex.message

    # Badly formatted message definition - Id is not an integer
    with pytest.raises(cmf.CmfException) as ex:
        cmf.parse_msg_definition(st, line_no, "Msg hello world {")
        assert 'integer' in ex.message

    # Badly formatted message definition - Id is a negative integer
    with pytest.raises(cmf.CmfException) as ex:
        cmf.parse_msg_definition(st, line_no, "Msg hello -1 {")
        assert 'integer' in ex.message

    # Badly formatted message definition - Id is too large of an integer
    with pytest.raises(cmf.CmfException) as ex:
        line = "Msg hello {} {{".format(pow(2, 32) + 1)
        cmf.parse_msg_definition(st, line_no, line)
        assert 'integer' in ex.message

    # Badly formatted message definition - missing opening brace
    with pytest.raises(cmf.CmfException) as ex:
        cmf.parse_msg_definition(st, line_no, "Msg hello 1 X")
        assert 'Missing opening brace' in ex.message

    # Insert msg into symbol_table and ensure that we can't add it twice
    st.messages['msg1'] = msg
    with pytest.raises(cmf.CmfException) as ex:
        cmf.parse_msg_definition(st, line_no+1, valid_def)
        assert 'already defined' in ex.message
