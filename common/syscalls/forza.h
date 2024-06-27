#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifndef _FORZA_H_
#define _FORZA_H_

// clang-format off
#define FORZA_SYSCALL(NUM, PROTO) __attribute__((naked)) \
  static PROTO{ asm(" li a7," #NUM "; ecall; ret"); }

FORZA_SYSCALL( 4000, uint64_t forza_scratchpad_alloc( size_t size ) );
FORZA_SYSCALL( 4001, int forza_scratchpad_free( uint64_t addr, size_t size ) );
FORZA_SYSCALL( 4002, int forza_get_hart_id() );
FORZA_SYSCALL( 4003, int forza_send( uint64_t dst, uint64_t spaddr, size_t size ) );
FORZA_SYSCALL( 4004, int forza_zen_credit_release( size_t size ) );
FORZA_SYSCALL( 4005, int forza_zen_setup( uint64_t addr, size_t size, uint64_t tailptr, uint64_t mbox_id ) );

/**
 * @param addr - start of application zone run queue - should be 8byte aligned
 * @param size - num bytes of application zone run queue; can be 0
 * @param min_hart - low HART number for this application
 * @param max_hart - top HART number for this application (inclusive)
 * @param seq_ld_flag - set to 1 if actor program, 0 for migr thread program
 * @return
 *
 */
FORZA_SYSCALL( 4006, int forza_zqm_setup( uint64_t addr,
                                          uint64_t size,
                                          uint64_t min_hart,
                                          uint64_t max_hart,
                                          uint64_t seq_ld_flag ) );

FORZA_SYSCALL( 4007, int forza_get_harts_per_zap() );
FORZA_SYSCALL( 4008, int forza_get_zaps_per_zone() );
FORZA_SYSCALL( 4009, int forza_get_zones_per_precinct() );
FORZA_SYSCALL( 4010, int forza_get_num_precincts() );
FORZA_SYSCALL( 4011, int forza_get_my_zap() );
FORZA_SYSCALL( 4012, int forza_get_my_zone() );
FORZA_SYSCALL( 4013, int forza_get_my_precinct() );
FORZA_SYSCALL( 4014, void forza_zone_barrier( uint32_t num_harts ) );
FORZA_SYSCALL( 4015, int forza_debug_print(uint64_t a, uint64_t b, uint64_t c) );


FORZA_SYSCALL( 4020, int forza_zqm_mbox_setup( uint64_t addr, size_t size, uint64_t tailptr, uint64_t mbox_id ) );

// clang-format on

#endif
