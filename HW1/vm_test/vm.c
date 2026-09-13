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

int base(int bp, int L);
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

            // OPR
            case 2:
                int a, b;
                // Sub-ops based on M
                switch(M) {
                  // RTN
                  case 0:
                      break;

                  // For ADD, SUB, MUL, & DIV, pop stack into b, adjust SP, pop stack into a, overwrite a's location with result & print tracer
                  // ADD
                  case 1:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = a + b;
                      print("ADD", L, M);
                      break;

                  // SUB
                  case 2:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = a - b;
                      print("SUB", L, M);
                      break;

                  // MUL
                  case 3:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = a * b;
                      print("MUL", L, M);
                      break;

                  // DIV
                  case 4:
                      b = pas[SP]; // Needs error check for 0
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = a / b;
                      print("DIV", L, M);
                      break;

                  // For conditionals, same format but switch operator for the conditional statement
                  // EQL
                  case 5:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = (a == b);
                      print("EQL", L, M);
                      break;

                  // NEQ
                  case 6:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = (a != b);
                      print("NEQ", L, M);
                      break;

                  // LSS
                  case 7:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = (a < b);
                      print("LSS", L, M);
                      break;

                  // LEQ
                  case 8:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = (a <= b);
                      print("LEQ", L, M);
                      break;

                  // GTR
                  case 9:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = (a > b);
                      print("GTR", L, M);
                      break;

                  // GEQ
                  case 10:
                      b = pas[SP];
                      SP += 1;
                      a = pas[SP];
                      pas[SP] = (a >= b);
                      print("GEQ", L, M);
                      break;
                }
                break;

            // LOD & STO wrapped in curly braces to not mix up addr variable
            // LOD
            // Get address of target activation record w/ base(), move SP and load into that slot
            case 3: {
                int addr = base(BP, L) - M;
                SP = SP - 1;
                pas[SP] = pas[addr];
                print("LOD", L, M);
                break;
            }

            // STO
            // Get address of target activation record w/ base(), store into that slot and move SP
            case 4: {
                int addr = base(BP, L) - M;
                pas[addr] = pas[SP];
                SP = SP + 1;
                print("STO", L, M);
                break;
            }

            // SYS
            case 9:
                // SYS has 3 dif func based on M
                switch(M) {
                    // WRITE (pops stack and prints)
                    case 1: {
                        int val = pas[SP];
                        SP = SP + 1;
                        printf("Output result is: %d\n", val);
                        print("SYS", L, M);
                        break;
                    }

                    // READ (prints prompt to get int and pushes on stack)
                    case 2: {
                        int val;
                        printf("Please Enter an Integer: ");
                        (void)scanf("%d", &val); // void to prevent warning since scanf returns an integer like fscanf does
                        printf("%d\n", val);
                        SP = SP - 1;
                        pas[SP] = val;
                        print("SYS", L, M);
                        break;
        }

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

int base(int bp, int L) {
    int arb = bp;
    while(L > 0) {
        arb = pas[arb];
        L--;
    }

    return arb;
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
