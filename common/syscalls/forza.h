#include <stdint.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <limits.h>
#include <stdarg.h>

int forza_scratchpad_alloc( size_t size ){
  int rc;
  asm volatile (
    "li a7, 4000 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

int forza_scratchpad_free( uint64_t addr, size_t size ){
  int rc;
  asm volatile (
    "li a7, 4001 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

int forza_get_hart_id( ){
  int rc;
  asm volatile (
    "li a7, 4002 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

int forza_send( uint64_t spaddr, uint64_t size, uint64_t dst ){
  int rc;
  asm volatile (
    "li a7, 4003 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

int forza_poll(){
  int rc;
  asm volatile (
    "li a7, 4004 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

int forza_popq(){
  int rc;
  asm volatile (
    "li a7, 4005 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}

int forza_zen_init(uint64_t addr, uint64_t size){
    int rc;
    asm volatile (
    "li a7, 4006 \n\t"
    "ecall \n\t"
    "mv %0, a0" : "=r" (rc)
  );
}