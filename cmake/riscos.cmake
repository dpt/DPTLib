# riscos.cmake
#
# CMake toolchain file for RISC OS GCCSDK
#
# Copyright (c) David Thomas, 2021-2026
#
# vim: sw=4 ts=8 et
#

# We need GCCSDK_INSTALL_CROSSBIN for the toolchain and GCCSDK_INSTALL_ENV
# for libraries like OSLib.
if(NOT DEFINED ENV{GCCSDK_INSTALL_CROSSBIN})
    message(FATAL_ERROR "GCCSDK_INSTALL_CROSSBIN is not set")
endif()
if(NOT DEFINED ENV{GCCSDK_INSTALL_ENV})
    message(FATAL_ERROR "GCCSDK_INSTALL_ENV is not set")
endif()

set(crossbin $ENV{GCCSDK_INSTALL_CROSSBIN})
set(prefix arm-unknown-riscos-)
set(CMAKE_ASM_ASASM_COMPILER ${crossbin}/asasm)
set(CMAKE_CXX_COMPILER ${crossbin}/${prefix}g++)
set(CMAKE_C_COMPILER ${crossbin}/${prefix}gcc)

set(CMAKE_ASM_ASASM_FLAGS "" CACHE INTERNAL "") # INTERNAL implies FORCE
set(CMAKE_C_FLAGS "-mlibscl -mhard-float" CACHE INTERNAL "")

# Don't embed function names in MinSizeRel builds, but do everywhere else.
if(CMAKE_BUILD_TYPE MATCHES MinSizeRel)
    add_compile_options($<$<NOT:$<COMPILE_LANGUAGE:ASM_ASASM>>:-mno-poke-function-name>)
    add_compile_options($<$<NOT:$<COMPILE_LANGUAGE:ASM_ASASM>>:-O1>)  # Dodge MinSizeRel -Os compiler crash
else()
    add_compile_options($<$<NOT:$<COMPILE_LANGUAGE:ASM_ASASM>>:-mpoke-function-name>)
endif()

# I'd like to set CMAKE_EXECUTABLE_SUFFIX to ,ff8 here but
# CMakeGenericSystem.cmake splats it.

set(CMAKE_FIND_ROOT_PATH ${crossbin})

# Search for programs only in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers only in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(TARGET_RISCOS 1)

# Define a macro that we can call once project() has been called.
macro(riscos_set_flags)
    # Overwrite the default of '-O3 -DNDEBUG'.
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "" FORCE)

    # See above.
    set(CMAKE_EXECUTABLE_SUFFIX ,ff8)

    # Set a CPU architecture target.
    # For now, divide assembly targets into ancient or modern ARM with the
    # pivot being ARMv5.
    option(ARMV5 "RISC OS: Target ARMv5" OFF)
    if(ARMV5)
        message(STATUS "RISC OS: Building for ARMv5")
        string(APPEND CMAKE_C_FLAGS " -march=armv5 -mtune=xscale")
        string(APPEND CMAKE_ASM_ASASM_FLAGS " -cpu XScale -fpu FPA10")
    else()
        message(STATUS "RISC OS: Building for ARMv2")
        string(APPEND CMAKE_C_FLAGS " -march=armv2 -mtune=arm2")
        string(APPEND CMAKE_ASM_ASASM_FLAGS " -cpu ARM2 -fpu FPA10")
    endif()
endmacro()
