#!/usr/bin/env python3
"""wrap_doxygen.py -- rewrap Doxygen /** ... */ block comments to a column
limit, greedy-fill, continuation lines aligned under the description text
(matching the \\param/\\return style already used across DPTLib headers).

Scope (ponytail: lazy on purpose):
  - Rewraps multi-line /** ... */ blocks: \\param/\\return/\\brief/\\file/etc
    tag lines, and plain prose lines, each as one reflow-able paragraph.
  - Promotes an overlong single-line /** ... */ comment into a multi-line
    block.
  - Leaves /**< ... */ trailing member comments and non-comment code lines
    alone even if too long -- fixing those means moving the comment above
    the declaration and touching struct/enum layout, a structural call this
    script doesn't make. Still reported so a human can look.
"""
import argparse
import re
import sys
import textwrap

OPEN_RE = re.compile(r'^(\s*)/\*\*\s*$')
CLOSE_RE = re.compile(r'^(\s*)\*/\s*$')
BLANK_RE = re.compile(r'^\s*\*\s*$')
SINGLE_LINE_RE = re.compile(r'^(\s*)/\*\*\s+(.*\S)\s+\*/\s*$')
PARAM_RE = re.compile(r'^(\s*\*\s*\\param(?:\[[^\]]*\])?\s+\S+\s+)(.*)$')
TAG_RE = re.compile(
    r'^(\s*\*\s*\\(?:return|brief|file|note|warning|pre|post|throws?)\b\s*)(.*)$')
PROSE_RE = re.compile(r'^(\s*\*\s+)(\S.*)$')


def cont_prefix(prefix1, indent):
    return indent + '*' + ' ' * (len(prefix1) - len(indent) - 1)


def fill(prefix1, words, width, indent):
    text = ' '.join(w for w in words if w)
    cont = cont_prefix(prefix1, indent)
    lines = textwrap.wrap(text, width=width, initial_indent=prefix1,
                           subsequent_indent=cont, break_long_words=False,
                           break_on_hyphens=False)
    return lines or [prefix1.rstrip()]


def wrap_single_line_comment(indent, text, width):
    body_width = width - len(indent) - 3  # " * " prefix
    lines = textwrap.wrap(text, width=body_width, break_long_words=False,
                           break_on_hyphens=False)
    out = [indent + '/**']
    out += [indent + ' * ' + l for l in lines]
    out.append(indent + ' */')
    return out


def process(lines, width):
    out = []
    overflow = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i].rstrip('\n')

        m = SINGLE_LINE_RE.match(line)
        if m and '/**<' not in line and len(line) > width:
            indent, text = m.groups()
            out.extend(wrap_single_line_comment(indent, text, width))
            i += 1
            continue

        m = OPEN_RE.match(line)
        if m:
            out.append(line)
            indent = m.group(1) + ' '  # body lines' "*" sits one col right of "/**"
            i += 1
            pending = None  # (prefix1, [words...])

            def flush():
                if pending is not None:
                    out.extend(fill(pending[0], pending[1], width, indent))

            while i < n:
                body = lines[i].rstrip('\n')

                mclose = CLOSE_RE.match(body)
                if mclose:
                    flush()
                    pending = None
                    out.append(body)
                    i += 1
                    break

                if BLANK_RE.match(body):
                    flush()
                    pending = None
                    out.append(body)
                    i += 1
                    continue

                mtag = PARAM_RE.match(body) or TAG_RE.match(body)
                if mtag:
                    flush()
                    pending = (mtag.group(1), [mtag.group(2)])
                    i += 1
                    continue

                mprose = PROSE_RE.match(body)
                if mprose:
                    if pending is None:
                        pending = (mprose.group(1), [mprose.group(2)])
                    else:
                        pending[1].append(mprose.group(2))
                    i += 1
                    continue

                # Doesn't look like a normal comment-body line -- pass through.
                flush()
                pending = None
                out.append(body)
                i += 1
            else:
                flush()
            continue

        if len(line) > width:
            overflow.append((i + 1, line))
        out.append(line)
        i += 1

    return out, overflow


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('files', nargs='+')
    ap.add_argument('--width', type=int, default=80)
    ap.add_argument('--check', action='store_true',
                     help="don't write; exit 1 if any file would change")
    args = ap.parse_args()

    changed = False
    for path in args.files:
        with open(path, encoding='utf-8') as f:
            original = f.readlines()
        new_lines, overflow = process(original, args.width)
        new_text = '\n'.join(new_lines) + '\n'
        old_text = ''.join(original)

        if new_text != old_text:
            changed = True
            if args.check:
                print(f'{path}: would reformat')
            else:
                with open(path, 'w', encoding='utf-8') as f:
                    f.write(new_text)
                print(f'{path}: reformatted')
        for lineno, text in overflow:
            print(f'{path}:{lineno}: still over {args.width} cols '
                  f'(needs manual restructuring): {text.strip()}',
                  file=sys.stderr)

    if args.check and changed:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
