# Generic Makefile for DPTLib
#
# by dpt

# Examples of use
#
# Build for OS X using clang, in release mode:
# make TARGET=osx-any-clang MODE=release build
#
# Build for OS X using clang, in debug mode, building in parallel:
# make TARGET=osx-any-clang -j build
#
# Build for iOS using clang in analyze mode:
# make TARGET=ios-arm-clang analyze
#
# Build using gcc:
# make build

# Project
#
PROJECT=DPTLib
LIBS=

# Paths
#
ROOT:=$(abspath $(dir $(lastword $(MAKEFILE_LIST)))../..)
PUBLIC_INCLUDES=$(ROOT)/include
LIBRARIES=$(ROOT)/libraries
PLATFORM_DIR=$(ROOT)/platform/generic
APPS_DIR=$(ROOT)/apps
LIB=$(PROJECT).a
TESTAPP=$(PROJECT)Test

# Tools (choices)
#
# Select a compiler based on TARGET
#
# Target is a quad consisting of:
#   <OS>-<CPU architecture>-<compiler>
# Where 'any' can be given as a wildcard element.
#
ifeq ($(TARGET),ios-arm-clang)
	DEVROOT=/Applications/Xcode.app/Contents/Developer
	SDK=$(DEVROOT)/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS8.0.sdk
	SDKBIN=$(DEVROOT)/Toolchains/XcodeDefault.xctoolchain/usr/bin
	CLANG=$(SDKBIN)/clang
	CROSSFLAGS=-isysroot $(SDK) -Os -arch armv7 -mthumb
	SANITIZE=
	CLANGFLAGS=-ferror-limit=2
	CC=$(CLANG) $(CLANGFLAGS) $(SANITIZE)
	LD=$(CLANG) $(SANITIZE)
	AR=$(SDKBIN)/libtool
	ARFLAGS=-static -syslibroot $(SDK) -o
else ifeq ($(TARGET),osx-any-clang)
	#CLANG=~/bin/clang+llvm-3.4/bin/clang
	CLANG=clang
	CLANGFLAGS=-ferror-limit=2
	#SANITIZE=-fsanitize=integer -fsanitize=address -fsanitize=undefined
	CC=$(CLANG) $(CLANGFLAGS) $(SANITIZE)
	LD=$(CLANG) $(SANITIZE)
	AR=libtool
	ARFLAGS=-static -o
else ifeq ($(TARGET),linux-any-clang)
	CLANG=clang
	CLANGFLAGS=-ferror-limit=2
	SANITIZE=
	CC=$(CLANG) $(CLANGFLAGS) $(SANITIZE)
	LD=$(CLANG) $(SANITIZE)
	# TODO: AR
else ifeq ($(TARGET),linux-arm-any)
	#CROSSFLAGS=-target arm-none-eabi -mcpu=arm7tdmi -mfloat-abi=soft
	# armv5-none-eabi -fpu=hard
	CC=gcc
	LD=gcc
else ifeq ($(TARGET),linux-arm-gcc)
	GCC=arm-linux-gnueabi-gcc-4.7
	CROSSFLAGS=-mcpu=arm7tdmi -mthumb -mfloat-abi=soft
	CC=$(GCC)
	LD=$(GCC)
	# TODO: AR
else
	CC=gcc
	LD=gcc
	AR=ar
	ARFLAGS=rc
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	CTAGS=/usr/local/bin/ctags
else
	CTAGS=ctags
endif

# Tools (config)
#
INCLUDES=-I$(PUBLIC_INCLUDES) -I$(LIBRARIES) -I$(PLATFORM_DIR)
WARNINGS=-Wall -Wextra -Wno-unused-parameter

# Tools (generic)
#
CFLAGS=-std=c99 $(WARNINGS) -MMD -MP $(INCLUDES) $(CROSSFLAGS)
LDFLAGS=$(CROSSFLAGS)
LDLIBS=$(LIBS)

ifeq ($(MODE),release)
  CFLAGS+=-Os -DNDEBUG
else
  CFLAGS+=-g
endif

# http://stackoverflow.com/a/6145814/306867
FILTER_OUT = $(foreach v,$(2),$(if $(findstring $(1),$(v)),,$(v)))

LIBSRC=$(shell find $(LIBRARIES) $(PLATFORM_DIR) -type f -name '*.c' -print)

SRC=$(call FILTER_OUT,test, $(LIBSRC))
OBJ=$(SRC:.c=.o)
DEP=$(SRC:.c=.d)

TESTSRC=$(filter-out $(SRC), $(LIBSRC))
TESTOBJ=$(TESTSRC:.c=.o)
TESTDEP=$(TESTSRC:.c=.d)

TESTAPPSRC=$(shell find $(APPS_DIR)/test -type f -name '*.c' -print)
TESTAPPOBJ=$(TESTAPPSRC:.c=.o)
TESTAPPDEP=$(TESTAPPSRC:.c=.d)

.c.o:;	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: usage
usage:
	@echo 'Usage:'
	@echo '  build		Build'
	@echo '  clean		Clean a previous build'
	@echo '  analyze	Perform a clang analyze run'
	@echo '  lint		Perform a lint run'
	@echo '  tags		Generate tags'
	@echo '  cscope		Generate cscope database'
	@echo '  astyle	Run astyle on the source'
	@echo '  docs		Generate docs'
	@echo
	@echo 'MODE=<release|debug>'

.PHONY: build
build: lib test

.PHONY:	lib
lib:	$(LIB)

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

.PHONY:	test
test:	$(TESTAPP)

$(TESTAPP): $(TESTAPPOBJ) $(TESTOBJ) $(LIB)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: analyze
analyze:
	$(CLANG) --analyze $(CFLAGS) -pedantic $(SRC)

.PHONY: lint
lint:
	splint -f $(ROOT)/.splintrc $(INCLUDES) $(SRC)

.PHONY: tags
tags:
	$(CTAGS) -R --sort=yes --c++-kinds=+p --fields=+iaS --extra=+q -f $(ROOT)/tags $(ROOT)

.PHONY: cscope
cscope:
	cscope -b -f $(ROOT)/cscope.out -R -s $(ROOT)

.PHONY: astyle
astyle:
	astyle --options=$(ROOT)/.astylerc --recursive "$(ROOT)/*.h" "$(ROOT)/*.c"

.PHONY: docs
docs:
	doxygen Doxyfile

.PHONY: clean
clean:
	@echo 'Cleaning...'
	-@rm -vf $(OBJ)
	-@rm -vf $(DEP)
	-@rm -vf $(LIB)
	-@rm -vf $(TESTOBJ)
	-@rm -vf $(TESTDEP)
	-@rm -vf $(TESTAPPOBJ)
	-@rm -vf $(TESTAPPDEP)
	-@rm -vf $(TESTAPP)
	-@rm -vf $(ROOT)/tags
	-@rm -vrf $(ROOT)/doxydocs

-include $(DEP)
-include $(TESTDEP)
-include $(TESTAPPDEP)

