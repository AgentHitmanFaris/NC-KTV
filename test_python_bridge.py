import pytest
from python_bridge import _normalize_word

def test_normalize_word_basic():
    assert _normalize_word("Hello") == "hello"
    assert _normalize_word("WORLD") == "world"
    assert _normalize_word("Python") == "python"

def test_normalize_word_punctuation():
    assert _normalize_word("hello!") == "hello"
    assert _normalize_word("world...") == "world"
    assert _normalize_word("it's") == "its"
    assert _normalize_word("can-not") == "cannot"
    assert _normalize_word("question?") == "question"
    assert _normalize_word("bracket(s)") == "brackets"
    assert _normalize_word("quote's\"") == "quotes"

def test_normalize_word_whitespace():
    # re.sub(r"[^\w]", "", word) will also remove spaces if they are passed as part of 'word'
    assert _normalize_word("hello world") == "helloworld"
    assert _normalize_word("  leading") == "leading"
    assert _normalize_word("trailing  ") == "trailing"

def test_normalize_word_numbers():
    assert _normalize_word("123") == "123"
    assert _normalize_word("v1.0") == "v10"

def test_normalize_word_unicode():
    # \w in Python 3 re matches Unicode word characters by default
    assert _normalize_word("Hélo") == "hélo"
    assert _normalize_word("München") == "münchen"
    assert _normalize_word("你好") == "你好"

def test_normalize_word_edge_cases():
    assert _normalize_word("") == ""
    assert _normalize_word("!!!") == ""
    assert _normalize_word("   ") == ""
