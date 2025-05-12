#
# Makefile
#
# makefile: large_bss
#
# Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

.PHONY: src

PROGRAMS_C=$(wildcard *.c)
PROGRAMS_EXE=$(patsubst %.c,%.exe,$(wildcard *.c))
ARCH=rv64g
ABI=lp64d
RVCC=/opt/riscv/bin/riscv64-unknown-elf-gcc

all: $(PROGRAMS_EXE)
%.exe: %.c
	$(RVCC) -march=$(ARCH) -mabi=$(ABI) -O1 -o $*.exe $*.c
clean:
	rm -Rf $(PROGRAMS_EXE)

#-- EOF
