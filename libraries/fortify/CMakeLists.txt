# CMakeLists.txt
#
# DPTLib
#
# Copyright (c) David Thomas, 2020-2021
#
# vim: sw=4 ts=8 et

add_library(Fortify fortify.c)
target_compile_definitions(Fortify PRIVATE FORTIFY)
target_include_directories(Fortify PUBLIC ../../include/)
