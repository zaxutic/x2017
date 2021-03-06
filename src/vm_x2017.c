#include "vm_x2017.h"

/*
 * The entry point
 */
int main(int argc, char** argv) {
    if (argc != 2)
        errx(1, "Incorrect number of arguments. Please provide a single binary "
                "file path.");

    func_t functions[MAX_FUNCTIONS] = {{ .size = 0 }};

    parse(argv[1], functions);
    vm_x2017(functions);

    return 0;
}

/*
 * The bulk of the program. Executes the x2017 program.
 */
void vm_x2017(func_t* functions) {
    uint8_t ram[RAM_SIZE];

    // initialise function addresses. we have a default value in order to check
    // if a main function is present. note that this default value is impossible
    // to be a function entry point as it can only be the last instruction, with
    // every function having maximum number of instructions
    for (int i = 0; i < MAX_FUNCTIONS; i++)
        ram[i] = MAX_INSTRUCTIONS;

    uint8_t registers[NUM_REGISTERS];

    inst_t instructions[MAX_INSTRUCTIONS_TOTAL];
    uint8_t instr_idx = 0;

    for (uint8_t i = 0; i < MAX_FUNCTIONS; i++) {
        func_t func = functions[MAX_FUNCTIONS - i - 1];

        if (!func.size)
            continue;

        INSTR_ADDR(func.label) = instr_idx;
        if (!func.size || func.instructions[func.size - 1].opcode != RET)
            errx(1, "No return instruction found at end of function %d",
                    func.label);

        for (uint8_t j = 0; j < func.size; j++, instr_idx++)
            instructions[instr_idx] = func.instructions[j];

        FRAME_SIZE(func.label) = func.frame_size;
    }

    PROG_CTR = INSTR_ADDR(MAIN_FUNC);

    if (PROG_CTR == UINT8_MAX)
        errx(1, "No main function found");

    // we'll have our stack frames backwards to make it easier to add them
    STACK_PTR = STACK_START + FRAME_SIZE(MAIN_FUNC);
    ram[STACK_PTR + 1] = 0; // 0 frame pointer to indicate this is entry point

    while (!run_instruction(instructions[PROG_CTR++], ram, registers)) {}

    return;
}

/*
 * Executes a single vm instruction
 */
uint8_t run_instruction(const inst_t inst, uint8_t* ram, uint8_t* registers) {
    switch (inst.opcode) {
    case MOV:
        // copy value from B to A
        if (inst.arg1.type == VAL)
            errx(1, "first argument to MOV must not be value typed.");

        registers[4] = arg_value(inst.arg2, ram, registers);
        mov(inst.arg1, ram, registers);

        break;
    case CAL:
        // call another function
        if (inst.arg1.type != VAL)
            errx(1, "first argument to CAL must be value typed.");

        call_function(inst.arg1.value, ram, registers);
        break;
    case RET:
        // return
        registers[4] = STACK_PTR + 1; // frame ptr position
        if (ram[registers[4]] == 0)
            // exit if returning from main function
            return 1;

        STACK_PTR = ram[registers[4]];

        registers[4]++; // previous program counter
        PROG_CTR = ram[registers[4]];
        break;
    case REF:
        if (inst.arg2.type != STACK && inst.arg2.type != PTR)
            errx(1, "second argument to REF must be stack or pointer typed.");

        // store the address of stack symbol B into A
        registers[4] = STACK_LOC(inst.arg2.value);
        if (inst.arg2.type == PTR)
            registers[4] = ram[registers[4]];

        mov(inst.arg1, ram, registers);
        break;
    case ADD:
        // add registers A and B, storing into A
        if (inst.arg1.type != REG || inst.arg2.type != REG)
            errx(1, "both arguments to ADD must be register typed.");

        registers[inst.arg1.value] += registers[inst.arg2.value];
        break;
    case PRINT:
        // print value at address as uint
        registers[4] = arg_value(inst.arg1, ram, registers);
        printf("%u\n", registers[4]);
        break;
    case NOT:
        // inplace bitwise NOT on register
        if (inst.arg1.type != REG)
            errx(1, "first argument to NOT must be register typed.");

        registers[inst.arg1.value] = ~registers[inst.arg1.value];
        break;
    case EQU:
        // tests if a register is equal to 0
        if (inst.arg1.type != REG)
            errx(1, "first argument to EQU must be register typed.");

        registers[inst.arg1.value] = registers[inst.arg1.value] == 0;
        break;
    }

    return 0;
}

/*
 * Helper to copy a value from one place to another in the vm
 */
void mov(const arg_t arg, uint8_t* ram, uint8_t* registers) {
    switch (arg.type) {
    case REG:
        registers[arg.value] = registers[4];
        break;
    case STACK:
        registers[5] = STACK_LOC(arg.value);
        ram[registers[5]] = registers[4];
        break;
    case PTR:
        registers[5] = STACK_LOC(arg.value);
        registers[5] = ram[registers[5]];
        // we now have the address and can dereference this pointer
        ram[registers[5]] = registers[4];
        break;
    case VAL:
        // our parser already ensures it can't be VAL
        break;
    }
}

/*
 * Gets the value from an argument
 *
 * For a VAL argument type, simply returns the raw value. For a REG argument,
 * returns the value stored in the corresponding register. For a STACK argument,
 * returns the value stored in the stack for that stack symbol. For a PTR
 * argument, treats it as a stack symbol, reads that symbol's value, then using
 * that value as a memory address, returns the value stored at that address.
 */
uint8_t arg_value(const arg_t arg, const uint8_t* ram, uint8_t* registers) {
    switch (arg.type) {
    case VAL:
        return arg.value;
    case REG:
        return registers[arg.value];
    case STACK:
        registers[4] = STACK_LOC(arg.value);
        return ram[registers[4]];
    case PTR:
        registers[4] = STACK_LOC(arg.value);
        registers[5] = ram[registers[4]];
        // we now have the address and can dereference this pointer
        return ram[registers[5]];
    default:
        // unreached
        // needed to stop gcc from complaining about end of non-void function
        return 0;
    }
}

/*
 * Calls an x2017 function by its label
 */
void call_function(uint8_t label, uint8_t* ram, uint8_t* registers) {
    // we need to offset by 2 for the "actual" edge of this stack frame,
    // then offset by 2 again for the extra information for the next stack frame
    registers[4] = STACK_MAX - ram[MAX_FUNCTIONS + label] - 4;
    if (STACK_PTR > registers[4])
        errx(1, "Stack overflow detected when trying to call function %d",
                label);

    // new stack pointer
    // note that after each frame we reserve 2 bytes for return information
    registers[4] = STACK_PTR + 2 + FRAME_SIZE(label);

    // store frame pointer
    registers[5] = registers[4] + 1;
    ram[registers[5]] = STACK_PTR;

    // store previous program counter
    registers[5]++;
    ram[registers[5]] = PROG_CTR;

    // update stack pointer and program counter
    STACK_PTR = registers[4];
    PROG_CTR = INSTR_ADDR(label);
}
