/*
 * argc.c
 *
 * Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 *
 */

#include <stdlib.h>

#define assert(x)                                                              \
  do                                                                           \
    if (!(x)) {                                                                \
      asm(".dword 0x00000000");                                                \
    }                                                                          \
  while (0)

int main(int argc, char **argv) {
  int a = argc;
  assert(a == 2);
  //forza_argc.exe
  assert(argv[0][0] == 'f');
  assert(argv[0][1] == 'o');
  assert(argv[0][2] == 'r');
  assert(argv[0][3] == 'z');
  assert(argv[0][4] == 'a');
  assert(argv[0][5] == '_');
  assert(argv[0][6] == 'a');
  assert(argv[0][7] == 'r');
  assert(argv[0][8] == 'g');
  assert(argv[0][9] == 'c');
  assert(argv[0][10] == '.');
  assert(argv[0][11] == 'e');
#if 0
  assert(argv[0][12] == 'x');
  assert(argv[0][12] == 'x');
  assert(argv[0][13] == 'e');
#endif
  assert(argv[1][0] == 'o');
  assert(argv[1][1] == 'n');
  assert(argv[1][2] == 'e');
  return 0;
}
