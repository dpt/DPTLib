#!/usr/bin/env python3
"""balance_braces.py -- brace every branch of a partly-braced if/else chain

For each Allman-style `if` / `else if` / `else` chain where some branches
have braces and some don't, wraps each unbraced single-statement body in
braces at the chain's indent, so all branches match.

Conservative: a chain is left untouched if any branch has something this
line-based scan can't be sure of -- a body on the same line as its head
(`if (x) foo();`), a body that is itself a control statement, a body led by
a comment or preprocessor line, or a K&R-style `{` on the head line.

Usage: balance_braces.py FILE [FILE ...]
Edits files in place.
"""
import re
import sys

HEAD_RE = re.compile(r'^(?P<indent>[ \t]*)(?P<kw>else\s+if|if|else)\b(?P<rest>.*)$')
CONTROL_RE = re.compile(r'^(if|else|for|while|do|switch)\b')


class Unsafe(Exception):
    pass


def strip_comment(s):
    """Drop a trailing // or single-line /* */ comment (not string-aware)."""
    s = re.sub(r'/\*.*?\*/', '', s)
    i = s.find('//')
    if i >= 0:
        s = s[:i]
    return s.rstrip()


def parse_head(lines, i):
    """Return (indent, kw, index of the head's last line) for the chain
    branch head starting at line i, or None if line i isn't a head."""
    m = HEAD_RE.match(lines[i])
    if m is None:
        return None
    indent, kw, rest = m.group('indent'), m.group('kw'), m.group('rest')
    if kw == 'else':
        if strip_comment(rest) != '':
            raise Unsafe()  # `else foo();` or `else {`
        return indent, kw, i
    rest = rest.lstrip()
    if not rest.startswith('('):
        return None  # e.g. an identifier merely starting with "if"
    depth = 0
    j = i
    text = rest
    while True:
        for k, c in enumerate(text):
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    if strip_comment(text[k + 1:]) != '':
                        raise Unsafe()  # inline body or K&R brace
                    return indent, kw, j
        j += 1
        if j >= len(lines):
            raise Unsafe()
        text = lines[j]


def parse_body(lines, i, indent):
    """Return (braced, last line index) for the body starting at line i."""
    if i >= len(lines):
        raise Unsafe()
    line = lines[i]
    if line.strip() == '{':
        if line[:len(line) - len(line.lstrip())] != indent:
            raise Unsafe()
        for j in range(i + 1, len(lines)):
            if lines[j] == indent + '}' or \
               (lines[j].startswith(indent + '}') and
                    strip_comment(lines[j][len(indent) + 1:]) == ''):
                return True, j
        raise Unsafe()
    body = line.strip()
    if not line.startswith(indent) or len(line) - len(line.lstrip()) <= len(indent):
        raise Unsafe()
    if body == '' or body.startswith(('/*', '//', '#', '{')) or CONTROL_RE.match(body):
        raise Unsafe()
    paren = brace = 0
    for j in range(i, len(lines)):
        code = strip_comment(lines[j])
        if code.lstrip().startswith('#'):
            raise Unsafe()
        paren += code.count('(') - code.count(')')
        brace += code.count('{') - code.count('}')
        if paren == 0 and brace == 0 and code.endswith(';'):
            return False, j
    raise Unsafe()


def parse_chain(lines, i):
    """Return [(body_start, body_end, braced)] for the chain whose `if` head
    starts at line i, plus the chain's indent."""
    branches = []
    head = parse_head(lines, i)
    indent = head[0]
    while True:
        _, kw, head_end = head
        braced, body_end = parse_body(lines, head_end + 1, indent)
        branches.append((head_end + 1, body_end, braced))
        if kw == 'else':
            break
        nxt = body_end + 1
        if nxt >= len(lines):
            break
        m = HEAD_RE.match(lines[nxt])
        if m is None or m.group('indent') != indent or m.group('kw') == 'if':
            break
        head = parse_head(lines, nxt)
    return indent, branches


def reformat_text(src):
    lines = src.split('\n')
    opens = {}   # line index -> indent: insert "{" before it
    closes = {}  # line index -> indent: insert "}" after it

    for i, line in enumerate(lines):
        m = HEAD_RE.match(line)
        if m is None or m.group('kw') != 'if':
            continue
        try:
            if parse_head(lines, i) is None:
                continue
            indent, branches = parse_chain(lines, i)
        except Unsafe:
            continue
        kinds = {braced for _, _, braced in branches}
        if len(kinds) != 2:
            continue
        for start, end, braced in branches:
            if not braced:
                opens[start] = indent
                closes[end] = indent

    if not opens:
        return src, False

    out = []
    for i, line in enumerate(lines):
        if i in opens:
            out.append(opens[i] + '{')
        out.append(line)
        if i in closes:
            out.append(closes[i] + '}')
    return '\n'.join(out), True


def process_file(path):
    with open(path) as f:
        src = f.read()
    out, changed = reformat_text(src)
    if changed:
        with open(path, 'w') as f:
            f.write(out)
    return changed


def main(argv):
    for path in argv[1:]:
        if process_file(path):
            print('reformatted: {}'.format(path))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
