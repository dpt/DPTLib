#!/usr/bin/env python3
"""assert-based self-check for balance_braces.py"""
from balance_braces import reformat_text


def one(src):
    out, changed = reformat_text(src)
    return out if changed else None


def test_unbraced_else_gets_braces():
    src = (
        "  if (a)\n"
        "  {\n"
        "    x = 1;\n"
        "    y = 2;\n"
        "  }\n"
        "  else\n"
        "    x = 3;\n"
    )
    assert one(src) == (
        "  if (a)\n"
        "  {\n"
        "    x = 1;\n"
        "    y = 2;\n"
        "  }\n"
        "  else\n"
        "  {\n"
        "    x = 3;\n"
        "  }\n"
    )


def test_unbraced_if_in_else_if_chain_with_multiline_statement():
    src = (
        "  if (a)\n"
        "    foo(1,\n"
        "        2);\n"
        "  else if (b)\n"
        "  {\n"
        "    bar();\n"
        "  }\n"
    )
    assert one(src) == (
        "  if (a)\n"
        "  {\n"
        "    foo(1,\n"
        "        2);\n"
        "  }\n"
        "  else if (b)\n"
        "  {\n"
        "    bar();\n"
        "  }\n"
    )


def test_multiline_condition():
    src = (
        "  if (a &&\n"
        "      b)\n"
        "    x = 1;\n"
        "  else\n"
        "  {\n"
        "    x = 2;\n"
        "  }\n"
    )
    assert one(src) == (
        "  if (a &&\n"
        "      b)\n"
        "  {\n"
        "    x = 1;\n"
        "  }\n"
        "  else\n"
        "  {\n"
        "    x = 2;\n"
        "  }\n"
    )


def test_nested_chain_inside_braced_body():
    src = (
        "  if (a)\n"
        "  {\n"
        "    if (b)\n"
        "      y = 1;\n"
        "    else\n"
        "    {\n"
        "      y = 2;\n"
        "    }\n"
        "  }\n"
    )
    assert one(src) == (
        "  if (a)\n"
        "  {\n"
        "    if (b)\n"
        "    {\n"
        "      y = 1;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "      y = 2;\n"
        "    }\n"
        "  }\n"
    )


def test_all_unbraced_untouched():
    src = (
        "  if (a)\n"
        "    x = 1;\n"
        "  else\n"
        "    x = 2;\n"
    )
    assert one(src) is None


def test_all_braced_untouched():
    src = (
        "  if (a)\n"
        "  {\n"
        "    x = 1;\n"
        "  }\n"
        "  else\n"
        "  {\n"
        "    x = 2;\n"
        "  }\n"
    )
    assert one(src) is None


def test_inline_body_left_alone():
    src = (
        "  if (a) x = 1;\n"
        "  else\n"
        "  {\n"
        "    x = 2;\n"
        "  }\n"
    )
    assert one(src) is None


def test_control_statement_body_left_alone():
    src = (
        "  if (a)\n"
        "    for (;;)\n"
        "      x++;\n"
        "  else\n"
        "  {\n"
        "    x = 2;\n"
        "  }\n"
    )
    assert one(src) is None


def test_separate_if_after_chain_not_joined():
    src = (
        "  if (a)\n"
        "  {\n"
        "    x = 1;\n"
        "  }\n"
        "  if (b)\n"
        "    y = 1;\n"
    )
    assert one(src) is None


def test_trailing_comment_on_head():
    src = (
        "  if (a) /* why */\n"
        "    x = 1;\n"
        "  else\n"
        "  {\n"
        "    x = 2;\n"
        "  }\n"
    )
    assert one(src) == (
        "  if (a) /* why */\n"
        "  {\n"
        "    x = 1;\n"
        "  }\n"
        "  else\n"
        "  {\n"
        "    x = 2;\n"
        "  }\n"
    )


if __name__ == '__main__':
    import sys

    mod = sys.modules[__name__]
    for name in dir(mod):
        if name.startswith('test_'):
            getattr(mod, name)()
    print('all tests passed')
