#
# src/zen/Makefile
#


ifeq (, $(shell which sst-config))
 $(error "No sst-config in $(PATH), add `sst-config` to your PATH")
endif

CXX=$(shell sst-config --CXX)
CXXFLAGS=$(shell sst-config --ELEMENT_CXXFLAGS)
LDFLAGS=$(shell sst-config --ELEMENT_LDFLAGS)
CPPFLAGS=-I../../include -I../../../forzarev/include
# OPTIMIZE_FLAGS=-O2
OPTIMIZE_FLAGS=-g -O0

COMPONENT_LIB=libforzazip.so

BASICCOMPONENT_SOURCES := $(wildcard *.cc)

BASICCOMPONENT_HEADERS := $(wildcard *.h)

BASICCOMPONENT_OBJS := $(patsubst %.cc,%.o,$(wildcard *.cc))

all: $(COMPONENT_LIB)

debug: CXXFLAGS += -DDEBUG -g
debug: $(COMPONENT_LIB)

$(COMPONENT_LIB): $(BASICCOMPONENT_OBJS)
	$(CXX) $(OPTIMIZE_FLAGS) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ *.o
%.o:%.cc $(SIMPLEEXAMPLE_HEADERS)
	$(CXX) $(OPTIMIZE_FLAGS) $(CXXFLAGS) $(CPPFLAGS) -c $<
install: $(COMPONENT_LIB)
	sst-register ForzaZIP ForzaZIP_LIBDIR=$(CURDIR)
uninstall:
	sst-register -u ForzaZIP
clean:
	rm -Rf *.o $(COMPONENT_LIB)
