#
# Makefile
#
# makefile: FORZA/isa
#
# Copyright (C) 2017-2025 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

.PHONY: src

CC=${FORZACC}
CXX=${FORZACXX}

ISA_SOURCES := $(wildcard *.c)
ISA_OBJS=$(ISA_SOURCES:.c=.o)
ISA_EXES=$(ISA_SOURCES:.c=.exe)
FORZA_OPTS ?= --target=riscv64-unknown-elf -mcpu=forza

all:$(ISA_EXES)
$(ISA_TESTS): $(ISA_EXES)
%.o:%.c
	$(CC) $(FORZA_OPTS)  -o $@ $<
%.exe:%.o
	$(RVCC) -o $@ $<
clean:
	rm -Rf *.exe *.o

#-- EOF
