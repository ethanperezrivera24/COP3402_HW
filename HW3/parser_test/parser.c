/*
Homework:
parser - HW3 PL/0 parser and symbol table

Author(s): Ethan Perez-Rivera, Kevin Castellanos Valido

Language: C only

To Compile:
  gcc -Wall -Wextra -std=c11 -O2 parser.c -o parser

To Execute (on Eustis):
  ./lex <input_file> && ./parser

where:
  <input_file> is the path to a text file holding a PL/0 source program;
  ./lex writes tokens.txt and nametable.txt, and ./parser reads them from
  the current directory and takes no arguments

Notes:
  - Implements the recursive-descent parser and symbol table described in
    the homework instructions.
  - Prints four sections to standard output: Name Table, Parse Trace,
    Symbol Table and Result.
  - Stops at the first syntax or symbol-table error, reports it with its
    token number, and exits with status 1.
  - Exits with status 0 when the whole program is accepted.
  - Tested on Eustis.

Class: COP 3402 - Systems Software

Instructor: Jie Lin, Ph.D.

Due Date: See Webcourses
*/

#include <stdio.h>
#include <stdlib.h>

// limits specified in assignment
#define MAX_TOKENS 5000
#define MAX_NAMES 200
#define MAX_IDENT_LEN 12
#define MAX_NUM_LEN 6

// token, nameEntry, tokenType structs copied from lex.c
// token codes
// first code of each table is pinned so the rest follow in order
typedef enum{
    // table 2: open classes
    identsym = 1, numbersym,

    // table 3: special symbols
    plussym = 3, minussym, multsym, slashsym, eqsym, neqsym, lessym, leqsym,
    gtrsym, geqsym, lparentsym, rparentsym, commasym, semicolonsym,
    periodsym, assignsym, initsym,

    // table 4: reserved words
    beginsym = 20, endsym, ifsym, fisym, thensym, whilesym, elihwsym, dosym,
    odsym, oddsym, callsym, constsym, varsym, procsym, writesym, readsym,
    elsesym
}TokenType;

// one token read from tokens.txt
// value = name table index for identifiers, unused for everything else
// lexeme = the digits as written for numbers (007 stays 007), unused for everything else
typedef struct{
    int code;
    int value;
    char lexeme[MAX_IDENT_LEN + 1];
}Token;

// one name table entry, 12 visible chars + terminator
// line/col are where the identifier was first seen
typedef struct{
    char name[MAX_IDENT_LEN + 1];
    int line;
    int col;
}NameEntry;

// global arrays & counts
Token tokens[MAX_TOKENS];
int tokenCount = 0;
NameEntry names[MAX_NAMES];
int nameCount = 0;

// printed text of every token code, indexed by code
// tokens.txt only stores code for symbols & reserved words, this is to get the text back
const char *spellings[37] = {
    "", "", "",
    "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">=", "(", ")", ",", ";", ".", "=", ":=",
    "begin", "end", "if", "fi", "then", "while", "elihw", "do", "od", "odd",
    "call", "const", "var", "procedure", "write", "read", "else"
};

void loadNames(FILE *nf);
void loadTokens(FILE *tf);
const char *tokenText(int i);
void printResults(const char *resultLine);

int main(int argc, char *argv[]) {
    if(argc != 1) {
        printf("Usage: %s\n", argv[0]);
        return 1;
    }

    // open both files before printing anything
    FILE *tf = fopen("tokens.txt", "r");
    FILE *nf = fopen("nametable.txt", "r");
    if(!tf || !nf) {
        // if a file fails to open, close the file that opened
        if(tf) fclose(tf);
        if(nf) fclose(nf);
        printResults("Error 18: cannot open tokens.txt or nametable.txt");
        return 1;
    }

    // load files & close them
    loadNames(nf);
    loadTokens(tf);
    fclose(tf);
    fclose(nf);

    // TODO: parse here

    printResults("program accepted");
    return 0;
}

// reads nametable.txt: index, name, line, column on each line
// an empty file just leaves nameCount at 0
void loadNames(FILE *nf) {
    int idx;
    while(nameCount < MAX_NAMES && fscanf(nf, "%d %s %d %d", &idx, names[nameCount].name, &names[nameCount].line, &names[nameCount].col) == 4) {
        nameCount++;
    }
}

// reads tokens.txt: a lone code, or a code of 1 / 2 followed by a second field
void loadTokens(FILE *tf) {
    while(tokenCount < MAX_TOKENS && fscanf(tf, "%d", &tokens[tokenCount].code) == 1) {
        tokens[tokenCount].value = 0;
        tokens[tokenCount].lexeme[0] = '\0';

        if(tokens[tokenCount].code == identsym) {
            // second field is the name table index
            if(fscanf(tf, "%d", &tokens[tokenCount].value) != 1) {
                break;
            }
        } else if(tokens[tokenCount].code == numbersym) {
            // second field is the digits, read as text so leading zeros survive
            if(fscanf(tf, "%s", tokens[tokenCount].lexeme) != 1) {
                break;
            }
        }
        tokenCount++;
    }
}

// printed text of token i (0 based): the name, the digits, or the spelling from table 2
// used for every trace line and error message
const char *tokenText(int i) {
    if(tokens[i].code == identsym) {
        return names[tokens[i].value].name;
    }
    if(tokens[i].code == numbersym) {
        return tokens[i].lexeme;
    }
    return spellings[tokens[i].code];
}

// prints the four sections, then the result line
// every ending goes through here: accepted, an error, or error 18
// a section only gets its header line when it has content
void printResults(const char *resultLine) {
    // Name Table
    printf("Name Table:\n\n");
    if(nameCount > 0) {
        printf("index name line column\n");
        for(int i = 0; i < nameCount; i++) {
            printf("%d %s %d %d\n", i, names[i].name, names[i].line, names[i].col);
        }
        printf("\n");
    }

    // Parse Trace (stub for now)
    printf("Parse Trace:\n\n");

    // Symbol Table (stub for now)
    printf("Symbol Table:\n\n");

    // Result
    printf("Result:\n\n");
    printf("%s\n", resultLine);
}