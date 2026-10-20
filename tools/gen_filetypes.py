#!/usr/bin/env python3
# gen_filetypes.py -- bake RISC OS Open's file type allocations into filetype.c
#
# Reads the "File Types" page of the RISC OS Open wiki and rewrites the two
# generated tables in libraries/io/filetype/filetype.c, between the
# "BEGIN GENERATED" and "END GENERATED" marker comments:
#
#   g_filetypes[]   { number, canonical extension, name }, ascending by number
#   g_extensions[]  { extension, number }, every extension listed, ascending
#
# Rows marked unregistered or "do not use" are skipped. Where several rows
# share a number, the first OS-defined row wins, then the first registered
# one, then the first of unknown status. An extension listed by several types
# goes to the type that lists it first, then by the same status order.
#
# Usage: tools/gen_filetypes.py [saved-page.html]
#        (with no argument the page is fetched)

import html
import re
import sys
import unicodedata
import urllib.request

URL = 'https://www.riscosopen.org/wiki/documentation/source/File%20Types'
TARGET = 'libraries/io/filetype/filetype.c'

STATUS_RANK = {'✔': 0, '✓': 1, '': 2}  # OS-defined, registered, unknown

# Types the wiki does not list but the library needs: Markdown has no
# allocation yet, so it borrows a number from the user range (see
# filetype_RISCOS_MARKDOWN).
EXTRA = [(0x1D0, ['md'], 'Markdown', 0)]


def ascii_only(s):
    return unicodedata.normalize('NFKD', s).encode('ascii', 'ignore').decode()


def unlink(s):
    """[[target|text]] or [[text]] -> text"""
    return re.sub(r'\[\[(?:[^|\]]*\|)?([^\]]*)\]\]', r'\1', s)


def c_string(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def rows(page):
    for raw in re.findall(r'<span>(\|.*?\|)</span>', page):
        row = unlink(html.unescape(raw).replace('\xa0', ' '))
        cells = [c.strip() for c in row.split('|')[1:-1]]
        if len(cells) != 6 or not re.fullmatch(r'[0-9A-Fa-f]{3}', cells[1]):
            continue
        status, number, name, _, _, mime = cells
        if status not in STATUS_RANK:
            continue  # unregistered or do not use
        exts = []
        for tok in mime.split():
            tok = tok.lstrip('.').lower()
            if '/' in tok or not re.fullmatch(r'[a-z0-9_]+', tok):
                continue  # a MIME type, or a range like "z1-z8"
            if tok not in exts:
                exts.append(tok)
        name = ascii_only(name).rstrip('? ')  # "?" marks a guessed name
        yield int(number, 16), exts, name, STATUS_RANK[status]


def main():
    if len(sys.argv) > 1:
        page = open(sys.argv[1], encoding='utf-8').read()
    else:
        page = urllib.request.urlopen(URL).read().decode('utf-8')

    all_rows = list(rows(page)) + EXTRA
    if len(all_rows) < 500:
        sys.exit('gen_filetypes: only %d rows parsed; page layout changed?'
                 % len(all_rows))

    types = {}
    for order, (number, exts, name, rank) in enumerate(all_rows):
        best = types.get(number)
        if best is None or rank < best[3]:
            types[number] = (exts, name, order, rank)

    owners = {}
    for number, (exts, name, order, rank) in types.items():
        for pos, ext in enumerate(exts):
            key = (pos, rank, order)
            if ext not in owners or key < owners[ext][0]:
                owners[ext] = (key, number)

    lines = ['static const filetype_t g_filetypes[] =', '{']
    for number in sorted(types):
        exts, name, _, _ = types[number]
        ext = c_string('.' + exts[0] if exts else '') + ','
        lines.append('  { 0x%03X, %-8s %s },' % (number, ext, c_string(name)))
    lines[-1] = lines[-1].rstrip(',')
    lines += ['};', '', 'static const filetype_ext_t g_extensions[] =', '{']
    for ext in sorted(owners):
        lines.append('  { %-13s 0x%03X },'
                     % (c_string('.' + ext) + ',', owners[ext][1]))
    lines[-1] = lines[-1].rstrip(',')
    lines.append('};')

    src = open(TARGET, encoding='utf-8').read()
    begin = src.index('/* BEGIN GENERATED')
    begin = src.index('*/\n', begin) + 3
    end = src.index('/* END GENERATED')
    open(TARGET, 'w', encoding='utf-8').write(
        src[:begin] + '\n'.join(lines) + '\n' + src[end:])

    print('gen_filetypes: %d types, %d extensions' % (len(types), len(owners)))


if __name__ == '__main__':
    main()
