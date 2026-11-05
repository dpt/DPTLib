# CMakeLists.txt
#
# DPTLibCRT: CRT post-effect for SDL3 windows.
#
# A project of its own so that it can be used without configuring the rest of
# DPTLib, which needs libpng. Either set DPTLIB_CRT in a project which adds
# DPTLib or fetch this alone:
#   FetchContent_Declare(dptlib_crt
#                        GIT_REPOSITORY https://github.com/dpt/DPTLib.git
#                        GIT_TAG <commit>
#                        SOURCE_SUBDIR libraries/sdl/crt)
#
# Copyright (c) David Thomas, 2026
#
# vim: sw=4 ts=8 et

cmake_minimum_required(VERSION 3.20)

project(DPTLibCRT LANGUAGES C)

option(DPTLIB_CRT_VULKAN "Use Vulkan off Apple platforms, if glslangValidator is found" ON)

if(EMSCRIPTEN)
    add_library(DPTLibCRT STATIC crt-gles.c)
    target_compile_definitions(DPTLibCRT PUBLIC DPTLIB_CRT_AVAILABLE)
    target_compile_options(DPTLibCRT PUBLIC "-sUSE_SDL=3")
    target_link_options(DPTLibCRT PUBLIC "-sUSE_SDL=3")
else()
    add_library(DPTLibCRT STATIC crt-gpu.c)

    if(NOT TARGET SDL3::SDL3)
        find_package(SDL3 REQUIRED)
    endif()
    target_link_libraries(DPTLibCRT PUBLIC SDL3::SDL3)

    if(APPLE)
        target_compile_definitions(DPTLibCRT PUBLIC DPTLIB_CRT_AVAILABLE)
    elseif(DPTLIB_CRT_VULKAN AND UNIX)
        # The Vulkan backend wants SPIR-V: compile the GLSL at build time.
        find_program(GLSLANG_VALIDATOR glslangValidator)
        if(GLSLANG_VALIDATOR)
            set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
            set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
            set(EMBED ${CMAKE_CURRENT_SOURCE_DIR}/EmbedSPIRV.cmake)
            file(MAKE_DIRECTORY ${GEN_DIR})
            add_custom_command(
                OUTPUT ${GEN_DIR}/crt-spirv.h
                COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_DIR}/crt.vert -o ${GEN_DIR}/crt.vert.spv
                COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_DIR}/crt.frag -o ${GEN_DIR}/crt.frag.spv
                COMMAND ${CMAKE_COMMAND} -DINPUT=${GEN_DIR}/crt.vert.spv -DOUTPUT=${GEN_DIR}/crt-vert.h -DVARNAME=crt_vertex_spirv -P ${EMBED}
                COMMAND ${CMAKE_COMMAND} -DINPUT=${GEN_DIR}/crt.frag.spv -DOUTPUT=${GEN_DIR}/crt-frag.h -DVARNAME=crt_fragment_spirv -P ${EMBED}
                COMMAND ${CMAKE_COMMAND} -E cat ${GEN_DIR}/crt-vert.h ${GEN_DIR}/crt-frag.h > ${GEN_DIR}/crt-spirv.h
                DEPENDS ${SHADER_DIR}/crt.vert ${SHADER_DIR}/crt.frag ${EMBED}
                VERBATIM)
            target_sources(DPTLibCRT PRIVATE ${GEN_DIR}/crt-spirv.h)
            target_include_directories(DPTLibCRT PRIVATE ${GEN_DIR})
            target_compile_definitions(DPTLibCRT PRIVATE DPTLIB_CRT_VULKAN)
            target_compile_definitions(DPTLibCRT PUBLIC DPTLIB_CRT_AVAILABLE)
        else()
            message(STATUS "glslangValidator not found - no CRT effect on this platform")
        endif()
    endif()
endif()

set_target_properties(DPTLibCRT PROPERTIES C_STANDARD 99)

target_include_directories(DPTLibCRT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
