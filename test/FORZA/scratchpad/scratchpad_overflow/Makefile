#
# Makefile
#
# makefile: scratchpad_overflow
#
# Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

.PHONY: src
EXAMPLE=scratchpad_overflow

CC="${RVCC}"
#ARCH=rv64g
ARCH=rv64gc

all: $(EXAMPLE).exe
$(EXAMPLE).exe: $(EXAMPLE).c
	$(CC) -march=$(ARCH) -O0 -o $(EXAMPLE).exe $(EXAMPLE).c  -static
clean:
	rm -Rf $(EXAMPLE).exe

#-- EOF
