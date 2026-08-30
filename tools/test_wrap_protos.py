#!/usr/bin/env python3
"""assert-based self-check for wrap_protos.py"""
from wrap_protos import process, split_top_level

W = 80


def one(src):
    out, changed = process(src, W)
    return out if changed else None


def test_short_untouched():
    assert one("int f(int a, int b);\n") is None


def test_void_untouched():
    long = "static int " + "x" * 70 + "(void);\n"
    assert one(long) is None


def test_basic_alignment():
    src = ("static int screen_copy_rect_p4(screen_t *scr, const box_t *s, "
           "const box_t *d, int width, int height, int dx, int dy)\n")
    assert one(src) == (
        "static int screen_copy_rect_p4(screen_t    *scr,\n"
        "                               const box_t *s,\n"
        "                               const box_t *d,\n"
        "                               int          width,\n"
        "                               int          height,\n"
        "                               int          dx,\n"
        "                               int          dy)\n"
    )


def test_idempotent():
    src = ("static int screen_copy_rect_p4(screen_t *scr, const box_t *s, "
           "const box_t *d, int width, int height, int dx, int dy)\n")
    first = one(src)
    assert process(first, W)[1] is False, "second pass should be a no-op"


def test_rewrap_bad_alignment():
    # joined form is >80, and it arrives already (badly) wrapped
    src = (
        "static int screen_copy_rectangle_p4(screen_t *scr,\n"
        "    const box_t *s, const box_t *d,\n"
        "    int width, int height, int dx, int dy)\n"
    )
    assert one(src) == (
        "static int screen_copy_rectangle_p4(screen_t    *scr,\n"
        "                                    const box_t *s,\n"
        "                                    const box_t *d,\n"
        "                                    int          width,\n"
        "                                    int          height,\n"
        "                                    int          dx,\n"
        "                                    int          dy)\n"
    )


def test_short_multiline_untouched():
    # already multi-line but joins to <80 -- leave the hand grouping alone
    src = (
        "void draw_rect(screen_t *scr,\n"
        "               int x, int y,\n"
        "               colour_t colour);\n"
    )
    assert one(src) is None


def test_semicolon_tail_kept():
    src = ("extern void some_really_long_function_name_here(int first_argument, "
           "int second_argument, int third_arg);\n")
    got = one(src)
    assert got.rstrip().endswith(");")
    assert got.count("\n") == 3


def test_brace_tail_spaced():
    src = ("static long another_long_one_that_is_over_the_limit(char *buffer, "
           "unsigned long length, int flags) {\n")
    assert one(src).rstrip().endswith(") {")


def test_func_pointer_param_aligned():
    src = ("int register_a_callback_with_a_longish_name(void *ctx, "
           "int (*cb)(void *, int), unsigned long some_flags)\n")
    got = one(src)
    assert got == (
        "int register_a_callback_with_a_longish_name(void         *ctx,\n"
        "                                            int           (*cb)(void *, int),\n"
        "                                            unsigned long some_flags)\n"
    ), got


def test_unnamed_params_aligned():
    src = ("int a_function_with_unnamed_parameters_that_is_long(const char *, "
           "unsigned long, int);\n")
    got = one(src)
    assert got == (
        "int a_function_with_unnamed_parameters_that_is_long(const char   *,\n"
        "                                                    unsigned long,\n"
        "                                                    int);\n"
    ), got


def test_split_top_level_respects_nesting():
    assert split_top_level("int a, void (*f)(int, int), char *b") == \
        ["int a", "void (*f)(int, int)", "char *b"]


def test_variadic_bails():
    src = ("int a_printf_like_function_with_a_long_name(const char *fmt, "
           "int count, ...)\n")
    assert one(src) is None


def test_call_statement_not_touched():
    src = ("  array_squeeze(v->base, v->used, v->width, width_argument_here); "
           "// a call, way over 80 columns of course yes indeed\n")
    assert one(src) is None


def test_bare_call_not_touched():
    src = ("  some_function_call_that_is_quite_long(argument_one, argument_two, "
           "argument_three, argument_four);\n")
    assert one(src) is None


def test_if_statement_not_touched():
    src = ("  if (some_long_condition_function(a, b) && another_condition(c, d) "
           "&& yet_another_one(e))\n")
    assert one(src) is None


def test_method_call_not_touched():
    src = ("  obj->do_something_with_a_really_long_method_name(first_arg, "
           "second_arg, third_arg, fourth);\n")
    assert one(src) is None


def test_array_param_aligned():
    src = ("static result_t walk_test(ntree_t *t, ntree_walk_flags_t flags, "
           "const char *expected[])\n{\n")
    got = one(src)
    assert got == (
        "static result_t walk_test(ntree_t           *t,\n"
        "                          ntree_walk_flags_t flags,\n"
        "                          const char        *expected[])\n"
        "{\n"
    ), got
    # declarators all begin at the same column
    lines = got.splitlines()
    assert lines[0].index('*t') + 1 == lines[1].rindex('flags') \
        == lines[2].index('*expected') + 1


def test_return_statement_not_touched():
    src = ("  return atom_set(db->tags, db->counts[tag].index, "
           "(const unsigned char *) name, strlen((char *) name) + 1);\n")
    assert one(src) is None


def test_sizeof_call_not_touched():
    src = ("  return some_allocator_function_with_a_long_name(sizeof(*p), "
           "count_of_things, alignment_bytes);\n")
    assert one(src) is None


def test_comment_line_not_touched():
    src = (
        "/** Mark it dirty. Shorthand for\n"
        " * some_function_with_a_long_name(window, NULL, 0, extra_argument). */\n"
        "#define M(w) some_function_with_a_long_name((w), NULL, 0, 0)\n"
    )
    assert one(src) is None


def test_definition_still_wraps():
    src = ("result_t vector_set_width_with_a_longer_name(vector_t *v, "
           "size_t width, int flags, int more)\n{\n")
    got = one(src)
    assert got is not None and got.splitlines()[0].endswith("(vector_t *v,")


if __name__ == '__main__':
    for name, fn in sorted(globals().items()):
        if name.startswith('test_'):
            fn()
            print(f'ok  {name}')
    print('all passed')
