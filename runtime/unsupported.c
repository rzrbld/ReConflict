#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
void ds_unsupported_instruction(uint32_t address, const char *instruction) {
    fprintf(stderr, "Unsupported translated instruction at guest VA %#x: %s\n", address, instruction);
    abort();
}
