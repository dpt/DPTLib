#!/usr/bin/env python3
# gen_filetypes.py -- bake RISC OS Open's file type allocations into filetype.c
#
# Reads the "File Types" page of the RISC OS Open wiki and rewrites the two
# generated tables in libraries/io/filetype/filetype.c, between the
# "BEGIN GENERATED" and "END GENERATED" marker comments:
#
#   g_types[64]     bitmap of allocated numbers, bit (n & 63) of word n >> 6
#   g_names[64]     per bitmap word, the names of its set bits in ascending
#                   order as one NUL-separated literal
#   g_exts          every extension, undotted and NUL-separated, each type's
#                   own in turn with its canonical one first
#   g_ext_types[]   the type each g_exts entry maps to
#
# No per-type pointers are stored: a name is found by skipping as many names
# in its word's block as there are set bits below it.
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

NWORDS = 4096 // 64  # filetype numbers are 12 bits

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

    # bitmap of allocated numbers, one bit per number
    words = [0] * NWORDS
    for number in types:
        words[number >> 6] |= 1 << (number & 63)

    lines = ['static const uint64_t g_types[%d] =' % NWORDS, '{']
    for w in range(0, NWORDS, 3):
        lines.append('  ' + ' '.join('UINT64_C(0x%016X),' % x
                                     for x in words[w:w + 3]))
    lines[-1] = lines[-1].rstrip(',')
    lines += ['};', '']

    # each block's names, NUL-separated in ascending order of number; split
    # into one literal per name so a "\0" never runs into a following digit
    lines += ['static const char *const g_names[%d] =' % NWORDS, '{']
    for w in range(NWORDS):
        numbers = [n for n in sorted(types) if n >> 6 == w]
        if not numbers:
            lines.append('  %-30s /* %03X-%03X */'
                         % ('"",', w << 6, (w << 6) + 63))
            continue
        for n in numbers:
            literal = c_string(types[n][1])[:-1] + '\\0"'
            if n == numbers[-1] and w != NWORDS - 1:
                literal += ','
            lines.append('  %-30s /* %03X */' % (literal, n))
    lines += ['};', '']

    # extensions: each type's own, canonical first, ascending by type; then
    # the canonical extension of each type that owns none, which only the
    # type-to-extension lookup can reach, since from_ext stops at the owner
    pool = []
    for number in sorted(types):
        for ext in types[number][0]:
            if owners[ext][1] == number:
                pool.append((ext, number))
    for number in sorted(types):
        exts = types[number][0]
        if exts and all(owners[ext][1] != number for ext in exts):
            pool.append((exts[0], number))

    lines += ['static const char g_exts[] =']
    for ext, _ in pool:
        lines.append('  ' + c_string(ext + '\0').replace('\0', '\\0'))
    lines[-1] += ';'
    lines += ['', 'static const unsigned short g_ext_types[%d] =' % len(pool),
              '{']
    for p in range(0, len(pool), 10):
        lines.append('  ' + ' '.join('0x%03X,' % n for _, n in pool[p:p + 10]))
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
