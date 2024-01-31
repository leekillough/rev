#include <stdint.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <limits.h>
#include <stdarg.h>

static int forza_scratchpad_alloc( size_t size ){
  int rc;
  asm volatile (
    "li a7, 4000 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

static int forza_scratchpad_free( uint64_t addr, size_t size ){
  int rc;
  asm volatile (
    "li a7, 4001 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

static int forza_get_hart_id( ){
  int rc;
  asm volatile (
    "li a7, 4002 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

static int forza_send( uint64_t dst, uint64_t spaddr, size_t size  ){
  int rc;
  asm volatile (
    "li a7, 4003 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

static int forza_zen_credit_release(size_t size){
  int rc;
  asm volatile (
    "li a7, 4004 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

static int forza_zen_setup(uint64_t addr, size_t size, uint64_t tailptr){
    int rc;
    asm volatile (
    "li a7, 4005 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

// Reserve 4006 for forza_zqm_setup
static int forza_get_harts_per_zap(){
  int rc;
  asm volatile (
		"li a7, 4007 \n\t"
		"ecall \n\t"
		"mv %0, a0" : "=r" (rc)
		);
}
