#!/usr/bin/env python3
"""assert-based self-check for realign_decls.py"""
from realign_decls import reformat_text


def one(src):
    out, changed = reformat_text(src)
    return out if changed else None


def test_pointer_star_column():
    src = (
        "  int ci;\n"
        "  box_t *cbox;\n"
        "  int cross_extent;\n"
    )
    assert one(src) == (
        "  int    ci;\n"
        "  box_t *cbox;\n"
        "  int    cross_extent;\n"
    )


def test_already_aligned_untouched():
    src = (
        "  int    ci;\n"
        "  box_t *cbox;\n"
    )
    assert one(src) is None


def test_struct_body_untouched():
    src = (
        "typedef struct foo\n"
        "{\n"
        "  int a;\n"
        "  box_t *b;\n"
        "}\n"
        "foo_t;\n"
    )
    assert one(src) is None


def test_array_subscript():
    src = (
        "  stack__child_t children[n];\n"
        "  int nchildren;\n"
    )
    assert one(src) == (
        "  stack__child_t children[n];\n"
        "  int            nchildren;\n"
    )


def test_blank_and_comment_bridge_same_group():
    # both lines uninitialised -> the blank/comment gap is transparent
    src = (
        "  int left_skip;\n"
        "\n"
        "  /* note */\n"
        "  unsigned int clamped_pos_x;\n"
    )
    assert one(src) == (
        "  int          left_skip;\n"
        "\n"
        "  /* note */\n"
        "  unsigned int clamped_pos_x;\n"
    )


def test_call_initializer_counts_width_but_not_rewritten():
    src = (
        "  int log2bpp;\n"
        "  pixelfmt_any_t nativefg = colour_to_pixel(scr->palette, fg);\n"
    )
    out = one(src)
    assert out == (
        "  int            log2bpp;\n"
        "  pixelfmt_any_t nativefg = colour_to_pixel(scr->palette, fg);\n"
    )


def test_init_group_break_across_blank_line():
    # 'quantum' is initialised; the following group is not -- the blank
    # line between them must NOT bridge the two into one aligned block
    src = (
        "  const size_t quantum = sizeof(free_t);\n"
        "\n"
        "  int nentries;\n"
        "  cache_t *c;\n"
    )
    assert one(src) == (
        "  const size_t quantum = sizeof(free_t);\n"
        "\n"
        "  int      nentries;\n"
        "  cache_t *c;\n"
    )


def test_single_decl_untouched():
    assert one("  int only_one;\n") is None


def test_idempotent():
    src = (
        "  int ci;\n"
        "  box_t *cbox;\n"
    )
    out1 = one(src)
    out2 = one(out1)
    assert out2 is None


if __name__ == '__main__':
    import sys

    mod = sys.modules[__name__]
    for name in dir(mod):
        if name.startswith('test_'):
            getattr(mod, name)()
    print('all tests passed')
