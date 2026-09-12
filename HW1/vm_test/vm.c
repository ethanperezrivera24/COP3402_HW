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

static int pas[1000];
int PC = 200, BP = 999, SP = 1000;

int base(void);
void print(void);

int main(int argc, char *argv[]) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if(!file) {
        fprintf(stderr, "Error: could not open file %s\n", argv[1]);
        return 1;
    }
    
    int op, L, M;
    int instr_c = 0;

    while(fscanf(file, "%d %d %d", &op, &L, &M) == 3) {
        pas[200 + 3*instr_c] = op;
        pas[200 + 3*instr_c + 1] = L;
        pas[200 + 3*instr_c + 2] = M;
        // printf("OP: %d | L: %d | M: %d\n", op, L, M);
        // printf("Pas location> OP: %d | L: %d | M: %d\n", (200+3*instr_c), (200 + 3*instr_c + 1), (200 + 3*instr_c + 2));

        instr_c++;
    }

    fclose(file);

    return 0;
}

int base(void) {
    return 0;
}

void print(void) {
    
}
