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

int main(int argc, char* argv[]) {
  // Check for correct usage
    if(argc != 2) {
        printf("Usage: %s <input file>\n", argv[0]);
        return 1;
    }

    // Open file in binary mode to avoid unwanted text translation
    FILE *fp = fopen(argv[1], "rb");
    if(!fp) {
        printf("Error: unable to open input file '%s'\n", argv[1]);
        return 1;
    }

    // Travel to end of file with fseek & return size of file, rewind to start of file after
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    rewind(fp);

    // If ftell fails, close file & return 1
    if(fileSize < 0) {
        fclose(fp);
        return 1;
    }

    // Allocate buffer to hold the whole file, plus one byte of
    // slack (not relied on as a null terminator, since the file may contain
    // a 0x00 byte)
    char *buf = malloc((size_t)fileSize + 1);
    if (buf == NULL) {
        fclose(fp);
        return 1;
    }

    // Read the entire file into buf using fread because source
    // may contain bytes that would stop fgets or fscanf
    size_t bytesRead = fread(buf, 1, (size_t)fileSize, fp);
    
    // Close file
    fclose(fp);

    /* Debug: prints every character in file
    for(int i = 0; i < bytesRead; i++) {
      printf("%c", buf[i]);
    }
    */

    // Free buf
    free(buf);

    return 0;
}
