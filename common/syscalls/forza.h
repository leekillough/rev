#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifndef _FORZA_H_
#define _FORZA_H_

// clang-format off
#define FORZA_SYSCALL(NUM, PROTO) __attribute__((naked)) \
  static PROTO{ asm(" li a7," #NUM "; ecall; ret"); }

FORZA_SYSCALL( 4000, uint64_t forza_read_zen_status() );
FORZA_SYSCALL( 4001, uint64_t forza_read_zqm_status() );
FORZA_SYSCALL( 4002, int forza_get_hart_id() );
FORZA_SYSCALL( 4003, int forza_send_word( uint64_t data, bool is_control_wd ) );
FORZA_SYSCALL( 4004, int forza_receive_word( uint64_t mbox_id ) );
FORZA_SYSCALL( 4005, uint64_t forza_zen_get_cntrs() );
FORZA_SYSCALL( 4006, int forza_zqm_setup( uint64_t logical_pe, uint64_t n_mailboxes ) );
FORZA_SYSCALL( 4007, int forza_get_harts_per_zap() );
FORZA_SYSCALL( 4008, int forza_get_zaps_per_zone() );
FORZA_SYSCALL( 4009, int forza_get_zones_per_precinct() );
FORZA_SYSCALL( 4010, int forza_get_num_precincts() );
FORZA_SYSCALL( 4011, int forza_get_my_zap() );
FORZA_SYSCALL( 4012, int forza_get_my_zone() );
FORZA_SYSCALL( 4013, int forza_get_my_precinct() );
FORZA_SYSCALL( 4014, void forza_zone_barrier( uint32_t num_harts ) );
FORZA_SYSCALL( 4015, int forza_debug_print(uint64_t a, uint64_t b, uint64_t c) );

// clang-format on

#endif
