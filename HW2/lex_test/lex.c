/*
Homework:
lex - HW2 PL/0 lexical analyzer

Author(s): Ethan Perez-Rivera, Kevin Castellanos Valido

Language: C only

To Compile:
  gcc -Wall -Wextra -std=c11 -O2 lex.c -o lex

To Execute (on Eustis):
  ./lex <input_file>

where:
  <input_file> is the path to a text file holding a PL/0 source program

Notes:
  - Implements the lexical analyzer described in the homework
    instructions.
  - Prints four sections to standard output: Source Program, Lexeme
    Table, Name Table and Token List.
  - Writes two files into the working directory: tokens.txt and
    nametable.txt.
  - Stops at the first lexical error, prints everything scanned before
    it, reports the error with its line and column, and exits with a
    non-zero status.
  - Exits with status 0 when the whole program scans without an error.
  - Tested on Eustis.

Class: COP 3402 - Systems Software

Instructor: Jie Lin, Ph.D.

Due Date: See Webcourses
*/

#include <stdio.h>
#include <stdlib.h>

int main(){
    
}
