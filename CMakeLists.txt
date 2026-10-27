# CMakeLists.txt
#
# DPTLib
#
# Copyright (c) David Thomas, 2020-2026
#
# vim: sw=4 ts=8 et

cmake_minimum_required(VERSION 3.23.2)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/)

project(DPTLib VERSION 0.6.0 DESCRIPTION "DPT's C Library" LANGUAGES C)

# The values set in the toolchain file aren't available until this point.
if(TARGET_RISCOS)
    riscos_set_flags()
endif()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

option(USE_FORTIFY "Use Fortify" OFF)
option(USE_ASAN "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(USE_COVERAGE "Build with Clang source-based code coverage" OFF)
option(DPTLIB_IMAGES_READ_ONLY "Remove libpng write support" OFF)

# Referencing CMAKE_TOOLCHAIN_FILE avoids a warning on rebuilds.
if(NOT ${CMAKE_TOOLCHAIN_FILE} STREQUAL "")
    message(STATUS "DPTLib: Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

add_subdirectory(libraries/fortify)

add_library(DPTLib)

set_target_properties(DPTLib PROPERTIES
    VERSION 0.6.0
    DESCRIPTION "DPT's Portable C Library"
    C_STANDARD 99
    PREFIX ""  # remove 'lib' prefix
    ARCHIVE_OUTPUT_NAME_DEBUG DPTLib-debug
    ARCHIVE_OUTPUT_NAME_RELEASE DPTLib
    ARCHIVE_OUTPUT_NAME_RELWITHDEBINFO DPTLib-relwithdebinfo
    ARCHIVE_OUTPUT_NAME_MINSIZEREL DPTLib-minsizerel)

target_include_directories(DPTLib PUBLIC include)

set(PUBLIC_HEADERS
    include/base/debug.h
    include/base/result.h
    include/base/types.h
    include/base/utils.h
    include/databases/digest-db.h
    include/databases/filename-db.h
    include/databases/pickle-reader-hash.h
    include/databases/pickle-writer-hash.h
    include/databases/pickle.h
    include/databases/tag-db.h
    include/datastruct/atom.h
    include/datastruct/bitarr.h
    include/datastruct/bitfifo.h
    include/datastruct/bitvec.h
    include/datastruct/cache.h
    include/datastruct/hash.h
    include/datastruct/hlist.h
    include/datastruct/list.h
    include/datastruct/ntree.h
    include/datastruct/vector.h
    include/framebuf/bitmap-set.h
    include/framebuf/bitmap.h
    include/framebuf/bmfont.h
    include/framebuf/bmfontcache.h
    include/framebuf/bmfontfamily.h
    include/framebuf/colour.h
    include/framebuf/colourspace.h
    include/framebuf/composite.h
    include/framebuf/curve.h
    include/framebuf/dither.h
    include/framebuf/effects.h
    include/framebuf/octree.h
    include/framebuf/palettes.h
    include/framebuf/pattern.h
    include/framebuf/tonemap.h
    include/framebuf/pixelfmt.h
    include/framebuf/pixelmap.h
    include/framebuf/screen.h
    include/framebuf/span-bgra8888.h
    include/framebuf/span-bgrx8888.h
    include/framebuf/span-p1.h
    include/framebuf/span-p2.h
    include/framebuf/span-p4.h
    include/framebuf/span-p8.h
    include/framebuf/span-registry.h
    include/framebuf/span-rgb565.h
    include/framebuf/span-rgba8888.h
    include/framebuf/span-rgbx5551.h
    include/framebuf/span-rgbx8888.h
    include/framebuf/span-xbgr8888.h
    include/framebuf/span.h
    include/geom/box.h
    include/geom/inset.h
    include/geom/layout.h
    include/geom/line.h
    include/geom/packer.h
    include/geom/point.h
    include/geom/region.h
    include/geom/size.h
    include/geom/stage.h
    include/geom/stack.h
    include/io/dirlist.h
    include/io/dirscan.h
    include/io/filetype.h
    include/io/namelist.h
    include/io/path.h
    include/io/stream-mem.h
    include/io/stream-mtfcomp.h
    include/io/stream-packbits.h
    include/io/stream-stdio.h
    include/io/stream.h
    include/test/all-tests.h
    include/test/tmpdir.h
    include/test/txtscr.h
    include/text/bmtext.h
    include/text/txtfmt.h
    include/text/utf8.h
    include/utils/alloc.h
    include/utils/array.h
    include/utils/barith.h
    include/utils/bsearch.h
    include/utils/bytesex.h
    include/utils/easing.h
    include/utils/fxp.h
    include/utils/maths.h
    include/utils/pack.h
    include/utils/primes.h
    include/utils/rng.h)

# The public headers must be set as properties of the library, not as
# target_sources. The quoting is essential.
set_target_properties(DPTLib PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}")

set(BASE_SOURCES
    libraries/base/result/result-string.c)

set(DATABASE_SOURCES
    libraries/databases/digest-db/digest-db.c
    libraries/databases/filename-db/filename-db.c
    libraries/databases/pickle/delete.c
    libraries/databases/pickle/hash-reader.c
    libraries/databases/pickle/hash-writer.c
    libraries/databases/pickle/pickle.c
    libraries/databases/pickle/unpickle.c
    libraries/databases/tag-db/tag-db.c)

set(DATASTRUCT_SOURCES
    libraries/datastruct/atom/create.c
    libraries/datastruct/atom/delete-block.c
    libraries/datastruct/atom/delete.c
    libraries/datastruct/atom/destroy.c
    libraries/datastruct/atom/for-block.c
    libraries/datastruct/atom/get.c
    libraries/datastruct/atom/impl.h
    libraries/datastruct/atom/new.c
    libraries/datastruct/atom/set.c
    libraries/datastruct/bitarr/count.c
    libraries/datastruct/bitfifo/bitfifo.c
    libraries/datastruct/bitvec/and.c
    libraries/datastruct/bitvec/clear-all.c
    libraries/datastruct/bitvec/clear.c
    libraries/datastruct/bitvec/count.c
    libraries/datastruct/bitvec/create.c
    libraries/datastruct/bitvec/destroy.c
    libraries/datastruct/bitvec/ensure.c
    libraries/datastruct/bitvec/eq.c
    libraries/datastruct/bitvec/get.c
    libraries/datastruct/bitvec/impl.h
    libraries/datastruct/bitvec/length.c
    libraries/datastruct/bitvec/next.c
    libraries/datastruct/bitvec/or.c
    libraries/datastruct/bitvec/set-all.c
    libraries/datastruct/bitvec/set.c
    libraries/datastruct/bitvec/toggle.c
    libraries/datastruct/cache/cache.c
    libraries/datastruct/hash/count.c
    libraries/datastruct/hash/create.c
    libraries/datastruct/hash/destroy.c
    libraries/datastruct/hash/impl.h
    libraries/framebuf/effects/impl.h
    libraries/datastruct/hash/insert.c
    libraries/datastruct/hash/lookup-node.c
    libraries/datastruct/hash/lookup.c
    libraries/datastruct/hash/remove.c
    libraries/datastruct/hash/walk-cont.c
    libraries/datastruct/hash/walk.c
    libraries/datastruct/hlist/append.c
    libraries/datastruct/hlist/copy.c
    libraries/datastruct/hlist/free.c
    libraries/datastruct/hlist/impl.h
    libraries/datastruct/hlist/length.c
    libraries/datastruct/hlist/list.c
    libraries/datastruct/hlist/map.c
    libraries/datastruct/hlist/pop.c
    libraries/datastruct/hlist/push.c
    libraries/datastruct/hlist/reverse.c
    libraries/datastruct/hlist/to-array.c
    libraries/datastruct/list/add-head.c
    libraries/datastruct/list/add-tail.c
    libraries/datastruct/list/find.c
    libraries/datastruct/list/init.c
    libraries/datastruct/list/remove.c
    libraries/datastruct/list/walk.c
    libraries/datastruct/ntree/append.c
    libraries/datastruct/ntree/copy.c
    libraries/datastruct/ntree/delete.c
    libraries/datastruct/ntree/depth.c
    libraries/datastruct/ntree/first-child.c
    libraries/datastruct/ntree/free.c
    libraries/datastruct/ntree/get-data.c
    libraries/datastruct/ntree/impl.h
    libraries/datastruct/ntree/insert-after.c
    libraries/datastruct/ntree/insert-before.c
    libraries/datastruct/ntree/insert.c
    libraries/datastruct/ntree/last-child.c
    libraries/datastruct/ntree/max-height.c
    libraries/datastruct/ntree/n-nodes.c
    libraries/datastruct/ntree/new.c
    libraries/datastruct/ntree/next-sibling.c
    libraries/datastruct/ntree/nth-child.c
    libraries/datastruct/ntree/parent.c
    libraries/datastruct/ntree/prepend.c
    libraries/datastruct/ntree/prev-sibling.c
    libraries/datastruct/ntree/set-data.c
    libraries/datastruct/ntree/unlink.c
    libraries/datastruct/ntree/walk.c
    libraries/datastruct/vector/clear.c
    libraries/datastruct/vector/create.c
    libraries/datastruct/vector/destroy.c
    libraries/datastruct/vector/ensure.c
    libraries/datastruct/vector/get.c
    libraries/datastruct/vector/impl.h
    libraries/datastruct/vector/insert.c
    libraries/datastruct/vector/length.c
    libraries/datastruct/vector/set-length.c
    libraries/datastruct/vector/set-width.c
    libraries/datastruct/vector/set.c
    libraries/datastruct/vector/width.c)

set(FRAMEBUF_SOURCES
    libraries/framebuf/bitmap/bitmap.c
    libraries/framebuf/bitmap/compress.c
    libraries/framebuf/bitmap/fill-pattern.c
    libraries/framebuf/bitmap/flood-fill.c
    libraries/framebuf/bitmap/load.c
    libraries/framebuf/bitmap/resize.c
    libraries/framebuf/bitmap/rle.h
    libraries/framebuf/bitmap/save.c
    libraries/framebuf/bmfont/bmfont.c
    libraries/framebuf/bmfont/cache.c
    libraries/framebuf/bmfont/enumerate.c
    libraries/framebuf/bmfont/family.c
    libraries/framebuf/colour/colour.c
    libraries/framebuf/colourspace/colourspace.c
    libraries/framebuf/composite/composite.c
    libraries/framebuf/curve/curve.c
    libraries/framebuf/dither/dither.c
    libraries/framebuf/effects/blend.c
    libraries/framebuf/effects/blur.c
    libraries/framebuf/effects/clear.c
    libraries/framebuf/effects/convolve.c
    libraries/framebuf/effects/equalise.c
    libraries/framebuf/effects/expand.c
    libraries/framebuf/effects/grey.c
    libraries/framebuf/effects/layout.c
    libraries/framebuf/effects/remap.c
    libraries/framebuf/effects/sharpen.c
    libraries/framebuf/effects/tonemap.c
    libraries/framebuf/octree/octree.c
    libraries/framebuf/palettes/palettes.c
    libraries/framebuf/pattern/pattern.c
    libraries/framebuf/tonemap/plot.c
    libraries/framebuf/tonemap/tonemap.c
    libraries/framebuf/pixelfmt/log2bpp.c
    libraries/framebuf/pixelfmt/paletted-nentries.c
    libraries/framebuf/pixelmap/build.c
    libraries/framebuf/pixelmap/get.c
    libraries/framebuf/pixelmap/impl.h
    libraries/framebuf/screen/screen.c
    libraries/framebuf/screen/screen-copy-bitmap-rle.c
    libraries/framebuf/screen/screen-copy-bitmap-rle.h
    libraries/framebuf/screen/screen-copy-ninepatch.c
    libraries/framebuf/screen/screen-copy-tiled.c
    libraries/framebuf/screen/screen-copy-rect.c
    libraries/framebuf/screen/screen-draw.c
    libraries/framebuf/screen/screen-draw-circle.c
    libraries/framebuf/screen/screen-draw-ellipse.c
    libraries/framebuf/screen/screen-draw-thick-line.c
    libraries/framebuf/screen/screen-fill-checks.c
    libraries/framebuf/screen/screen-fill-hline.c
    libraries/framebuf/screen/screen-fill-pattern.c
    libraries/framebuf/span-registry/get.c
    libraries/framebuf/span-registry/regdata.h
    libraries/framebuf/span/all16bpp.c
    libraries/framebuf/span/all16bpp.h
    libraries/framebuf/span/all8888.c
    libraries/framebuf/span/all8888.h
    libraries/framebuf/span/bgra8888.c
    libraries/framebuf/span/bgrx8888.c
    libraries/framebuf/span/p1.c
    libraries/framebuf/span/p2.c
    libraries/framebuf/span/p4.c
    libraries/framebuf/span/p8.c
    libraries/framebuf/span/rgb565.c
    libraries/framebuf/span/rgbx5551.c
    libraries/framebuf/span/rgbx8888.c
    libraries/framebuf/span/xbgr8888.c)

set(GEOM_SOURCES
    libraries/geom/box/clipped.c
    libraries/geom/box/contains-box.c
    libraries/geom/box/contains-point.c
    libraries/geom/box/could-hold.c
    libraries/geom/box/extend.c
    libraries/geom/box/grow.c
    libraries/geom/box/intersection.c
    libraries/geom/box/intersects.c
    libraries/geom/box/is-empty.c
    libraries/geom/box/list-add.c
    libraries/geom/box/reset.c
    libraries/geom/box/round.c
    libraries/geom/box/round4.c
    libraries/geom/box/size.c
    libraries/geom/box/translated.c
    libraries/geom/box/union.c
    libraries/geom/layout/layout.c
    libraries/geom/line/line.c
    libraries/geom/stage/stage.c
    libraries/geom/packer/impl.h
    libraries/geom/packer/packer.c
    libraries/geom/region/impl.h
    libraries/geom/region/builder.c
    libraries/geom/region/invert.c
    libraries/geom/region/outline.c
    libraries/geom/region/polygon.c
    libraries/geom/region/region.c
    libraries/geom/stack/impl.h
    libraries/geom/stack/axis-size.c
    libraries/geom/stack/measure-min.c
    libraries/geom/stack/solve.c
    libraries/geom/stack/valid-tree.c)

set(IO_SOURCES
    libraries/io/dirlist/dirlist.c
    libraries/io/dirscan/dirscan.c
    libraries/io/filetype/filetype.c
    libraries/io/namelist/namelist.c
    libraries/io/path/path.c
    libraries/io/stream/stream-mem.c
    libraries/io/stream/stream-mtfcomp.c
    libraries/io/stream/stream-packbitscomp.c
    libraries/io/stream/stream-packbitsdecomp.c
    libraries/io/stream/stream-stdio.c
    libraries/io/stream/stream.c)

set(TESTLIB_SOURCES
    libraries/test/tmpdir/tmpdir.c
    libraries/test/txtscr/txtscr.c)

set(TEXT_SOURCES
    libraries/text/bmtext/draw.c
    libraries/text/bmtext/layout.c
    libraries/text/txtfmt/create.c
    libraries/text/txtfmt/destroy.c
    libraries/text/txtfmt/get-length.c
    libraries/text/txtfmt/get-line.c
    libraries/text/txtfmt/get-nlines.c
    libraries/text/txtfmt/get-wrapped-width.c
    libraries/text/txtfmt/impl.h
    libraries/text/txtfmt/print.c
    libraries/text/txtfmt/wrap.c
    libraries/text/utf8/decode.c
    libraries/text/utf8/encode.c
    libraries/text/utf8/prev.c)

set(UTILS_SOURCES
    libraries/utils/alloc/alloc.c
    libraries/utils/array/delelem.c
    libraries/utils/array/delelems.c
    libraries/utils/array/grow.c
    libraries/utils/array/shrink.c
    libraries/utils/array/squeeze.c
    libraries/utils/array/stretch.c
    libraries/utils/barith/barith.c
    libraries/utils/bsearch/bsearch-impl.h
    libraries/utils/bsearch/bsearch-int.c
    libraries/utils/bsearch/bsearch-short.c
    libraries/utils/bsearch/bsearch-uint.c
    libraries/utils/bsearch/bsearch-ushort.c
    libraries/utils/bytesex/bytesex.c
    libraries/utils/bytesex/rev-l-block.c
    libraries/utils/bytesex/rev-s-block.c
    libraries/utils/easing/easing.c
    libraries/utils/fxp/smull-fxp16.c
    libraries/utils/fxp/umull-fxp16.c
    libraries/utils/maths/degs-to-rads.c
    libraries/utils/maths/gcd.c
    libraries/utils/pack/pack.c
    libraries/utils/pack/unpack.c
    libraries/utils/primes/primes.c)

set(ALL_SOURCES
    ${PUBLIC_HEADERS}
    ${BASE_SOURCES}
    ${DATABASE_SOURCES}
    ${DATASTRUCT_SOURCES}
    ${FRAMEBUF_SOURCES}
    ${GEOM_SOURCES}
    ${IO_SOURCES}
    ${TESTLIB_SOURCES}
    ${TEXT_SOURCES}
    ${UTILS_SOURCES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ALL_SOURCES})

target_sources(DPTLib PRIVATE ${ALL_SOURCES})

if(MSVC)
    target_compile_options(DPTLib PRIVATE /W4)
    target_compile_definitions(DPTLib PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
else()
    target_compile_options(DPTLib PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(DPTLib PRIVATE -ffunction-sections -fdata-sections)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(DPTLib PRIVATE DPTLIB_DEBUG)
endif()

# Library dependencies
#

if(TARGET_RISCOS)
    # OSLib (& anything else in GCCSDK)
    target_include_directories(DPTLib PUBLIC $ENV{GCCSDK_INSTALL_ENV}/include)
    target_link_libraries(DPTLib PUBLIC $ENV{GCCSDK_INSTALL_ENV}/lib/libOSLib32.a)
    target_compile_definitions(DPTLib PUBLIC TARGET_RISCOS)
endif()

if(USE_FORTIFY)
    target_compile_definitions(DPTLib PRIVATE FORTIFY)
    target_link_libraries(DPTLib PUBLIC Fortify)
endif()

if(USE_ASAN)
    target_compile_options(DPTLib PUBLIC -fsanitize=address,undefined -g -O0)
    target_link_options(DPTLib PUBLIC -fsanitize=address,undefined)
endif()

if(USE_COVERAGE)
    # Clang only: run tools/coverage.sh to build, test and report
    target_compile_options(DPTLib PUBLIC -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(DPTLib PUBLIC -fprofile-instr-generate)
endif()

if(DPTLIB_IMAGES_READ_ONLY)
    target_compile_definitions(DPTLib PRIVATE DPTLIB_IMAGES_READ_ONLY)
endif()

if(EMSCRIPTEN)
    # Emscripten's bundled libpng port (pulls in its zlib port); provides the
    # headers at compile time and the archive at link time.
    target_compile_options(DPTLib PUBLIC "-sUSE_LIBPNG=1")
    target_link_options(DPTLib PUBLIC "-sUSE_LIBPNG=1")
elseif(NOT TARGET_RISCOS)
    set(CMAKE_FIND_FRAMEWORK NEVER)
    find_package(PNG REQUIRED MODULE)
    target_link_libraries(DPTLib PRIVATE PNG::PNG)
else()
    ## On RISC OS we must build libpng & zlib
    include(FetchContent)

    FetchContent_Declare(zlib
                         GIT_REPOSITORY https://github.com/madler/zlib.git
                         GIT_TAG v1.2.11)
    FetchContent_Declare(libpng
                         GIT_REPOSITORY https://github.com/glennrp/libpng.git
                         GIT_TAG v1.6.46)

    FetchContent_GetProperties(zlib)
    if(NOT zlib_POPULATED)
        message(STATUS "zlib: fetching and patching...")

        FetchContent_Populate(zlib)

        # Patch CMake minimum to 3.0.0
        execute_process(COMMAND           ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_LIST_DIR}/cmake/zlib.patch
                        WORKING_DIRECTORY ${zlib_SOURCE_DIR}
                        OUTPUT_QUIET
                        ERROR_QUIET) # because it'll error upon re-run

        # https://stackoverflow.com/questions/37582508/silence-cmp0048-warnings-in-vendored-projects
        set(CMAKE_PROJECT_INCLUDE_BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/enable_cmp0048.cmake")
        add_subdirectory(${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} EXCLUDE_FROM_ALL)
        unset(CMAKE_PROJECT_INCLUDE_BEFORE)

        # Set vars that libpng's CMakeLists will use to get zlib (as FindZLIB would set)
        set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
        set(ZLIB_LIBRARY ${zlib_BINARY_DIR}/libz.a)
    endif()

    FetchContent_GetProperties(libpng)
    if(NOT libpng_POPULATED)
        message(STATUS "libpng: fetching and patching...")

        FetchContent_Populate(libpng)

        # Patch libm find_library problem
        execute_process(COMMAND           ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_LIST_DIR}/cmake/libpng-libm-unix.patch
                        WORKING_DIRECTORY ${libpng_SOURCE_DIR}
                        OUTPUT_QUIET
                        ERROR_QUIET) # because it'll error upon re-run

        cmake_policy(SET CMP0077 NEW) # option() honors normal variables

        set(PNG_SHARED OFF CACHE BOOL "" FORCE)
        set(PNG_STATIC ON CACHE BOOL "" FORCE)
        set(PNG_TOOLS OFF CACHE BOOL "" FORCE) # we only want lib
        set(PNG_TESTS OFF CACHE BOOL "" FORCE) # we only want lib
        set(SKIP_INSTALL_ALL OFF CACHE BOOL "" FORCE) # we only want lib

        add_subdirectory(${libpng_SOURCE_DIR} ${libpng_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    if(DPTLIB_IMAGES_READ_ONLY)
        message(STATUS "libpng: configuring for read only")
        set(PNGUSR pngusr-ro.dfa)
    else()
        message(STATUS "libpng: configuring for read-write")
        set(PNGUSR pngusr-rw.dfa)
    endif()
    set(DFA_XTRA ${libpng_BINARY_DIR}/pngusr-dptlib.dfa CACHE FILEPATH "File containing extra configuration settings" FORCE)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PNGUSR} ${DFA_XTRA} COPYONLY)

    include_directories(SYSTEM ${libpng_SOURCE_DIR}/ ${libpng_BINARY_DIR}/)
    include_directories(SYSTEM ${zlib_SOURCE_DIR}/)

    target_include_directories(DPTLib PUBLIC ${libpng_SOURCE_DIR}/ ${libpng_BINARY_DIR}/)
    target_link_libraries(DPTLib PUBLIC zlibstatic png_static)
endif()

# Installation
#

# Set install location
# - We can't use set(...) with FORCE because that will override any passed-in
#   configuration values.
# - We can't use set(...) without FORCE because that won't override the default
#   value...
# - So, just check for the default value and override it if set.
if(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr/local")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install" CACHE PATH "CMake install prefix" FORCE)
endif()

# Install headers and library
# - We have to use EXCLUDE_FROM_ALL to stop the PUBLIC_HEADERs getting
#   installed by install(TARGETS ...).
install(TARGETS DPTLib ARCHIVE
    PUBLIC_HEADER EXCLUDE_FROM_ALL)
# - We can't use install(TARGETS ... PUBLIC_HEADER) to install headers, as
#   expected, since that flattens the include directory's hierarchy. Use
#   install(DIRECTORY ...) instead.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    FILES_MATCHING PATTERN "*.h")


# Self-test executable
#

option(BUILD_TESTS "Build test program" OFF)

# Only offered at top level: a parent project's cache should not carry it
if(PROJECT_IS_TOP_LEVEL)
    option(BUILD_SDL_TESTS "Build tests that use SDL" OFF)
endif()

# Only when DPTLib is the top-level project, so a parent project's own
# BUILD_TESTS does not also build DPTLibTest
if(BUILD_TESTS AND PROJECT_IS_TOP_LEVEL)
    set(TEST_SOURCES
        apps/test/main.c
        libraries/databases/tag-db/test/tag-db-test.c
        libraries/databases/pickle/test/pickle-test.c
        libraries/datastruct/atom/test/atom-test.c
        libraries/datastruct/bitarr/test/bitarr-test.c
        libraries/datastruct/bitfifo/test/bitfifo-test.c
        libraries/datastruct/bitvec/test/bitvec-test.c
        libraries/datastruct/cache/test/cache-test.c
        libraries/datastruct/hash/test/hash-test.c
        libraries/datastruct/list/test/list-test.c
        libraries/datastruct/ntree/test/ntree-test.c
        libraries/framebuf/bitmap/test/load-test.c
        libraries/framebuf/bitmap/test/resize-test.c
        libraries/framebuf/bitmap/test/rle-test.c
        libraries/framebuf/bitmap/test/convert-test.c
        libraries/framebuf/span/test/span-test.c
        libraries/framebuf/bmfont/test/bmfont-test.c
        libraries/framebuf/bmfont/test/bmfontcache-test.c
        libraries/framebuf/bmfont/test/bmfontfamily-test.c
        libraries/framebuf/colour/test/colour-test.c
        libraries/framebuf/colourspace/test/colourspace-test.c
        libraries/framebuf/composite/test/composite-test.c
        libraries/framebuf/curve/test/curve-test.c
        libraries/framebuf/dither/test/dither-test.c
        libraries/framebuf/effects/test/effects-test.c
        libraries/framebuf/octree/test/octree-test.c
        libraries/framebuf/pattern/test/pattern-test.c
        libraries/framebuf/pixelmap/test/pixelmap-test.c
        libraries/framebuf/tonemap/test/tonemap-test.c
        libraries/framebuf/screen/test/screen-test.c
        libraries/geom/box/test/box-test.c
        libraries/geom/layout/test/layout-test.c
        libraries/geom/stage/test/stage-test.c
        libraries/geom/packer/test/packer-test.c
        libraries/geom/region/test/region-test.c
        libraries/geom/stack/test/stack-test.c
        libraries/io/dirlist/test/dirlist-test.c
        libraries/io/filetype/test/filetype-test.c
        libraries/io/path/test/path-test.c
        libraries/io/stream/test/stream-test.c
        libraries/text/bmtext/test/bmtext-test.c
        libraries/text/txtfmt/test/txtfmt-test.c
        libraries/text/utf8/test/utf8-test.c
        libraries/utils/array/test/array-test.c
        libraries/utils/barith/test/barith-test.c
        libraries/utils/bsearch/test/bsearch-test.c
        libraries/utils/bytesex/test/bytesex-test.c
        libraries/utils/easing/test/easing-test.c
        libraries/utils/pack/test/pack-test.c
        libraries/datastruct/vector/test/vector-test.c)

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${TEST_SOURCES})

    # Avoid a warning from CMake
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

    add_executable(DPTLibTest ${TEST_SOURCES})

    set_target_properties(DPTLibTest PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DESCRIPTION "DPTLib test program"
        C_STANDARD 99
        OUTPUT_NAME_DEBUG DPTLibTest-debug
        OUTPUT_NAME_RELEASE DPTLibTest
        OUTPUT_NAME_RELWITHDEBINFO DPTLibTest-relwithdebinfo
        OUTPUT_NAME_MINSIZEREL DPTLibTest-minsizerel)

    # DPTTest dependencies
    # Of course, DPTLibTest depends on DPTLib.
    target_link_libraries(DPTLibTest DPTLib)
    if(NOT MSVC)
        target_link_libraries(DPTLibTest m)
    endif()

    # Some unit tests (e.g. bmfont-test) gate an interactive SDL mode on
    # USE_SDL; BUILD_SDL_TESTS links SDL3 and defines it for the test binary.
    if(BUILD_SDL_TESTS)
        find_package(SDL3 REQUIRED)
        find_package(SDL3_image REQUIRED)

        target_link_libraries(DPTLibTest SDL3::SDL3 SDL3_image::SDL3_image)
        target_compile_definitions(DPTLibTest PRIVATE USE_SDL)
    endif()

    install(TARGETS DPTLibTest RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
