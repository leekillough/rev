.PHONY: src

EXAMPLE=forza_main
CC=riscv64-unknown-elf-g++
ARCH=rv64gc

all: $(EXAMPLE).exe
$(EXAMPLE).exe: $(EXAMPLE).cc
	$(CC) -march=$(ARCH) -Wall -o $(EXAMPLE).exe $(EXAMPLE).cc app.cc -static
clean:
	rm -Rf $(EXAMPLE).exe

#-- EOF
