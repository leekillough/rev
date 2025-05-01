#
# Makefile
#
# makefile: forza_spawn
#
# Copyright (C) 2017-2025 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

.PHONY: src

EXAMPLE=forza_spawn
CC=${RVCC}
ARCH=rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0_zfa1p0_zfh1p0_zfhmin1p0_zbs1p0_xforza1p0

WORDONLY=forza_spawn_word

all: $(EXAMPLE).exe $(WORDONLY).exe
$(EXAMPLE).exe: $(EXAMPLE).c
	$(CC) -march=$(ARCH) -o $(EXAMPLE).exe $(EXAMPLE).c generic_spawn_helper.asm -static
$(WORDONLY).exe: $(WORDONLY).c
	$(CC) -march=$(ARCH) -o $(WORDONLY).exe $(WORDONLY).c -static
clean:
	rm -Rf $(EXAMPLE).exe
	rm -Rf $(WORDONLY).exe

#-- EOF
