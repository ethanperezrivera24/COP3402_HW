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
#define MAX_SYMBOL_COUNT 100

// kinds
#define CONSTANT 1
#define VARIABLE 2

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

typedef struct{
    int kind;
    int nameIndex;
    int value;
    int level;
    int address;
    int mark;
}SymbolEntry;

// global arrays & counts
Token tokens[MAX_TOKENS];
int tokenCount = 0;
NameEntry names[MAX_NAMES];
int nameCount = 0;
SymbolEntry symbolTable[MAX_SYMBOL_COUNT]; // symbol table and declarations
int symbolCount = 0;
int nextAddress = 3; // 0 to 2 taken by static link, dynamic link, and return address
int cur = 0; // index of the current token (next one not consumed)

// printed text of every token code, indexed by code
// tokens.txt only stores code for symbols & reserved words, this is to get the text back
const char *spellings[37] = {
    "", "", "",
    "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">=", "(", ")", ",", ";", ".", "=", ":=",
    "begin", "end", "if", "fi", "then", "while", "elihw", "do", "od", "odd",
    "call", "const", "var", "procedure", "write", "read", "else"
};

// error messages copied from table 3, indexed by error number
const char *errorMessages[19] = {
    "",
    "the program must end with a period, and nothing may follow it",
    "const, var, and read must be followed by an identifier",
    "a constant must be declared as name := number",
    "declarations must be separated by , and end with ;",
    "an identifier that starts a statement must be followed by =",
    "begin must be closed with end",
    "a condition after if or while must be followed by then",
    "if must be closed with fi, and while with elihw",
    "a condition must contain one of == != < <= > >=",
    "a left parenthesis must be closed with a right parenthesis",
    "a factor must be an identifier, a number, or (",
    "procedure declarations are not part of this grammar",
    "call statements are not part of this grammar",
    "identifier already declared in this block",
    "undeclared identifier",
    "only a variable may be assigned or read",
    "symbol table is full",
    "cannot open tokens.txt or nametable.txt"
};

void loadNames(FILE *nf);
void loadTokens(FILE *tf);
const char *tokenText(int i);
void printNameTable(void);
void printSymbolTable(void);
void failOpen(void);
void fail(int n, int i);
int currentCode(void);
void consume(const char *owner);
void program(void);
void block(void);
void constDeclaration(void);
void varDeclaration(void);
void statement(void);
void statementList(void);
void condition(void);
void expression(void);
void term(void);
void factor(void);
int isDuplicate(int nameTableIndex);
int lookup(int nameTableIndex);
void insertHelper(int kind, int nameIndex, int value, int address);
void declarationHelper(int nameIndex);
int lookupHelper(int nameIndex, int mustBeVariable);

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
        failOpen();
    }

    // load files & close them
    loadNames(nf);
    loadTokens(tf);
    fclose(tf);
    fclose(nf);

    printNameTable();
    printf("Parse Trace:\n\n");
    printf("token input text nonterminal\n");
    printf("  enter level 0\n");

    // any error inside program() prints the rest of the output and exits
    program();

    printf("\n"); // closes the parse trace section
    printSymbolTable();
    printf("Result:\n\n");
    printf("program accepted\n");
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

// prints name table section, called before parsing starts
void printNameTable(void) {
    printf("Name Table:\n\n");
    if(nameCount > 0) {
        printf("index name line column\n");
        for(int i = 0; i < nameCount; i++) {
            printf("%d %s %d %d\n", i, names[i].name, names[i].line, names[i].col);
        }
        printf("\n");
    }
}

// stub for now, filled in with the symbol table phases
void printSymbolTable(void) {
    printf("Symbol Table:\n\n");
    if(symbolCount > 0){
        printf("entry kind name value level address mark\n");
        for(int i = 0; i < symbolCount; i++){
            printf("%d ", i); // entry number
            if(symbolTable[i].kind == CONSTANT){ // kind
                printf("constant ");
            }
            else{
                printf("variable ");
            }
            printf("%s ", names[symbolTable[i].nameIndex].name); // name

            // value and address are -1 when they don't apply, printed as a dash
            if(symbolTable[i].value == -1){ // value
                printf("- ");
            }
            else{
                printf("%d ", symbolTable[i].value);
            }
            printf("%d ", symbolTable[i].level); // level
            if(symbolTable[i].address == -1){ // address
                printf("- ");
            }
            else{
                printf("%d ", symbolTable[i].address);
            }
            printf("%d\n", symbolTable[i].mark); // mark, ends the row
        }
        printf("\n");
    }
}

// error 18: four headings with nothing under them, then the error line
void failOpen(void) {
    printf("Name Table:\n\n");
    printf("Parse Trace:\n\n");
    printf("Symbol Table:\n\n");
    printf("Result:\n\n");
    printf("Error 18: %s\n", errorMessages[18]);
    exit(1);
}

// reports error n at token index i (0 based) and stops
// syntax errors pass cur (not consumed), symbol table errors pass cur - 1 (already consumed)
// an index past the last token means "end of tokens"
void fail(int n, int i) {
    printf("\n"); // closes the parse trace section
    printSymbolTable();
    printf("Result:\n\n");
    if(i >= tokenCount) {
        printf("Error %d at end of tokens: %s\n", n, errorMessages[n]);
    } else {
        printf("Error %d at token %d '%s': %s\n", n, i + 1, tokenText(i), errorMessages[n]);
    }
    exit(1);
}

// code of the current token, or 0 when the tokens have run out
// 0 is not a real token code, so comparing against it is always safe
int currentCode(void) {
    if(cur >= tokenCount) {
        return 0;
    }
    return tokens[cur].code;
}

// prints the trace line for the current token, then moves on to the next one
// owner is the nonterminal whose production contains the token
void consume(const char *owner) {
    printf("%d %d", cur + 1, tokens[cur].code);
    if(tokens[cur].code == identsym) {
        printf(" %d", tokens[cur].value);
    } else if(tokens[cur].code == numbersym) {
        printf(" %s", tokens[cur].lexeme);
    }
    printf(" %s %s\n", tokenText(cur), owner);
    cur++;
}

// program -> block "."
void program(void) {
    block();

    if(currentCode() != periodsym) {
        fail(1, cur);
    }
    consume("program");

    // nothing may follow the period
    if(cur < tokenCount) {
        fail(1, cur);
    }
}

// block -> [ const-decl ] [ var-decl ] statement
void block(void) {
    if(currentCode() == constsym) {
        constDeclaration();
    }
    if(currentCode() == varsym) {
        varDeclaration();
    }
    statement();

    // the block ended, so its names are hidden from lookups but stay in the table
    for(int i = 0; i < symbolCount; i++){
        printf("  mark entry %d\n", i);
        symbolTable[i].mark = 1;
    }

    printf("  leave level 0\n");
}

// const-decl -> "const" ident ":=" number { "," ident ":=" number } ";"
void constDeclaration(void) {
    consume("const-decl");

    // one pass per ident := number, a comma after the number starts another pass
    while(1) {
        if(currentCode() != identsym) {
            fail(2, cur);
        }
        consume("const-decl");
        int nameIndex = tokens[cur - 1].value;
        declarationHelper(nameIndex);

        if(currentCode() != initsym) {
            fail(3, cur);
        }
        consume("const-decl");

        if(currentCode() != numbersym) {
            fail(3, cur);
        }
        consume("const-decl");
        int value = atoi(tokens[cur-1].lexeme);
        insertHelper(CONSTANT, nameIndex, value, -1);

        if(currentCode() != commasym) {
            break;
        }
        consume("const-decl");
    }

    if(currentCode() != semicolonsym) {
        fail(4, cur);
    }
    consume("const-decl");
}

// a helper to check duplicate and symbolCount for kind declaration
void declarationHelper(int nameIndex){
    // check for duplicates
    if(isDuplicate(nameIndex)){ // nameIndex is duplicate
        fail(14, cur-1);
    }

    if(symbolCount == MAX_SYMBOL_COUNT){ // symbolCount reached its capacity
        fail(17, cur-1);
    }
}

// a helper to look up a used identifier and check its kind
// mustBeVariable is 1 for assignment and read targets, 0 for a factor
int lookupHelper(int nameIndex, int mustBeVariable){
    int entry = lookup(nameIndex);

    if(entry == -1){ // nameIndex is not declared
        fail(15, cur-1);
    }

    // event line printed right away so it lands right after the identifier's trace line
    if(symbolTable[entry].kind == CONSTANT){
        printf("  lookup %s: entry %d, constant, level %d\n", names[nameIndex].name, entry, symbolTable[entry].level);
    }
    else{
        printf("  lookup %s: entry %d, variable, level %d\n", names[nameIndex].name, entry, symbolTable[entry].level);
    }

    if(mustBeVariable == 1 && symbolTable[entry].kind != VARIABLE){ // only a VARIABLE can be assigned or read
        fail(16, cur-1);
    }

    return entry;
}

// var-decl -> "var" ident { "," ident } ";"
void varDeclaration(void) {
    consume("var-decl");

    if(currentCode() != identsym) {
        fail(2, cur);
    }
    consume("var-decl");
    int nameIndex = tokens[cur - 1].value;
    declarationHelper(nameIndex);
    insertHelper(VARIABLE, nameIndex, -1, nextAddress);

    while(currentCode() == commasym) {
        consume("var-decl");
        if(currentCode() != identsym) {
            fail(2, cur);
        }
        consume("var-decl");
        nameIndex = tokens[cur - 1].value;
        declarationHelper(nameIndex);
        insertHelper(VARIABLE, nameIndex, -1, nextAddress);
    }

    if(currentCode() != semicolonsym) {
        fail(4, cur);
    }
    consume("var-decl");
}

// statement -> [ ident "=" expression | "begin" ... | "if" ... | "while" ... | "read" ident | "write" expression ]
// the first token picks the alternative, any other token is the empty statement
void statement(void) {
    int code = currentCode();

    if(code == identsym) {
        consume("statement");
        // lookup and kind check come before the = check (y20, y22)
        int nameIndex = tokens[cur - 1].value;
        lookupHelper(nameIndex, 1);

        if(currentCode() != assignsym) {
            fail(5, cur);
        }
        consume("statement");
        expression();
    } else if(code == beginsym) {
        consume("statement");
        statementList();
        if(currentCode() != endsym) {
            fail(6, cur);
        }
        consume("statement");
    } else if(code == ifsym) {
        consume("statement");
        condition();
        if(currentCode() != thensym) {
            fail(7, cur);
        }
        consume("statement");
        statementList();
        // else part is optional, fi closes the if either way
        if(currentCode() == elsesym) {
            consume("statement");
            statementList();
        }
        if(currentCode() != fisym) {
            fail(8, cur);
        }
        consume("statement");
    } else if(code == whilesym) {
        consume("statement");
        condition();
        if(currentCode() != thensym) {
            fail(7, cur);
        }
        consume("statement");
        statementList();
        if(currentCode() != elihwsym) {
            fail(8, cur);
        }
        consume("statement");
    } else if(code == readsym) {
        consume("statement");
        if(currentCode() != identsym) {
            fail(2, cur);
        }
        consume("statement");
        int nameIndex = tokens[cur - 1].value;
        lookupHelper(nameIndex, 1);
    } else if(code == writesym) {
        consume("statement");
        expression();
    } else if(code == procsym) {
        fail(12, cur);
    } else if(code == callsym) {
        fail(13, cur);
    }
    // anything else: empty statement, consume nothing
}

// statement-list -> statement { ";" statement }
void statementList(void) {
    statement();
    while(currentCode() == semicolonsym) {
        consume("statement-list");
        statement();
    }
}

// condition -> expression ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) expression
void condition(void) {
    expression();

    // the six relational codes are in a row (eqsym to geqsym)
    // currentCode() is 0 at end of tokens, so that case lands in the error too
    if(currentCode() < eqsym || currentCode() > geqsym) {
        fail(9, cur);
    }
    consume("condition");
    expression();
}

// expression -> [ "+" | "-" ] term { ( "+" | "-" ) term }
void expression(void) {
    if(currentCode() == plussym || currentCode() == minussym) {
        consume("expression");
    }
    term();
    while(currentCode() == plussym || currentCode() == minussym) {
        consume("expression");
        term();
    }
}

// term -> factor { ( "*" | "/" ) factor }
void term(void) {
    factor();
    while(currentCode() == multsym || currentCode() == slashsym) {
        consume("term");
        factor();
    }
}

// factor -> ident | number | "(" expression ")"
void factor(void) {
    if(currentCode() == identsym) {
        consume("factor");
        int nameIndex = tokens[cur - 1].value;
        lookupHelper(nameIndex, 0); // a CONSTANT or a VARIABLE can both be in an expression
    } else if(currentCode() == numbersym) {
        consume("factor");
    } else if(currentCode() == lparentsym) {
        consume("factor");
        expression();
        if(currentCode() != rparentsym) {
            fail(10, cur);
        }
        consume("factor");
    } else {
        fail(11, cur);
    }
}

int isDuplicate(int nameTableIndex){ // check if the name table index is duplicate
    int lookupValue = lookup(nameTableIndex);

    if(lookupValue == -1){ // not duplicate
        return 0;
    }

    return 1; // duplicate
}

int lookup(int nameTableIndex){ // search the symbol table for the identifier that was just consumed
    for(int i = 0; i < symbolCount; i++){
        if(symbolTable[i].nameIndex == nameTableIndex && symbolTable[i].mark == 0){
            return i;
        }
    } 

    return -1; // nothing is matched
}

void insertHelper(int kind, int nameIndex, int value, int address){
    symbolTable[symbolCount].kind = kind;
    symbolTable[symbolCount].nameIndex = nameIndex;
    symbolTable[symbolCount].value = value;
    symbolTable[symbolCount].address = address;
    symbolTable[symbolCount].level = 0;
    symbolTable[symbolCount].mark = 0;

    // event line printed right away so it lands between the right trace lines
    // a constant has no address and a variable has no value, so each kind prints a different shape
    if(kind == CONSTANT){
        printf("  insert entry %d: constant %s, value %d, level 0\n", symbolCount, names[nameIndex].name, value);
    }
    else{
        printf("  insert entry %d: variable %s, level 0, address %d\n", symbolCount, names[nameIndex].name, address);
        nextAddress++; // if the kind is VARIABLE, then increase nextAddress by 1
    }

    symbolCount++;
}