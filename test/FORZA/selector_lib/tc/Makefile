# include $(HCLIB_ROOT)/../modules/bale_actor/inc/hclib_bale_actor.pre.mak
# include $(HCLIB_ROOT)/include/hclib.mak
# include $(HCLIB_ROOT)/../modules/bale_actor/inc/hclib_bale_actor.post.mak

CXX ?= CC
BALE_FLAGS ?= -DUSE_SHMEM=0
FORZA_FLAGS ?= -DFORZA -Wall

TARGETS= triangle_forza

all: $(TARGETS)

# test: test.cpp
# 	$(CXX) -g -O3 -std=c++11 $(BALE_FLAGS) $(HCLIB_CFLAGS) $(HCLIB_LDFLAGS) -o $@ $^ $(HCLIB_LDLIBS) -lspmat -lconvey -lexstack -llibgetput -lhclib_bale_actor -lm

triangle_forza: triangle.cpp selector.forza.h forzalib.h
	$(CXX) -g -O3 -std=c++11 $(FORZA_FLAGS) -lpthread -o $@ $^

clean:
	rm -f $(TARGETS)
