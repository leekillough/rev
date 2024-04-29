#
# Makefile
#
# makefile: forza_zen_setup_c
#
# Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

.PHONY: src

EXAMPLE=forza_zen_setup
EXCXX=forza_zen_setup_cxx
CC=${RVCC}
ARCH=rv64gc
CXX=${RVCXX}
CXXFLAGS="-std=c++14" # warns about using register as type otherwise (c++17 default)

all: $(EXAMPLE).exe $(EXCXX).exe
$(EXAMPLE).exe: $(EXAMPLE).c
	$(CC) -march=$(ARCH) -o $(EXAMPLE).exe $(EXAMPLE).c -static

$(EXCXX).exe: $(EXAMPLE).c
	$(CXX) -march=$(ARCH) $(CXXFLAGS) -o $(EXCXX).exe $(EXAMPLE).c -static

clean:
	rm -Rf $(EXAMPLE).exe $(EXCXX).exe

#-- EOF
