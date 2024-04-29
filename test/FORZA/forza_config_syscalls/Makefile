#
# Makefile
#
# makefile:
#
# Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

.PHONY: src

EXAMPLE=ex2
CC=${RVCC}
ARCH=rv64gc

TESTECALLS=config_ecalls

all: $(EXAMPLE).exe $(TESTECALLS).exe

all: $(EXAMPLE).exe
$(EXAMPLE).exe: $(EXAMPLE).c
	$(CC) -march=$(ARCH) -o $(EXAMPLE).exe $(EXAMPLE).c -static

$(TESTECALLS).exe: $(TESTECALLS).c
	$(CC) -march=$(ARCH) -o $(TESTECALLS).exe $(TESTECALLS).c -static

clean:
	rm -Rf $(EXAMPLE).exe

#-- EOF
