#!/usr/bin/env python3
"""realign_decls.py -- tighten C declaration-block alignment

For each contiguous block of simple variable declarations at the top of a
function scope, re-pads the column between the type and the name/'*' so it
sits exactly one space past the block's own longest type token, rather than
whatever wider column happened to be in use before.

Only touches declaration lines (`type name[, name2][ = init];`, optionally
`static`/`const` qualified, optionally array-subscripted, optionally with a
leading `*` on the name), and only inside executable-code braces -- never
inside a struct/enum/union body. Leaves function signatures, macros,
comments and all other code untouched.

Usage: realign_decls.py FILE [FILE ...]
Edits files in place.
"""
import re
import sys

DECL_RE = re.compile(
    r'^(?P<indent>[ \t]*)'
    r'(?P<qual>(?:static\s+|const\s+)*)'
    r'(?P<type>[A-Za-z_][A-Za-z0-9_]*(?:\s+(?:const|unsigned|signed|long|short|int|char)\b)*)'
    r'(?P<space>\s+)'
    r'(?P<stars>\*+\s*)?'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)'
    r'(?P<tail>(?:\s*\[[^\]]*\])*\s*(?:,\s*\*{0,3}\s*[A-Za-z_][A-Za-z0-9_]*(?:\s*\[[^\]]*\])*)*\s*(?:=[^;]*)?;.*)$'
)

KEYWORDS = {
    'return', 'if', 'else', 'for', 'while', 'do', 'switch', 'break',
    'continue', 'goto', 'sizeof', 'typedef', 'struct', 'enum', 'union',
    'default', 'case',
}

# a line that (on its own, or Allman-style with '{' on the next line)
# opens a struct/enum/union body -- its members must never be treated as a
# realignable declaration block
STRUCT_KW_RE = re.compile(r'\b(struct|enum|union)\b[^;{]*$')
STRUCT_OPEN_SAMELINE_RE = re.compile(r'\b(struct|enum|union)\b[^;{]*\{\s*$')
BARE_BRACE_RE = re.compile(r'^\s*\{\s*$')

# lines that never end a declaration block and never count as a decl:
# blank lines and comment-only lines. They're transparent -- a block can
# run across them without breaking.
TRANSPARENT_RE = re.compile(r'^\s*(//.*|/\*.*\*/\s*)?$')


def parse_decl(line):
    m = DECL_RE.match(line)
    if not m:
        return None
    type_tok = (m.group('qual') + m.group('type')).strip()
    if type_tok.split()[-1] in KEYWORDS:
        return None
    tail = m.group('name') + m.group('tail')
    body = tail.split(';')[0]
    has_call = '(' in body
    return {
        'indent': m.group('indent'),
        'type': type_tok,
        'stars': (m.group('stars') or ''),
        'rest': tail,
        'editable': not has_call,  # counts toward width either way
        'has_init': '=' in body,
    }


def struct_body_lines(lines):
    """Return the set of line indices that lie inside a struct/enum/union
    body -- opened either by '... struct X {' on one line, or Allman-style
    with the keyword line followed by a bare '{' -- so declaration scanning
    can skip them."""
    in_struct = set()
    stack = []  # stack of bool: True if this brace level is a struct body
    pending_struct = False  # previous line was a bare struct/enum/union kw
    for i, line in enumerate(lines):
        is_struct_open = bool(STRUCT_OPEN_SAMELINE_RE.search(line))
        if not is_struct_open and pending_struct and BARE_BRACE_RE.match(line):
            is_struct_open = True
        pending_struct = bool(STRUCT_KW_RE.search(line.strip())) and \
            not line.strip().endswith(';') and '{' not in line
        for ch in line:
            if ch == '{':
                stack.append(is_struct_open)
                is_struct_open = False
            elif ch == '}':
                if stack:
                    stack.pop()
        if stack and stack[-1]:
            in_struct.add(i)
    return in_struct


def find_blocks(lines, skip):
    """Yield (start, end, entries) for consecutive declaration lines sharing
    the same indent, tolerating blank/comment-only lines in between (they
    don't break the run, but aren't decls either), skipping any line index
    in `skip`. entries[k] is None for a transparent (blank/comment) line."""
    blocks = []
    i = 0
    n = len(lines)
    while i < n:
        if i in skip:
            i += 1
            continue
        parsed = parse_decl(lines[i])
        if parsed is None:
            i += 1
            continue
        indent = parsed['indent']
        start = i
        j = i
        entries = []
        last_real = parsed
        pending_transparent = False
        while j < n and j not in skip:
            line = lines[j]
            p = parse_decl(line)
            if p is not None and p['indent'] == indent:
                if pending_transparent and p['has_init'] != last_real['has_init']:
                    # a blank/comment line separating an initialised decl
                    # from an uninitialised one (or vice versa) marks a
                    # deliberate group break (see
                    # feedback_var_declaration_order) -- stop here, don't
                    # bridge the two groups into one aligned block.
                    break
                entries.append(p)
                last_real = p
                pending_transparent = False
                j += 1
                continue
            if p is None and TRANSPARENT_RE.match(line):
                entries.append(None)
                pending_transparent = True
                j += 1
                continue
            break
        # trim trailing transparent lines -- they belong to whatever follows,
        # not to this block
        while entries and entries[-1] is None:
            entries.pop()
            j -= 1
        blocks.append((start, j, entries))
        i = j
    return blocks


def reformat_block(lines, start, end, entries):
    # the name (or, for a pointer, its '*') must start in the same column
    # for every editable line in the block; a pointer's '*' occupies a
    # column of its own, so it counts toward each line's "effective"
    # type-side width. Lines with a call in the initializer, and blank/
    # comment lines, count toward the width but are never rewritten.
    def effective_len(d):
        return len(d['type']) + len(d['stars'].rstrip())

    real = [d for d in entries if d is not None]
    name_col = max(effective_len(d) for d in real) + 1
    for k, idx in enumerate(range(start, end)):
        d = entries[k]
        if d is None or not d['editable']:
            continue
        type_tok = d['type']
        stars = d['stars'].rstrip()
        rest = d['rest']
        pad = name_col - effective_len(d)
        new_line = '{}{}{}{}{}'.format(
            d['indent'], type_tok, ' ' * pad, stars, rest)
        lines[idx] = new_line


def process_file(path):
    with open(path, 'r') as f:
        text = f.read()
    had_trailing_newline = text.endswith('\n')
    lines = text.split('\n')
    if had_trailing_newline:
        lines = lines[:-1]

    skip = struct_body_lines(lines)
    blocks = find_blocks(lines, skip)
    changed = False
    for start, end, entries in blocks:
        real = [d for d in entries if d is not None]
        if len(real) < 2:
            continue
        before = lines[start:end]
        reformat_block(lines, start, end, entries)
        if lines[start:end] != before:
            changed = True

    if changed:
        out = '\n'.join(lines)
        if had_trailing_newline:
            out += '\n'
        with open(path, 'w') as f:
            f.write(out)
    return changed


def main(argv):
    any_changed = False
    for path in argv[1:]:
        if process_file(path):
            print('reformatted: {}'.format(path))
            any_changed = True
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
