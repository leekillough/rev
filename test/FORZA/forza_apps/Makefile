.PHONY: src

EXAMPLE=forza_main
CC=riscv64-unknown-elf-g++
ARCH=rv64gc

all: $(EXAMPLE).exe
$(EXAMPLE).exe: $(EXAMPLE).cpp 
	$(CC) -march=$(ARCH) -Wall -o $(EXAMPLE).exe $(EXAMPLE).cpp app.cpp -static
clean:
	rm -Rf $(EXAMPLE).exe

#-- EOF
