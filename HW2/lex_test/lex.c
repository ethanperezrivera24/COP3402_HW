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
#include <string.h>

// longest identifier (12) and number (6) the scanner accepts, counted as visible chars
#define MAX_IDENT_LEN 12
#define MAX_NUM_LEN 6

// starting size for the growable arrays, doubled every time one fills up
#define INITIAL_CAP 16

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

// one scanned token
// value = name table index for identifiers, unused for everything else
// lexeme = spelling as written, so numbers keep their digits exactly (007 stays 007)
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

// reserved word spelling paired with its token code
typedef struct{
    const char *word;
    int code;
}ReservedWord;

// All 17 reserved words (Table 4)
const ReservedWord reservedWords[] = {
    {"begin", beginsym},
    {"end", endsym},
    {"if", ifsym},
    {"fi", fisym},
    {"then", thensym},
    {"while", whilesym},
    {"elihw", elihwsym},
    {"do", dosym},
    {"od", odsym},
    {"odd", oddsym},
    {"call", callsym},
    {"const", constsym},
    {"var", varsym},
    {"procedure", procsym},
    {"write", writesym},
    {"read", readsym},
    {"else", elsesym}
};
const int numReserved = sizeof(reservedWords) / sizeof(reservedWords[0]);

// initialize globals
// growable token list: pointer, count, capacity
Token *tokens = NULL;
int tokenCount = 0, tokenCap = 0;

// growable name table: pointer, count, capacity
NameEntry *names = NULL;
int nameCount = 0, nameCap = 0;

int addToken(int code, int value, const char *lexeme);
int findOrAddName(const char *name, int line, int col);

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

    // Free buf, token list and name table
    free(buf);
    free(tokens);
    free(names);

    return 0;
}

// appends one token to the token list, doubling the list first if it's full
// returns 1 on success, 0 if realloc fails
int addToken(int code, int value, const char *lexeme){
    if(tokenCount == tokenCap){
        int newCap = (tokenCap == 0) ? INITIAL_CAP : tokenCap * 2;
        // realloc into tmp so the old list isn't lost if it fails
        Token *tmp = realloc(tokens, (size_t)newCap * sizeof(Token));
        if(tmp == NULL){
            return 0;
        }
        tokens = tmp;
        tokenCap = newCap;
    }

    tokens[tokenCount].code = code;
    tokens[tokenCount].value = value;
    // copy at most 12 chars & always terminate, accepted lexemes are never longer
    strncpy(tokens[tokenCount].lexeme, lexeme, MAX_IDENT_LEN);
    tokens[tokenCount].lexeme[MAX_IDENT_LEN] = '\0';
    tokenCount++;

    return 1;
}

// returns the name table index of name, searching first & only adding it if it's new
// line/col are only recorded the first time the name is seen
// returns -1 if realloc fails
int findOrAddName(const char *name, int line, int col){
    // linear search from index 0, exact & case sensitive (Total != total)
    for(int i = 0; i < nameCount; i++){
        if(strcmp(names[i].name, name) == 0){
            return i;
        }
    }

    // not found, grow the table if it's full
    if(nameCount == nameCap){
        int newCap;
        if (nameCap == 0){
            newCap = INITIAL_CAP;
        }else{
            newCap = nameCap * 2;
        }
        NameEntry *tmp = realloc(names, (size_t)newCap * sizeof(NameEntry));
        if(tmp == NULL){
            return -1;
        }
        names = tmp;
        nameCap = newCap;
    }

    strncpy(names[nameCount].name, name, MAX_IDENT_LEN);
    names[nameCount].name[MAX_IDENT_LEN] = '\0';
    names[nameCount].line = line;
    names[nameCount].col = col;
    nameCount++;

    return nameCount - 1;
}
