#!/usr/bin/env python3
"""wrap_protos.py -- rewrap C function prototypes/definitions that exceed a
column limit onto one parameter per line, with the parameter names aligned
in a column (the style already used across DPTLib, e.g. screen-copy-rect.c):

    static int screen_copy_rect_p4(screen_t    *scr,
                                   const box_t *s,
                                   int          width)

Scope:
  - Rewraps a header/definition whose parameter list ends in ")" then an
    optional ";" or "{". Handles both a single overlong physical line and an
    already-wrapped multi-line prototype (the list is joined, then re-split
    and realigned) -- so re-running is idempotent and fixes bad alignment.
  - Splits the parameter list on top-level commas only, so function-pointer
    params and (T)(args) casts survive the split.
  - Aligns the declarator (name, or the "(*name)" of a function pointer)
    into one column; "*"s are pulled against the name. Unnamed params align
    on their trailing token. void / empty / variadic-only lists are left
    as-is (nothing to align).
  - Only rewrites when the original spans >1 line OR exceeds the width.
"""
import argparse
import re
import sys

# Match "<ret> <name>(" ... ")" <tail>, across newlines. Non-greedy head stops
# at the first "(", the args group is everything up to the final ")".
#
# The head is deliberately strict -- "<type words / * / space> <name>(" with
# nothing else -- so a bare call statement like "foo(a, b);" or "p->fn(x)"
# never matches. It still needs the is_prototype_head() checks below (>=2
# space-separated words, a plausible return type, real param syntax).
DECL_RE = re.compile(
    r'^(?P<indent>[ \t]*)'
    r'(?P<head>[A-Za-z_]\w*(?:[ \t]+(?:[A-Za-z_]\w*|\*+))*[ \t]*'
    r'\*?[ \t]*[A-Za-z_]\w*\()'
    r'(?P<args>.*?)'
    r'\)[ \t]*(?P<tail>[;{]?)[ \t]*$',
    re.DOTALL)
# statement-start guard: the previous non-blank line must end like this for the
# candidate to be at the start of a statement (not mid-expression / mid-call).
STMT_END_RE = re.compile(r'(?:[;{}]|\*/|\)|,|^\s*#.*|^\s*/[/*].*)\s*$')
QUALIFIERS = {'static', 'extern', 'inline', '_Noreturn', 'auto', 'register',
              '__inline', '__inline__', '__forceinline'}
# C keywords that can't appear in a function decl/def head -- their presence
# means the line is a statement (return/if/while/...), not a prototype.
STMT_KW = {'return', 'if', 'else', 'while', 'for', 'do', 'switch', 'case',
           'default', 'goto', 'break', 'continue', 'sizeof', 'typedef',
           'typeof', '__typeof__', 'defined'}
IDENT_TAIL_RE = re.compile(r'([A-Za-z_]\w*)\s*$')
# C type keywords -- a param made only of these (+ * and whitespace) is unnamed
TYPE_KW = {'void', 'char', 'short', 'int', 'long', 'float', 'double',
           'signed', 'unsigned', 'const', 'volatile', 'struct', 'union',
           'enum', '_Bool', 'size_t', 'ssize_t', 'ptrdiff_t', 'wchar_t'}
# function pointer / array-of-fn-pointer param: "TYPE (*name)(...)" / "(*name[])"
FNPTR_RE = re.compile(r'^(?P<ret>.*?)\(\s*\*\s*(?P<name>\w*)\s*(?P<arr>(?:\[\s*\w*\s*\])*)\)\s*(?P<rest>\(.*\))\s*$', re.DOTALL)


def is_prototype_head(indent, head, args):
    """Reject anything that isn't a real top-level function decl/def head.

    Guards against call statements ("foo(a,b);", "obj->method(x)"), macro
    invocations, and control-flow ("if (...)"). Conservative: a false
    negative just means a line is left unwrapped."""
    h = head[:-1].strip()                     # drop trailing "("
    if any(c in h for c in '->.=[]"\'+-/%<>&|!~?:'):
        return False
    words = h.split()
    if len(words) < 2:                        # need at least "<type> <name>"
        return False
    name = words[-1].lstrip('*')
    if not name.isidentifier() or name in QUALIFIERS:
        return False
    # every leading word is a type-ish token (keyword, identifier, or stars)
    # and none is a statement keyword
    for w in words:
        bare = w.strip('*')
        if bare in STMT_KW:
            return False
    for w in words[:-1]:
        if w.strip('*') and not (w.strip('*').isidentifier()):
            return False
    # args must look like a parameter list: empty, "void", or comma-separated
    # items that each contain a type token (an identifier or a "*").
    a = args.strip()
    if a in ('', 'void'):
        return True
    for part in split_top_level(a):
        p = part.strip()
        if not p:
            return False
        if p == '...':
            continue
        if not re.search(r'[A-Za-z_]', p):   # no type at all -> not a param
            return False
        if p[0] in '"\'' or '=' in p:        # string arg / default -> a call
            return False
    return True


def split_top_level(args):
    """Split on commas not nested in () or []."""
    parts, depth, start = [], 0, 0
    for i, c in enumerate(args):
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
        elif c == ',' and depth == 0:
            parts.append(args[start:i])
            start = i + 1
    parts.append(args[start:])
    return [' '.join(p.split()) for p in parts]


def parse_param(param):
    """Return (lhs, stars, decl) so that "lhs" + pad + stars + decl reflows the
    param with "decl" (the name, or "(*name)(...)") in an aligned column.

    Returns None for void / "..." (nothing meaningful to align)."""
    p = param.strip()
    if p in ('', 'void', '...'):
        return None

    m = FNPTR_RE.match(p)
    if m:
        ret = m.group('ret').rstrip()
        stars = ''
        while ret.endswith('*'):
            stars += '*'
            ret = ret[:-1].rstrip()
        decl = f"(*{m.group('name')}{m.group('arr')}){m.group('rest')}"
        return ret, stars, decl

    if '(' in p:                       # some other parenthesised form -- leave whole
        return p, '', ''

    # peel a trailing array suffix ("[]", "[N]", "[N][M]") off the declarator
    arr = ''
    mt = re.search(r'((?:\[[^\]]*\])+)\s*$', p)
    if mt:
        arr = mt.group(1)
        p = p[:mt.start()].rstrip()

    bare = {w for w in p.replace('*', ' ').split()}
    m = IDENT_TAIL_RE.search(p)
    if m and p[:m.start()].strip() and not bare <= TYPE_KW:  # "TYPE name"
        head = p[:m.start()].rstrip()
        name = m.group(1)
    else:                             # unnamed: "TYPE" / "TYPE *"
        head, name = p, ''
    stars = ''
    while head.endswith('*'):
        stars += '*'
        head = head[:-1].rstrip()
    return head, stars, name + arr


def reflow_parts(indent, head, args, tail):
    params = split_top_level(args.strip())
    if len(params) == 1 and params[0].strip() in ('', 'void'):
        return None

    parsed = [parse_param(p) for p in params]
    if any(p is None for p in parsed):    # void mixed in, or "..." -- bail
        return None

    # DPTLib style (see screen-copy-rect.c, cache.c): the "*" sits immediately
    # before the declarator, declarators line up in one column, and a "**"
    # param's extra star hangs one place to the left. So the name column is
    # one past the longest "type + space + stars".
    name_col = max(len(l) + 1 + len(s) for l, s, _ in parsed)
    cont = ' ' * len(indent + head)
    close = ') ' + tail if tail == '{' else ')' + tail

    out = []
    for i, (lhs, stars, decl) in enumerate(parsed):
        # right-align "stars + decl" so decl starts at name_col
        tail_txt = stars + decl
        piece = (lhs + ' ' * (name_col - len(stars) - len(lhs)) + tail_txt).rstrip()
        prefix = (indent + head) if i == 0 else cont
        sep = ',' if i < len(parsed) - 1 else close
        out.append(prefix + piece + sep)
    return '\n'.join(out) + '\n'


def _balanced(s):
    """True if () and [] are balanced and never go negative in s."""
    depth = 0
    for c in s:
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
            if depth < 0:
                return False
    return depth == 0


def _strip_comments(s):
    """Blank out /* ... */ and // ... content so scanning ignores it.
    Assumes s starts outside a comment."""
    out = []
    k = 0
    while k < len(s):
        if s[k:k+2] == '//':
            break
        if s[k:k+2] == '/*':
            end = s.find('*/', k + 2)
            if end < 0:
                return ''.join(out), True     # unterminated -> rest is comment
            out.append('  ')
            out.append(' ' * (end + 2 - k - 2))
            k = end + 2
            continue
        out.append(s[k])
        k += 1
    return ''.join(out), False


def process(text, width):
    out = []
    changed = False
    lines = text.splitlines(keepends=True)
    prev_nonblank = ''                        # last source line already emitted
    in_comment = False
    i = 0
    while i < len(lines):
        if in_comment:
            out.append(lines[i])
            if '*/' in lines[i]:
                in_comment = False
                rest, cont = _strip_comments(lines[i].split('*/', 1)[1])
                in_comment = cont
            i += 1
            continue
        _, opened = _strip_comments(lines[i])
        if opened:                           # this line opens a block comment
            out.append(lines[i])
            in_comment = True
            i += 1
            continue
        # a comment-continuation line (" * ...") never starts a prototype
        if lines[i].lstrip().startswith('*'):
            out.append(lines[i])
            i += 1
            continue
        # Grow a candidate span up to the first line that closes the parens and
        # ends in ) / ); / {. Stop early if a line ends in ; or { without
        # balancing -- that means this was never a prototype.
        j = i
        span = ''
        m = None
        while j < len(lines) and j - i < 40:
            span += lines[j]
            body = span.rstrip()
            if _balanced(body) and body.endswith((')', ');', ') {', '){')):
                m = DECL_RE.match(body)
                break
            if body.endswith((';', '{', '}')) and _balanced(body):
                break                        # closed, but not as a prototype
            j += 1

        ok = False
        if m:
            at_stmt_start = (prev_nonblank == ''
                             or STMT_END_RE.search(prev_nonblank))
            head_first = m.group('head').split()[0]
            # a definition head may sit on its own line after "static\n" etc.
            ok = (at_stmt_start
                  and is_prototype_head(m.group('indent'), m.group('head'),
                                        m.group('args')))
            # reject macro-style ALL-CAPS "return type"
            if head_first.isupper() and head_first not in TYPE_KW:
                ok = False

        if ok:
            one_line = (m.group('indent') + m.group('head')
                        + ', '.join(split_top_level(m.group('args').strip()))
                        + ')' + (' ' + m.group('tail') if m.group('tail') == '{'
                                 else m.group('tail')))
            if len(one_line) > width:
                new = reflow_parts(m.group('indent'), m.group('head'),
                                   m.group('args'), m.group('tail'))
                if new is not None and new != span:
                    out.append(new)
                    changed = True
                    prev_nonblank = new.rstrip('\n').rsplit('\n', 1)[-1]
                    i = j + 1
                    continue

        out.append(lines[i])
        if lines[i].strip():
            prev_nonblank = lines[i].rstrip('\n')
        i += 1
    return ''.join(out), changed


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='+')
    ap.add_argument('-w', '--width', type=int, default=77)
    ap.add_argument('-n', '--dry-run', action='store_true',
                    help='report files that would change, write nothing')
    args = ap.parse_args()

    rc = 0
    for path in args.files:
        with open(path) as f:
            text = f.read()
        new, changed = process(text, args.width)
        if not changed:
            continue
        if args.dry_run:
            print(f'would rewrap: {path}')
            rc = 1
        else:
            with open(path, 'w') as f:
                f.write(new)
            print(f'rewrapped: {path}')
    return rc


if __name__ == '__main__':
    sys.exit(main())
