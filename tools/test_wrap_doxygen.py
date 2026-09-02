#!/usr/bin/env python3
"""Smallest possible smoke test for wrap_doxygen.py. Run directly:
python3 tools/test_wrap_doxygen.py
"""
from wrap_doxygen import process

src = '''/**
 * A short line.
 *
 * \\param[in] thing This description is deliberately long enough that it has to wrap across more than one line to fit.
 * \\return Also deliberately long enough that this return description needs to wrap across two lines at least.
 */
int f(int thing);

/** This single-line comment is deliberately long enough to need promoting to a multi-line block comment. */
int g(void);
'''.splitlines()

out, overflow = process(src, width=77)

assert overflow == [], overflow
assert all(len(l) <= 77 for l in out), [l for l in out if len(l) > 77]
assert out[0] == '/**'
assert out[1] == ' * A short line.'
# continuation lines must keep the "*" one column right of "/**", with a
# space after it -- regression check for the "* text" (no leading space)
# alignment bug this script originally had.
cont = [l for l in out if 'wrap across more than' in l]
assert cont and cont[0][:3] == ' * ', repr(cont)
assert 'int g(void);' in out
assert out.count('/**') == 2  # original block + the promoted single-liner

# idempotent: reformatting already-reformatted text changes nothing
out2, overflow2 = process(out, width=77)
assert out2 == out
assert overflow2 == []

print('ok')
