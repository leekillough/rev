#
# Makefile
#
# makefile: amoadd_c
#
# Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

.PHONY: src

EXAMPLE=jaccard
CC=/opt/RISCV/bin/riscv64-unknown-elf-g++
ARCH=rv64gc

all: $(EXAMPLE).exe
$(EXAMPLE).exe: $(EXAMPLE).cpp
	$(CC) -march=$(ARCH) -Wall -o $(EXAMPLE).exe $(EXAMPLE).cpp -static
clean:
	rm -Rf $(EXAMPLE).exe

#-- EOF
