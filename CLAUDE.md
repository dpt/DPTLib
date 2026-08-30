# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

DPTLib is a platform-independent C99 library (base, databases, datastruct, framebuf, geom, io, text, utils, wuss modules) shared across the author's projects (PrivateEye, MotionMasks, etc). Targets desktop (Linux/macOS/Windows) and RISC OS (via GCCSDK).

## Build

Build directories are per-config and already created; there is **no** plain
`build/` — never invoke `./build/DPTLibTest` or `cmake --build build`.

- `build-asan/`  — **default**. Debug, core tests (`BUILD_TESTS=YES`). Use this
  unless told otherwise. Despite the name its cache currently has
  `USE_ASAN=OFF`; re-run cmake with `-DUSE_ASAN=YES` if you actually need the
  sanitisers.
- `build-sdl/`   — Release, SDL tests on (`BUILD_SDL_TESTS=ON`). Needed for the
  interactive `wuss` driver and anything under `libraries/wuss/test/tasks/`.
- `build-nosdl/` — Release, core tests only.
- `build-riscos/`— GCCSDK cross build. Leave alone unless working on RISC OS.
- `build.xc/`    — Xcode generator.

CMake options: `BUILD_TESTS`, `BUILD_SDL_TESTS`, `USE_ASAN` (ASan + UBSan),
`USE_FORTIFY` (bundled Fortify), `DPTLIB_IMAGES_READ_ONLY` (libpng no write).

(Re)configure a dir only if its `CMakeCache.txt` is missing or you are changing
options:
```
cmake -B build-asan -G Ninja -DBUILD_TESTS=YES
```

Build:
```
cmake --build build-asan --target DPTLibTest
```

Requires libpng (`find_package(PNG)` on non-RISC OS). On RISC OS the build fetches and patches zlib/libpng itself via `FetchContent` (see `cmake/*.patch`).

## Testing

Run from the **repo root** so `-resources .` resolves the fixture files:
```
./build-asan/DPTLibTest -resources .
```

Run a subset by naming tests (names come from the `tests[]` table in `apps/test/main.c`, e.g. `atom`, `bitvec`, `curve`, `pickle`, `stream`, `packer`, `wuss`):
```
./build-asan/DPTLibTest -resources . atom bitvec
```

SDL / interactive tests use the `build-sdl` binary instead.

Success prints `++ Tests completed in Ns: N of N tests passed.`

### Adding a new test

1. Write `libraries/<area>/<module>/test/<module>-test.c` exposing a `result_t <module>_test(const char *resources)`.
2. Declare it as `extern testfn_t <module>_test;` in `include/test/all-tests.h` (grouped by area).
3. Add `{ "<module>", <module>_test }` to the `tests[]` table in `apps/test/main.c`.
4. Add the new `.c`/`.h` files to `TEST_SOURCES` in `CMakeLists.txt`.

## Architecture

**Module layout.** Each module is split between `include/<area>/<module>.h` (public API, `extern "C"`, Doxygen-documented) and `libraries/<area>/<module>/` (implementation `.c` files, often one function per file, plus a private `impl.h` for the opaque struct and internal-only declarations).

New source/header files must be added by hand to the relevant `set(..._SOURCES ...)` list and, for public headers, to `PUBLIC_HEADERS`, in `CMakeLists.txt` — there is no globbing.

**Internal naming (see `wuss`).** Functions declared in a private `impl.h` and not part of the public header use a double-underscore prefix, e.g. `wuss__titlebar_height_for`; internal-only enum constants still use the module's normal single-underscore style (e.g. `wuss_WINDOW_STATE_TOGGLED`), matching public enums. Per-instance internal state that isn't part of the public appearance API (e.g. `struct wuss_window`'s toggled/maximised state) is kept as a bitflags enum with `wuss__window_*` accessor helpers rather than loose `int`/`bool` fields, leaving room to add flags without growing the struct.

**Error handling.** No exceptions; functions return `result_t` (`include/base/result.h`). Each module reserves a `result_BASE_<MODULE>` offset block and defines its own `result_<MODULE>_*` codes starting from that base. Common generic codes (`result_OK`, `result_OOM`, `result_BAD_ARG`, etc.) live at `result_BASE_GENERIC`. Callers typically check `rc != result_OK` and propagate.

**Debug/logging.** `include/base/debug.h` provides `logf_info/warning/error/fatal/abort`, plus `check(err)` (log-and-`goto failure`) and `sentinel` (unreachable-code marker), both of which are compiled out entirely when `NDEBUG` is set — don't rely on their side effects in release builds.

**Test structure.** There is a single test binary (`DPTLibTest`, from `apps/test/main.c`) that dispatches to per-module `<module>_test(resources)` functions declared centrally in `include/test/all-tests.h`. Tests return `result_TEST_PASSED`/`result_TEST_FAILED` (`result_BASE_TEST`).

**Notable sub-libraries with their own docs** (`docs/*.md`): `io/stream` (chainable byte-stream sources/transforms, PackBits and move-to-front compression), `framebuf/bmfont` (proportional bitmap fonts loaded from specially-formatted PNGs), `framebuf/composite` (Porter-Duff compositing on RGBA/BGRA bitmaps), `wuss` (minimal window manager: creation, z-ordering, mouse routing, dirty-region redraw, client-delegated content).

**RISC OS.** `TARGET_RISCOS` (set by an external toolchain file) switches CMake to fetch/build zlib+libpng from source, link OSLib from `$GCCSDK_INSTALL_ENV`, and applies `riscos_set_flags()`. `pngusr-ro.dfa`/`pngusr-rw.dfa` select libpng's build config depending on `DPTLIB_IMAGES_READ_ONLY`.

## Code style

- C99, `-Wall -Wextra -pedantic`. Allman brace style, 2-space indentation, tabs converted to spaces (see `.astylerc`/`.editorconfig` — astyle is the formatter of record).
- Declare variables at the top of each scope (pre-C99 style is followed throughout, even though the standard is C99), ordered by first use.
- File header comment format: `/* filename.c -- one-line description */`.
- Section breaks within files use `/* ----- ... ----- */` rule comments.
- Public API docs use Doxygen (`\file`, `\param`, `\return`); a `Doxyfile` exists for generating them.
- Edit `.c`/`.h` files with the Edit tool, never `sed -i` line-range splices — they corrupt the Allman/2-space layout and can't be verified without re-reading. To inspect exact bytes or indentation, Read the file; don't shell out to `cat -A`/`cat -v`.
- After editing any `.c`/`.h` function prototype or definition, run `python3 tools/wrap_protos.py <file>`; after editing a header's Doxygen, run `python3 tools/wrap_doxygen.py <file>` (skip vendored headers).

## Commit messages

Use [Conventional Commits](https://www.conventionalcommits.org/): `<type>[optional scope]: <description>`, e.g. `fix(pickle): handle zero-length blobs`. Common types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `ci`, `build`. Add a `!` before the colon (or a `BREAKING CHANGE:` footer) for breaking changes.

When the user says "commit": stage the relevant files and `git commit` with the
message passed via repeated `-m` flags (subject, then body). Do **not** write a
`COMMIT_MSG` / `COMMIT_MSG_TMP` file. Never `git push` unless explicitly asked —
pushing prompts for an SSH key passphrase and will hang.
