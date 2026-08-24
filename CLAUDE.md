# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

DPTLib is a platform-independent C99 library (base, databases, datastruct, framebuf, geom, io, text, utils, wuss modules) shared across the author's projects (PrivateEye, MotionMasks, etc). Targets desktop (Linux/macOS/Windows) and RISC OS (via GCCSDK).

## Build

```
mkdir build && cd build
cmake -DBUILD_TESTS=YES ..
make -j4
```

Useful CMake options:
- `BUILD_TESTS=YES` — build the `DPTLibTest` self-test executable.
- `BUILD_SDL_TESTS=YES` — additionally build tests needing SDL2/SDL2_image.
- `USE_FORTIFY=YES` — link the bundled Fortify memory-debugging library.
- `DPTLIB_IMAGES_READ_ONLY=YES` — build libpng without write support.

Requires libpng (`find_package(PNG)` on non-RISC OS). On RISC OS the build fetches and patches zlib/libpng itself via `FetchContent` (see `cmake/*.patch`).

## Testing

Run all tests (needs `-resources` pointing at the repo root, for test fixture files):
```
./build/DPTLibTest -resources /path/to/DPTLib
```

Run a subset by naming tests (names come from the `tests[]` table in `apps/test/main.c`, e.g. `atom`, `bitvec`, `curve`, `pickle`, `stream`, `packer`):
```
./build/DPTLibTest -resources /path/to/DPTLib atom bitvec
```

Success prints `++ Tests completed in Ns: N of N tests passed.`

### Adding a new test

1. Write `libraries/<area>/<module>/test/<module>-test.c` exposing a `result_t <module>_test(const char *resources)`.
2. Declare it as `extern testfn_t <module>_test;` in `include/test/all-tests.h` (grouped by area).
3. Add `{ "<module>", <module>_test }` to the `tests[]` table in `apps/test/main.c`.
4. Add the new `.c`/`.h` files to `TEST_SOURCES` in `CMakeLists.txt`.

## Architecture

**Module layout.** Each module lives in two places that must be kept in sync:
- `include/<area>/<module>.h` — the public API, always wrapped in `extern "C"`, documented with Doxygen `\file`/`\param`/`\return` comments.
- `libraries/<area>/<module>/` — implementation `.c` files (often one function per file, e.g. `libraries/datastruct/vector/{create,destroy,insert,...}.c`), plus a private `impl.h` defining the opaque struct behind the public typedef and any internal-only declarations.

New source/header files must be added by hand to the relevant `set(..._SOURCES ...)` list and, for public headers, to `PUBLIC_HEADERS`, in `CMakeLists.txt` — there is no globbing.

**Error handling.** No exceptions; functions return `result_t` (`include/base/result.h`). Each module reserves a `result_BASE_<MODULE>` offset block and defines its own `result_<MODULE>_*` codes starting from that base. Common generic codes (`result_OK`, `result_OOM`, `result_BAD_ARG`, etc.) live at `result_BASE_GENERIC`. Callers typically check `rc != result_OK` and propagate.

**Debug/logging.** `include/base/debug.h` provides `logf_info/warning/error/fatal/abort`, plus `check(err)` (log-and-`goto failure`) and `sentinel` (unreachable-code marker), both of which are compiled out entirely when `NDEBUG` is set — don't rely on their side effects in release builds.

**Test structure.** There is a single test binary (`DPTLibTest`, from `apps/test/main.c`) that dispatches to per-module `<module>_test(resources)` functions declared centrally in `include/test/all-tests.h`. Tests return `result_TEST_PASSED`/`result_TEST_FAILED` (`result_BASE_TEST`).

**Notable sub-libraries with their own docs** (`docs/*.md`): `io/stream` (chainable byte-stream sources/transforms, PackBits and move-to-front compression), `framebuf/bmfont` (proportional bitmap fonts loaded from specially-formatted PNGs), `framebuf/composite` (Porter-Duff compositing on RGBA/BGRA bitmaps).

**RISC OS.** `TARGET_RISCOS` (set by an external toolchain file) switches CMake to fetch/build zlib+libpng from source, link OSLib from `$GCCSDK_INSTALL_ENV`, and applies `riscos_set_flags()`. `pngusr-ro.dfa`/`pngusr-rw.dfa` select libpng's build config depending on `DPTLIB_IMAGES_READ_ONLY`.

## Code style

- C99, `-Wall -Wextra -pedantic`. Allman brace style, 2-space indentation, tabs converted to spaces (see `.astylerc`/`.editorconfig` — astyle is the formatter of record).
- Declare variables at the top of each scope (pre-C99 style is followed throughout, even though the standard is C99), ordered by first use.
- File header comment format: `/* filename.c -- one-line description */`.
- Section breaks within files use `/* ----- ... ----- */` rule comments.
- Public API docs use Doxygen (`\file`, `\param`, `\return`); a `Doxyfile` exists for generating them.

## Commit messages

Use [Conventional Commits](https://www.conventionalcommits.org/): `<type>[optional scope]: <description>`, e.g. `fix(pickle): handle zero-length blobs`. Common types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `ci`, `build`. Add a `!` before the colon (or a `BREAKING CHANGE:` footer) for breaking changes.
