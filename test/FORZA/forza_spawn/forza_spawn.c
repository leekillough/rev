// cspawn.c
//
// Spawn test in C to sum array elements
//		c[i] = a[i] + b[i]

#include <stdint.h>
#include <stdlib.h>
#include "/home/ksuarez/forzarev/common/syscalls/syscalls.h"
#include "/home/ksuarez/forzarev/common/syscalls/forza.h"

#define F 1		// 1 = use forza, 0 = default sequential code
#if F == 0
#include <stdio.h>
#endif

#define N 12		// Array size
#define T 4		// Number of threads (N must be evenly divisible by T)

// Below are hacks to avoid using malloc
#define FRAME_SIZE 0xFFFUL // Default Frame Size - Currently array_add is 0x40, Hack until we have compiler to generate this
uint64_t STACK[FRAME_SIZE * T];
#define SPAWN_BLOCK_SIZE 0xFFUL
uint64_t SPAWN_BLOCK[SPAWN_BLOCK_SIZE * T];
uint64_t DONE;

uint64_t a[N];
uint64_t b[N];
uint64_t c[N];


uint64_t * spawn_block_ptrs[T];
uint64_t * stack_ptrs[T];

#if F
extern void generic_spawn_helper(); // assembly code to call the function with values from spawn_block

long get_cycle() {
	long result;
	asm volatile ("csrrs %0, time, x0" : "=r" (result) : );
	return result;
}

uint64_t get_global_ptr() {

	uint64_t result;
	asm volatile ("add %0, x0, gp" : "=r" (result) : );
	return result;

}
#endif

uint32_t verify_result(uint64_t* x1, uint64_t* x2, uint64_t* x3) {
	for (uint64_t i = 0; i < N; i++) {
		if ( c[i] != (i+1) ){
			return 1;
		}
	}
	__forza_fence();
	return 0;
}

// array_add: SP offset = 0x50
void array_add(uint64_t* x1, uint64_t* x2, uint64_t* x3, uint64_t start, uint64_t grain) {
	for (uint64_t i = start; i < start + grain; i++) {
		x3[i] = x1[i] + x2[i];
	}
#if F
	__forza_fence();
#endif
	return;
}

void clear_array(uint64_t* x, uint64_t n) {
	for (uint64_t i = 0; i < n; i++) {
		c[i] = 0;
	}

}

void init_arrays(uint64_t* x1, uint64_t* x2, uint64_t* x3) {
	for (uint64_t i = 0; i < N; i++) {
		x1[i] = 1;
		x2[i] = i;
		x3[i] = 0;
	}
}

void allocate_mem_blocks() {
	uint64_t* stack_ptr = STACK;
	uint64_t* spawn_blk_ptr = SPAWN_BLOCK;
	for (long i = 0; i < T; i++) {
#if F == 0
		printf("%d: Stack %p, Spawn blk %p\n", i, stack_ptr, spawn_blk_ptr);
#endif
		stack_ptrs[i] = stack_ptr;
		stack_ptr += (FRAME_SIZE >> 3);
		spawn_block_ptrs[i] = spawn_blk_ptr;
		spawn_blk_ptr += (SPAWN_BLOCK_SIZE >> 3);
	}

}

#if F
uint64_t* create_generic_spawn_block(
	int tid,
	void* fcn_ptr,
	uint64_t * sync_struct_ptr,
	// int args0-7
	uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, 
	uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7, 
	// fp args0-7
	double farg0, double farg1, double farg2, double farg3, 
	double farg4, double farg5, double farg6, double farg7 ) 
{
		//  spawn_block[] (used to initialize child thread)
		// 0: register count(fp register count : int register count)
		//		Could optimize by using this as fp args and int args count instead and assume all others always exist
		//	  However, the cost of computing end and adding extra checks after each arg may offset the performance improvement
		// 1: x2/sp	: stack pointer
		// 2: x3/gp : global pointer
		// 3: x4/tp : thread ptr = sync struct ptr
		// 4: x5/t0 : function pointer
		// 
		// 5: x10/a0 : int arg
		// 6: x11/a1 : int arg 
		// 7: x12/a2 : int arg 
		// 8: x13/a3 : int arg 
		// 9: x14/a4 : int arg 
		// 10: x15/a5 : int arg 
		// 11: x16/a6 : int arg 
		// 12: x17/a7 : int arg
		// 
		// 13: f10/fa0 : fp arg
		// 14: f11/fa1 : fp arg
		// 15: f12/fa2 : fp arg
		// 16: f13/fa3 : fp arg
		// 17: f14/fa4 : fp arg
		// 18: f15/fa5 : fp arg
		// 19: f16/fa6 : fp arg
		// 20: f17/fa7 : fp arg

	uint8_t n_int_reg = 12; // I am assuming 4 ptrs + all integer + all fp args are included for now
	uint8_t n_fp_reg = 8;
	uint64_t spawn_block_size = n_int_reg + n_fp_reg + 1; // 21 (if allocated in big chunk, use 32 for alignment)

	// Allocate spawn block
	// HACK to avoid use of malloc
	//uint64_t* spawn_block = (uint64_t*)malloc(spawn_block_size * sizeof(uint64_t));
	uint64_t* spawn_block = spawn_block_ptrs[tid];

	// Initialize sb[0]: number of int and fp registers
	spawn_block[0] = n_fp_reg;
	spawn_block[0] = spawn_block[0] << 8 | n_int_reg;

	// Initialize sb[1]: global_ptr
	uint64_t gp = get_global_ptr();
	spawn_block[2] = gp; 


	// Initialize sb[2]: Stack ptr
	// HACK to avoid malloc
	//uint64_t * stack_ptr = (uint64_t*)malloc(FRAME_SIZE);
	uint64_t* stack_ptr = stack_ptrs[tid];
	spawn_block[1] = (uint64_t) stack_ptr;


	// Initialize sb[3]: Thread count/sync struct ptr (thread ptr reg)
	spawn_block[3] = (uint64_t) sync_struct_ptr;

	// Initialize sb[4]: Function pointer
	spawn_block[4] = (uint64_t)  &array_add;

	// Initialize sb[5]-sb[12]: integer arguments to function
	spawn_block[5] = arg0;
	spawn_block[6] = arg1;
	spawn_block[7] = arg2;
	spawn_block[8] = arg3;
	spawn_block[9] = arg4;
	spawn_block[10] = arg5;
	spawn_block[11] = arg6;
	spawn_block[12] = arg7;

	// Initialize sb[13]-sb[20]: fp arguments to function
	double* fp_sb = (double *) spawn_block;
	fp_sb[13] = farg0;
#if 0
	spawn_block[14] = farg1;
	spawn_block[15] = farg2;
	spawn_block[16] = farg3;
	spawn_block[17] = farg4;
	spawn_block[18] = farg5;
	spawn_block[19] = farg6;
	spawn_block[20] = farg7;
#endif

	// Store pointers in global arrays for parent free after child threads complete
	//spawn_block_ptrs[tid] = spawn_block;
	//stack_ptrs[tid] = stack_ptr;

	return spawn_block;
}


void free_spawn_blocks() {

	for (int i = 0; i < T; i++) {
		free(spawn_block_ptrs[i]);
		free(stack_ptrs[i]);
	}
}
#endif


int main() {
	// HACK to avoid malloc, I don't want done on the stack for now
	//uint64_t *done = (uint64_t *) malloc( sizeof(uint64_t) );
	uint64_t* done = &DONE;
	uint64_t start;
	uint64_t grain;
	uint64_t* generic_spawn_block;
	uint64_t ret;
	//rev_write(STDOUT_FILENO, "Program starts\n", sizeof("Program starts\n"));
	init_arrays(a, b, c);
	allocate_mem_blocks();


#if 1	// v4: WIP: Parallel spawn with T threads
	// Spawn threads to add in parallel
	* done = T;  // do I need a fence?
	//__forza_amo_r_swap64migr_no(done, T);
	//__forza_fence();
	grain = N / T;
	// Could malloc all spawn blocks initially and then just pass in the pointer
	for (int i = 0; i < T; i++) {
		start = i * grain;
		generic_spawn_block = create_generic_spawn_block(
			i, 
			&array_add,
			done,
			// int args0-7
			(uint64_t)a, 
			(uint64_t)b, 
			(uint64_t)c,
			(uint64_t)start,
			(uint64_t)grain,
			0, 0, 0,
			// fp args0-7
			0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 );
		ret = __forza_spawn((unsigned long *) &generic_spawn_helper, generic_spawn_block);  // Intrinsic for spawn instruction
	}
     
	__forza_fence();
	while (*done !=0) {
	}  // wait for all child threads to sync

    //	free_spawn_blocks();

#endif

        __forza_fence();
	uint32_t res = verify_result(a, b, c);
	return res;
}
