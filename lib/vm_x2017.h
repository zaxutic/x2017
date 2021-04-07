#ifndef VM_X2017_H
#define VM_X2017_H

#include "parser.h"

#define RAM_SIZE (1 << 8)
#define NUM_REGISTERS 8

void vm_x2017(func_t* functions);

uint8_t run_instruction(const inst_t inst, uint8_t* ram, uint8_t* registers);
uint8_t arg_value(const arg_t arg, const uint8_t* ram, uint8_t* registers);
void call_function(uint8_t label, uint8_t* ram, uint8_t* registers);

#endif