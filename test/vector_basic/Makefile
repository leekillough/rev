#
# Makefile vector-basic
#
# Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

# Project root
PROJHOME ?= $(realpath ../../)

# SST
ifdef VALGRIND
 SST ?= valgrind sst
else
 SST = sst
endif

# SST configuration
SSTCFG ?= $(PROJHOME)/test/rev-model-options-config.py

# REV paths
REVLIBPATH ?= $(PROJHOME)/build/src
REVPRINT ?= $(PROJHOME)/scripts/rev-print.py

# Test source code
SRCDIR = ./src

# SST Libs
SSTOPTS += --add-lib-path=$(REVLIBPATH)

# Output Directories
OUTDIR = output

# Test Selection
ALL_TESTS += $(notdir $(basename $(wildcard $(SRCDIR)/*.cc)))
# User can specify test list at command line
TLIST ?= $(ALL_TESTS)

# REV Executables
SRCS   := $(basename $(notdir $(wildcard $(SRCDIR)/*.cc)))
EXES   := $(addprefix $(OUTDIR)/bin/,$(addsuffix .exe,$(SRCS)))
DIASMS := $(patsubst %.exe,%.dis,$(EXES))
SECTIONS := $(patsubst %.exe,%.sections,$(EXES))

# Targets
LOGS    := $(addsuffix /run.log,$(addprefix $(OUTDIR)/,$(TLIST)))
REVLOGS := $(addsuffix /run.revlog,$(addprefix $(OUTDIR)/,$(TLIST)))
TARGS   := $(LOGS) $(EXES) $(DIASMS) $(SECTIONS) $(REVLOGS)

# Recipes
all: $(TARGS)
run: $(LOGS)
compile: $(EXES) $(DIASMS)

# Rev toolchain
CC=riscv64-unknown-elf-g++
LD=riscv64-unknown-elf-ld
RVOBJDUMP=riscv64-unknown-elf-objdump
OBJDUMP   = ${RVOBJDUMP} --source -l -dC -Mno-aliases

# Rev headers
INCLUDE  := -I$(PROJHOME)/common/syscalls
INCLUDE  += -I$(PROJHOME)/test/include

ifdef DEBUG
 CCOPT       ?= -O0 -g -DDEBUG=1
 DEBUG_MODE   = 1
else
 CCOPT       ?= -O2 -g
endif

ifdef DEBUG_MODE
 CCOPT += -DDEBUG_MODE=1
endif

# Configuration
WFLAGS    := -Wall -Wextra -Wuninitialized -pedantic-errors
WFLAGS    += -Wno-unused-function -Wno-unused-parameter
#WFLAGS    += -Werror
# DEBUG_FLAGS := -DDEBUG_MODE
FLAGS     := $(CCOPT) $(WFLAGS) $(DEBUG_FLAGS) -static -lm -fpermissive
ARCH      := rv64gc
# TODO ARCH      := rv64gcv
# ABI       := -mabi=lp64d

# Test Specific Customization
# $(OUTDIR)/testname/run.log: ...

# The magical run command
%.log: compile
	@mkdir -p $(@D)
	@$(eval app = $(notdir $(@D)))
	$(eval exe = $(shell if [ -z ${REV_EXE} ]; then echo $(OUTDIR)/bin/$(app).exe; else echo ${REV_EXE}; fi))
	@$(eval statf = $(basename $@).status)
	@$(eval csv = $(basename $@).csv)
	@rm -f $(statf) $(csv)
	@echo Running $(basename $@)
	$(SST) $(SSTCFG) -- --noStats --machine=CORES:$(ARCH)_zicntr --program=$(exe) --enableMemH=1 > $@ && (echo "pass" > $(statf))

# To run all with all failures use this instead
#  > $@ && echo "pass" > $(statf) || echo "fail" > $(statf)
# 	@echo "### " $@":" `cat $(statf)`

%.revlog: %.log
	@ $(REVPRINT) -l $< > $@

%.dis: %.exe
	$(OBJDUMP) $< > $@

%.sections: %.exe
	${RVOBJDUMP} -s $< > $@

$(OUTDIR)/bin/%.exe: $(SRCDIR)/%.cc
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -g -march=$(ARCH) $(ABI) $(INCLUDE) -o $@ $<

clean:
	rm -rf $(OUTDIR)

help:
	@echo make compile
	@echo make run
	@echo make TLIST="test1 test2 ..."
	@echo make DOT=1
	@echo Valid TLIST selections are:
	@echo $(ALL_TESTS)
	@echo

.PHONY: run clean help

.SECONDARY:

.PRECIOUS: %.log %.revlog

#-- EOF

