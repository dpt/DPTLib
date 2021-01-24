# CMakeLists.txt
#
# DPTLib
#
# Copyright (c) David Thomas, 2020-2021
#
# vim: sw=4 ts=8 et

# TODO
# - Fortify

cmake_minimum_required(VERSION 3.18)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules/)

project(DPTLib VERSION 0.2.7 DESCRIPTION "DPT's C Library" LANGUAGES C)

# The values set in the toolchain file aren't available until this point.

if(TARGET_RISCOS AND NOT DEFINED ENV{GCCSDK_INSTALL_ENV})
    message(FATAL_ERROR "GCCSDK_INSTALL_ENV is not set")
endif()

if(TARGET_RISCOS)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "" FORCE) # overwrite default of '-O3 -NDEBUG'

    # I tried to get this to live in riscos.cmake but was defeated.
    set(CMAKE_EXECUTABLE_SUFFIX ,ff8)

    # For now, divide assembly targets into ancient or modern ARM (pivot=ARMv5).
    option(ARMV5 "Utilise ARMv5" OFF)
    if(ARMV5)
        string(APPEND CMAKE_C_FLAGS " -march=armv5 -mtune=xscale")
    else()
        string(APPEND CMAKE_C_FLAGS " -march=armv2 -mtune=arm2")
    endif()
endif()

# If this is enabled then the final binary will need to be linked against
# Fortify too.
option(USE_FORTIFY "Use Fortify" OFF)

# Referencing CMAKE_TOOLCHAIN_FILE avoids a warning on rebuilds.
if(NOT ${CMAKE_TOOLCHAIN_FILE} STREQUAL "")
    message(STATUS "DPTLib: Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

set(LIBRARY ${PROJECT_NAME})

add_library(${LIBRARY})

set_target_properties(${LIBRARY} PROPERTIES
    VERSION 0.2.7
    DESCRIPTION "DPT's Portable C Library"
    C_STANDARD 99
    PREFIX ""  # remove 'lib' prefix
    ARCHIVE_OUTPUT_NAME_DEBUG ${LIBRARY}-debug
    ARCHIVE_OUTPUT_NAME_RELEASE ${LIBRARY}
    ARCHIVE_OUTPUT_NAME_RELWITHDEBINFO ${LIBRARY}-relwithdebinfo
    ARCHIVE_OUTPUT_NAME_MINSIZEREL ${LIBRARY}-minsizerel)

target_include_directories(${LIBRARY} PUBLIC include)

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
    include/framebuf/composite.h
    include/framebuf/pixelfmt.h
    include/framebuf/screen.h
    include/geom/box.h
    include/geom/layout.h
    include/geom/packer.h
    include/geom/point.h
    include/io/stream-mem.h
    include/io/stream-mtfcomp.h
    include/io/stream-packbits.h
    include/io/stream-stdio.h
    include/io/stream.h
    include/test/txtscr.h
    include/utils/array.h
    include/utils/barith.h
    include/utils/bsearch.h
    include/utils/bytesex.h
    include/utils/fxp.h
    include/utils/maths.h
    include/utils/pack.h
    include/utils/primes.h)

# The public headers must be set as properties of the library, not as
# target_sources. The quoting is essential.
set_target_properties(${LIBRARY} PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}")

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
    libraries/datastruct/list/find.c
    libraries/datastruct/list/init.c
    libraries/datastruct/list/remove.c
    libraries/datastruct/list/walk.c
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
    libraries/framebuf/composite/composite.c
    libraries/framebuf/pixelfmt/log2bpp.c)

set(GEOM_SOURCES
    libraries/geom/box/contains-box.c
    libraries/geom/box/contains-point.c
    libraries/geom/box/could-hold.c
    libraries/geom/box/grow.c
    libraries/geom/box/intersection.c
    libraries/geom/box/intersects.c
    libraries/geom/box/is-empty.c
    libraries/geom/box/reset.c
    libraries/geom/box/round.c
    libraries/geom/box/round4.c
    libraries/geom/box/union.c
    libraries/geom/layout/layout.c
    libraries/geom/packer/impl.h
    libraries/geom/packer/packer.c)

set(IO_SOURCES
    libraries/io/stream/stream-mem.c
    libraries/io/stream/stream-mtfcomp.c
    libraries/io/stream/stream-packbitscomp.c
    libraries/io/stream/stream-packbitsdecomp.c
    libraries/io/stream/stream-stdio.c
    libraries/io/stream/stream.c)

set(TEST_SOURCES
    libraries/test/txtscr/txtscr.c)

set(UTILS_SOURCES
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
    libraries/utils/bytesex/rev-l-block.c
    libraries/utils/bytesex/rev-l-m.c
    libraries/utils/bytesex/rev-l.c
    libraries/utils/bytesex/rev-s-block.c
    libraries/utils/bytesex/rev-s-m.c
    libraries/utils/bytesex/rev-s-pair-m.c
    libraries/utils/bytesex/rev-s-pair.c
    libraries/utils/bytesex/rev-s.c
    libraries/utils/bytesex/util.h
    libraries/utils/fxp/smull-fxp16.c
    libraries/utils/fxp/umull-fxp16.c
    libraries/utils/maths/degs-to-rads.c
    libraries/utils/maths/gcd.c
    libraries/utils/pack/pack.c
    libraries/utils/pack/unpack.c
    libraries/utils/primes/primes.c)

set(ALL_SOURCES
    ${DATABASE_SOURCES}
    ${DATASTRUCT_SOURCES}
    ${FRAMEBUF_SOURCES}
    ${GEOM_SOURCES}
    ${IO_SOURCES}
    ${TEST_SOURCES}
    ${UTILS_SOURCES})

target_sources(${LIBRARY} PRIVATE ${ALL_SOURCES})

target_compile_options(${LIBRARY} PRIVATE -Wall -Wextra -pedantic)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(${LIBRARY} PRIVATE $<UPPER_CASE:${LIBRARY}>_DEBUG)
endif()

# Library dependencies
#

if(TARGET_RISCOS)
    # OSLib (& anything else in GCCSDK)
    target_include_directories(${LIBRARY} PUBLIC $ENV{GCCSDK_INSTALL_ENV}/include)
    target_link_libraries(${LIBRARY} PUBLIC $ENV{GCCSDK_INSTALL_ENV}/lib/libOSLib32.a)
endif()

if(USE_FORTIFY)
    target_compile_definitions(${LIBRARY} PUBLIC FORTIFY)
    target_link_libraries(${LIBRARY} PUBLIC Fortify)
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
    set(CMAKE_INSTALL_PREFIX "install" CACHE PATH "CMake install prefix" FORCE)
endif()

# Install headers and library
# - We have to use EXCLUDE_FROM_ALL to stop the PUBLIC_HEADERs getting
#   installed by install(TARGETS ...).
install(TARGETS ${LIBRARY} ARCHIVE
    PUBLIC_HEADER EXCLUDE_FROM_ALL)
# - We can't use install(TARGETS ... PUBLIC_HEADER) to install headers, as
#   expected, since that flattens the include directory's hierarchy. Use
#   install(DIRECTORY ...) instead.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    FILES_MATCHING PATTERN "*.h")


# Self-test executable
#

option(BUILD_TESTS "Build test program" OFF)

if(BUILD_TESTS)
    set(TEST ${PROJECT_NAME}Test)

    set(TEST_SOURCES
        apps/test/main.c
        libraries/databases/tag-db/test/tag-db-test.c
        libraries/databases/pickle/test/pickle-test.c
        libraries/io/stream/test/stream-test.c
        libraries/utils/bsearch/test/bsearch-test.c
        libraries/utils/array/test/array-test.c
        libraries/geom/packer/test/packer-test.c
        libraries/geom/layout/test/layout-test.c
        libraries/datastruct/bitfifo/test/bitfifo-test.c
        libraries/datastruct/bitvec/test/bitvec-test.c
        libraries/datastruct/cache/test/cache-test.c
        libraries/datastruct/atom/test/atom-test.c
        libraries/datastruct/ntree/test/ntree-test.c
        libraries/datastruct/hash/test/hash-test.c
        libraries/datastruct/list/test/list-test.c
        libraries/datastruct/bitarr/test/bitarr-test.c
        libraries/datastruct/vector/test/vector-test.c)

    # Avoid a warning from CMake
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

    add_executable(${TEST} ${TEST_SOURCES})

    set_target_properties(${TEST} PROPERTIES
        DESCRIPTION "DPTLib test program"
        C_STANDARD 99
        OUTPUT_NAME_DEBUG ${TEST}-debug
        OUTPUT_NAME_RELEASE ${TEST}
        OUTPUT_NAME_RELWITHDEBINFO ${TEST}-relwithdebinfo
        OUTPUT_NAME_MINSIZEREL ${TEST}-minsizerel)

    # DPTTest dependencies
    # Of course, DPTTest depends on DPTLib.
    target_link_libraries(${TEST} ${LIBRARY})

    install(TARGETS ${TEST} RUNTIME
        DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
