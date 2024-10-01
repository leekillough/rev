#
# Makefile
#
# makefile: forza_barrier
#
# Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

.PHONY: src

CC=${RVCC}
ARCH=rv64gc
SOURCES := $(wildcard *.c)
EXES=$(SOURCES:.c=.exe)

all: $(EXES)
%.exe:%.c
	$(CC) -march=$(ARCH) -o $@ $< -static
clean:
	rm -Rf *.exe

#-- EOF
