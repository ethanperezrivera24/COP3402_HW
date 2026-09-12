/*
Assignment:
vm - HW1 PM/0 virtual machine

Author: Ethan Perez-Rivera, Kevin Castellanos Valido

Language: C only

To Compile:
  gcc -Wall -Wextra -std=c11 -O2 vm.c -o vm

To Execute (on Eustis):
  ./vm <input_file>

where:
  <input_file> is the path to a text file holding one PM/0 instruction
               per line, as three integers OP L M

Notes:
  - Implements the PM/0 virtual machine described in the homework
    instructions.
  - No heap allocation and no function-like macros. The PAS array is
    indexed, not walked with a pointer.
  - Does not implement any VM instruction as a separate function; the
    fetch-execute cycle runs directly in main.
  - Defines at most three functions: main, base, and one print function.
  - Exits with status 0 after a normal halt and with a non-zero status
    after any error.
  - Tested on Eustis.

Class: COP 3402 - Systems Software

Instructor: Jie Lin, Ph.D.

Due Date: See Webcourses
*/

#include <stdio.h>
#include <stdlib.h>

// Initialize globals
static int pas[1000];
int PC = 200, BP = 999, SP = 1000;

int base(void);
void print(const char *name, int L, int M);

int main(int argc, char *argv[]) {
    // Check for file invocation + input file
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        return 1;
    }

    // Check input file opens
    FILE *file = fopen(argv[1], "r");
    if(!file) {
        fprintf(stderr, "Error: could not open file %s\n", argv[1]);
        return 1;
    }
    
    // Read file into pas with specific file-loading varaibles
    int fop, fL, fM;
    int instr_c = 0;
    // Loop while 3 inputs, stops when EOF or misinput
    while(fscanf(file, "%d %d %d", &fop, &fL, &fM) == 3) {
        pas[200 + 3*instr_c] = fop;
        pas[200 + 3*instr_c + 1] = fL;
        pas[200 + 3*instr_c + 2] = fM;
        /* Checks file is correctly read into correct pas indexes
        printf("OP: %d | L: %d | M: %d\n", op, L, M);
        printf("Pas location> OP: %d | L: %d | M: %d\n", (200+3*instr_c), (200 + 3*instr_c + 1), (200 + 3*instr_c + 2)); */

        instr_c++;
    }

    // Close file, isn't needed anymore
    fclose(file);

    // Print header line and initial register values, once before execution
    printf("\tL\tM\tPC\tBP\tSP\tstack\n");
    printf("Initial values:\t\t%d\t%d\t%d\n", PC, BP, SP);

    // Fetch-execute cycle
    int op, L, M;
    int exit_status = 0;
    while(1) {
        // Read current instruction and parse, increment PC counter
        op = pas[PC];
        L = pas[PC + 1];
        M = pas[PC + 2];
        PC += 3;

        // Depending on op, decide what to do
        switch(op) {
            // LIT
            case 1:
                // Push M onto the stack
                SP = SP - 1;
                pas[SP] = M;
                print("LIT", L, M);
                break;

            // SYS
            case 9:
                // SYS has 3 dif func based on M
                switch(M) {
                    // HALT
                    case 3:
                        // HALT is the last instruction traced
                        print("SYS", L, M);
                        goto finish;
                }
        }
    }

    // Finish label to prevent scattered return statements
    finish: 
        return exit_status;
}

int base(void) {
    return 0;
}

// Trace printer: prints one line for the instruction that just executed.
// Columns are the mnemonic, L, M, then PC, BP and SP after execution,
// followed by the stack contents from address 999 down to SP.
void print(const char *name, int L, int M) {
    printf("%s\t%d\t%d\t%d\t%d\t%d\t", name, L, M, PC, BP, SP);

    // Stack grows downward, so walk from the highest address to the top (SP)
    for(int addr = 999; addr >= SP; addr--) {
        printf("%d ", pas[addr]);
    }
    printf("\n");
}
