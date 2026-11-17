/*
Homework:
parsercodegen - HW4 PL/0 compiler: parser, symbol table, and code generator

Author(s): Ethan Perez-Rivera

Language: C only

To Compile:
  gcc -Wall -Wextra -std=c11 -O2 parsercodegen.c -o parsercodegen

To Execute (on Eustis):
  ./lex <input_file> && ./parsercodegen && ./vm elf.txt

where:
  <input_file> is the path to a text file holding a PL/0 source program;
  ./lex writes tokens.txt and nametable.txt, ./parsercodegen reads them
  from the current directory, takes no arguments, and writes elf.txt, and
  ./vm runs elf.txt

Notes:
  - Implements the recursive-descent compiler described in the homework
    instructions: procedures, lexical levels, and PM/0 code generation.
  - Prints three sections to standard output: Generated Code, Symbol
    Table and Result.
  - Writes elf.txt, one instruction per line as OP L M, only when the
    whole program is accepted, and then exits with status 0.
  - Stops at the first error, reports it, and exits with status 1.
  - Tested on Eustis.

Class: COP 3402 - Systems Software

Instructor: Jie Lin, Ph.D.

Due Date: See Webcourses
*/

#include <stdio.h>
#include <stdlib.h>

// limits from the handout
#define MAX_TOKENS 5000
#define MAX_NAMES 200
#define MAX_IDENT_LEN 12
#define MAX_NUM_LEN 6
#define MAX_SYMBOL_COUNT 100

// kinds
#define CONSTANT 1
#define VARIABLE 2
#define PROCEDURE 3

// how a name is used
#define USE_TARGET 1 // assign/read target: variable (err 16)
#define USE_CALL 2   // call: procedure (err 13)
#define USE_VALUE 3  // factor: not a procedure (err 19)

#define MAX_CODE 200

// ops (table 3)
#define LIT 1
#define OPR 2
#define LOD 3
#define STO 4
#define CAL 5
#define INC 6
#define JMP 7
#define JPC 8
#define SYS 9

// opr m values (table 4)
#define RTN 0
#define ADD 1
#define SUB 2
#define MUL 3
#define DIV 4
#define EQL 5
#define NEQ 6
#define LSS 7
#define LEQ 8
#define GTR 9
#define GEQ 10

// token codes, copied from lex.c
typedef enum{
    identsym = 1, numbersym,

    plussym = 3, minussym, multsym, slashsym, eqsym, neqsym, lessym, leqsym,
    gtrsym, geqsym, lparentsym, rparentsym, commasym, semicolonsym,
    periodsym, assignsym, initsym,

    beginsym = 20, endsym, ifsym, fisym, thensym, whilesym, elihwsym, dosym,
    odsym, oddsym, callsym, constsym, varsym, procsym, writesym, readsym,
    elsesym
}TokenType;

// value: name table index for idents
// lexeme: digits as written, keeps leading zeros
typedef struct{
    int code;
    int value;
    char lexeme[MAX_IDENT_LEN + 1];
}Token;

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

typedef struct{
    int op;
    int l;
    int m;
}Instruction;

// globals
Token tokens[MAX_TOKENS];
int tokenCount = 0;
NameEntry names[MAX_NAMES];
int nameCount = 0;
SymbolEntry symbolTable[MAX_SYMBOL_COUNT];
int symbolCount = 0;
int nextAddress = 3; // 0-2 are sl, dl, ra
int cur = 0; // current token index
Instruction code[MAX_CODE];
int codeCount = 0;

// token text by code
const char *spellings[37] = {
    "", "", "",
    "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">=", "(", ")", ",", ";", ".", "=", ":=",
    "begin", "end", "if", "fi", "then", "while", "elihw", "do", "od", "odd",
    "call", "const", "var", "procedure", "write", "read", "else"
};

// error msgs (table 5)
const char *errorMessages[21] = {
    "",
    "the program must end with a period, and nothing may follow it",
    "const, var, procedure, call, and read must be followed by an identifier",
    "a constant must be declared as name := number",
    "declarations must be separated by , and end with ;",
    "an identifier that starts a statement must be followed by =",
    "begin must be closed with end",
    "a condition after if or while must be followed by then",
    "if must be closed with fi, and while with elihw",
    "a condition must contain one of == != < <= > >=",
    "a left parenthesis must be closed with a right parenthesis",
    "a factor must be an identifier, a number, or (",
    "a procedure name and a procedure block must each be followed by ;",
    "call must name a procedure",
    "identifier already declared in this block",
    "undeclared identifier",
    "only a variable may be assigned or read",
    "symbol table is full",
    "cannot open tokens.txt or nametable.txt",
    "a procedure name cannot be used in an expression",
    "generated code exceeds 200 instructions"
};

const char *opNames[10] = {
    "", "LIT", "OPR", "LOD", "STO", "CAL", "INC", "JMP", "JPC", "SYS"
};

const char *oprNames[11] = {
    "RTN", "ADD", "SUB", "MUL", "DIV", "EQL", "NEQ", "LSS", "LEQ", "GTR", "GEQ"
};

void loadNames(FILE *nf);
void loadTokens(FILE *tf);
const char *tokenText(int i);
void printSymbolTable(void);
void printCode(void);
void writeElf(void);
void fail(int n, int i);
int currentCode(void);
void consume(void);
void program(void);
void block(int level);
void constDeclaration(int level);
void procDeclaration(int level);
void varDeclaration(int level);
void statement(int level);
void statementList(int level);
void condition(int level);
void expression(int level);
void term(int level);
void factor(int level);
int isDuplicate(int nameTableIndex, int level);
int lookup(int nameTableIndex);
void insertHelper(int kind, int nameIndex, int value, int address, int level);
void declarationHelper(int nameIndex, int level);
int lookupHelper(int nameIndex, int use);
int nextCodeAddress(void);
int emit(int op, int l, int m);
void patch(int k);

int main(int argc, char *argv[]) {
    if(argc != 1) {
        printf("Usage: %s\n", argv[0]);
        return 1;
    }

    // open both files before printing
    FILE *tf = fopen("tokens.txt", "r");
    FILE *nf = fopen("nametable.txt", "r");
    if(!tf || !nf) {
        if(tf) fclose(tf);
        if(nf) fclose(nf);
        fail(18, -1);
    }

    loadNames(nf);
    loadTokens(tf);
    fclose(tf);
    fclose(nf);

    // errors exit inside program()
    program();

    // accepted: print sections, write elf
    printCode();
    printSymbolTable();
    printf("Result:\n\n");
    printf("program accepted\n");
    writeElf();
    return 0;
}

// reads nametable.txt: index name line col
void loadNames(FILE *nf) {
    int idx;
    while(nameCount < MAX_NAMES && fscanf(nf, "%d %s %d %d", &idx, names[nameCount].name, &names[nameCount].line, &names[nameCount].col) == 4) {
        nameCount++;
    }
}

// reads tokens.txt: code, plus a 2nd field for idents and numbers
void loadTokens(FILE *tf) {
    while(tokenCount < MAX_TOKENS && fscanf(tf, "%d", &tokens[tokenCount].code) == 1) {
        tokens[tokenCount].value = 0;
        tokens[tokenCount].lexeme[0] = '\0';

        if(tokens[tokenCount].code == identsym) {
            // name table index
            if(fscanf(tf, "%d", &tokens[tokenCount].value) != 1) {
                break;
            }
        } else if(tokens[tokenCount].code == numbersym) {
            // digits as text, keeps leading zeros
            if(fscanf(tf, "%s", tokens[tokenCount].lexeme) != 1) {
                break;
            }
        }
        tokenCount++;
    }
}

// text of token i (0 based)
const char *tokenText(int i) {
    if(tokens[i].code == identsym) {
        return names[tokens[i].value].name;
    }
    if(tokens[i].code == numbersym) {
        return tokens[i].lexeme;
    }
    return spellings[tokens[i].code];
}

void printSymbolTable(void) {
    printf("Symbol Table:\n\n");
    if(symbolCount > 0){
        printf("entry kind name value level address mark\n");
        for(int i = 0; i < symbolCount; i++){
            printf("%d ", i);
            if(symbolTable[i].kind == CONSTANT){
                printf("constant ");
            }
            else if(symbolTable[i].kind == VARIABLE){
                printf("variable ");
            }
            else{
                printf("procedure ");
            }
            printf("%s ", names[symbolTable[i].nameIndex].name);

            // -1 prints as a dash
            if(symbolTable[i].value == -1){
                printf("- ");
            }
            else{
                printf("%d ", symbolTable[i].value);
            }
            printf("%d ", symbolTable[i].level);
            if(symbolTable[i].address == -1){
                printf("- ");
            }
            else{
                printf("%d ", symbolTable[i].address);
            }
            printf("%d\n", symbolTable[i].mark);
        }
        printf("\n");
    }
}

void printCode(void) {
    printf("Generated Code:\n\n");
    if(codeCount > 0) {
        printf("index address op L M\n");
        for(int k = 0; k < codeCount; k++) {
            printf("%d %d ", k, 200 + 3 * k);
            // opr prints its sub-op name
            if(code[k].op == OPR) {
                printf("%s ", oprNames[code[k].m]);
            } else {
                printf("%s ", opNames[code[k].op]);
            }
            printf("%d %d\n", code[k].l, code[k].m);
        }
        printf("\n");
    }
}

// writes elf.txt, numbers only
void writeElf(void) {
    FILE *elf = fopen("elf.txt", "w");
    if(elf == NULL) {
        return;
    }
    for(int k = 0; k < codeCount; k++) {
        fprintf(elf, "%d %d %d\n", code[k].op, code[k].l, code[k].m);
    }
    fclose(elf);
}

// reports error n and exits
// i: token index, cur for syntax errors, cur - 1 for symbol table errors
// i >= tokenCount: end of tokens, i < 0: no token
void fail(int n, int i) {
    printf("Generated Code:\n\n");
    printf("Symbol Table:\n\n");
    printf("Result:\n\n");
    if(i < 0) {
        printf("Error %d: %s\n", n, errorMessages[n]);
    } else if(i >= tokenCount) {
        printf("Error %d at end of tokens: %s\n", n, errorMessages[n]);
    } else {
        printf("Error %d at token %d '%s': %s\n", n, i + 1, tokenText(i), errorMessages[n]);
    }
    exit(1);
}

// current token code, 0 at end of tokens
int currentCode(void) {
    if(cur >= tokenCount) {
        return 0;
    }
    return tokens[cur].code;
}

void consume(void) {
    cur++;
}

// address of the next instr: 200 + 3k
int nextCodeAddress(void) {
    return 200 + 3 * codeCount;
}

// appends an instr, returns its index
int emit(int op, int l, int m) {
    if(codeCount >= MAX_CODE) {
        fail(20, -1);
    }
    code[codeCount].op = op;
    code[codeCount].l = l;
    code[codeCount].m = m;
    codeCount++;
    return codeCount - 1;
}

// sets m of instr k to the next address
void patch(int k) {
    code[k].m = nextCodeAddress();
}

// program -> block "."
void program(void) {
    block(0);

    if(currentCode() != periodsym) {
        fail(1, cur);
    }
    consume();
    emit(SYS, 0, 3); // halt

    // nothing may follow the period
    if(cur < tokenCount) {
        fail(1, cur);
    }
}

// block -> [ const-decl ] [ var-decl ] { proc-decl } statement
void block(int level) {
    int start = symbolCount;         // this block's entries start here
    int savedAddress = nextAddress;  // outer block's next address
    nextAddress = 3;                 // vars start at 3

    // opening jmp over the procs, patched below
    int jmp = emit(JMP, 0, 0);

    if(currentCode() == constsym) {
        constDeclaration(level);
    }
    if(currentCode() == varsym) {
        varDeclaration(level);
    }
    while(currentCode() == procsym) {
        procDeclaration(level);
    }

    patch(jmp);                // jmp lands on the inc
    emit(INC, 0, nextAddress); // 3 + vars, consts take no slot

    statement(level);

    // block ended: mark its entries
    for(int i = start; i < symbolCount; i++){
        if(symbolTable[i].level == level && symbolTable[i].mark == 0){
            symbolTable[i].mark = 1;
        }
    }

    nextAddress = savedAddress;
}

// const-decl -> "const" ident ":=" number { "," ident ":=" number } ";"
void constDeclaration(int level) {
    consume();

    while(1) {
        if(currentCode() != identsym) {
            fail(2, cur);
        }
        consume();
        int nameIndex = tokens[cur - 1].value;
        declarationHelper(nameIndex, level);

        if(currentCode() != initsym) {
            fail(3, cur);
        }
        consume();

        if(currentCode() != numbersym) {
            fail(3, cur);
        }
        consume();
        int value = atoi(tokens[cur-1].lexeme);
        insertHelper(CONSTANT, nameIndex, value, -1, level);

        if(currentCode() != commasym) {
            break;
        }
        consume();
    }

    if(currentCode() != semicolonsym) {
        fail(4, cur);
    }
    consume();
}

// proc-decl -> "procedure" ident ";" block ";"
void procDeclaration(int level) {
    consume();

    if(currentCode() != identsym) {
        fail(2, cur);
    }
    consume();
    int nameIndex = tokens[cur - 1].value;
    declarationHelper(nameIndex, level);
    // insert before the block (recursion), address = its opening jmp
    insertHelper(PROCEDURE, nameIndex, -1, nextCodeAddress(), level);

    if(currentCode() != semicolonsym) {
        fail(12, cur);
    }
    consume();

    block(level + 1); // body is one level deeper

    if(currentCode() != semicolonsym) {
        fail(12, cur);
    }
    consume();
    emit(OPR, 0, RTN); // return after the closing ;
}

// duplicate check (14), then capacity (17)
void declarationHelper(int nameIndex, int level){
    if(isDuplicate(nameIndex, level)){
        fail(14, cur-1);
    }

    if(symbolCount == MAX_SYMBOL_COUNT){
        fail(17, cur-1);
    }
}

// lookup (15), then kind check by use
int lookupHelper(int nameIndex, int use){
    int entry = lookup(nameIndex);

    if(entry == -1){
        fail(15, cur-1);
    }

    if(use == USE_TARGET && symbolTable[entry].kind != VARIABLE){
        fail(16, cur-1);
    }
    if(use == USE_CALL && symbolTable[entry].kind != PROCEDURE){
        fail(13, cur-1);
    }
    if(use == USE_VALUE && symbolTable[entry].kind == PROCEDURE){
        fail(19, cur-1);
    }

    return entry;
}

// var-decl -> "var" ident { "," ident } ";"
void varDeclaration(int level) {
    consume();

    if(currentCode() != identsym) {
        fail(2, cur);
    }
    consume();
    int nameIndex = tokens[cur - 1].value;
    declarationHelper(nameIndex, level);
    insertHelper(VARIABLE, nameIndex, -1, nextAddress, level);

    while(currentCode() == commasym) {
        consume();
        if(currentCode() != identsym) {
            fail(2, cur);
        }
        consume();
        nameIndex = tokens[cur - 1].value;
        declarationHelper(nameIndex, level);
        insertHelper(VARIABLE, nameIndex, -1, nextAddress, level);
    }

    if(currentCode() != semicolonsym) {
        fail(4, cur);
    }
    consume();
}

// statement -> [ ident "=" expression | "call" ident | "begin" ... | "if" ... | "while" ... | "read" ident | "write" expression ]
void statement(int level) {
    int firstCode = currentCode();

    if(firstCode == identsym) {
        consume();
        // lookup before checking =
        int nameIndex = tokens[cur - 1].value;
        int entry = lookupHelper(nameIndex, USE_TARGET); // sto needs it after the expression

        if(currentCode() != assignsym) {
            fail(5, cur);
        }
        consume();
        expression(level);
        emit(STO, level - symbolTable[entry].level, symbolTable[entry].address);
    } else if(firstCode == beginsym) {
        consume();
        statementList(level);
        if(currentCode() != endsym) {
            fail(6, cur);
        }
        consume();
    } else if(firstCode == ifsym) {
        consume();
        condition(level);
        if(currentCode() != thensym) {
            fail(7, cur);
        }
        consume();
        int jpc = emit(JPC, 0, 0); // skips then list when false, patched later
        statementList(level);

        if(currentCode() == elsesym) {
            int jmp = emit(JMP, 0, 0); // skips the else list
            patch(jpc);                // after the jmp, so jpc lands on else
            consume();
            statementList(level);
            if(currentCode() != fisym) {
                fail(8, cur);
            }
            consume();
            patch(jmp);                // lands after else list
        } else {
            if(currentCode() != fisym) {
                fail(8, cur);
            }
            consume();
            patch(jpc);                // lands after then list
        }
    } else if(firstCode == whilesym) {
        consume();
        int loopStart = nextCodeAddress(); // saved before the condition emits
        condition(level);
        if(currentCode() != thensym) {
            fail(7, cur);
        }
        consume();
        int jpc = emit(JPC, 0, 0); // exits when false, patched later
        statementList(level);
        if(currentCode() != elihwsym) {
            fail(8, cur);
        }
        consume();
        emit(JMP, 0, loopStart);   // back to the condition, before the patch
        patch(jpc);                // exit lands after the jmp
    } else if(firstCode == readsym) {
        consume();
        if(currentCode() != identsym) {
            fail(2, cur);
        }
        consume();
        int nameIndex = tokens[cur - 1].value;
        int entry = lookupHelper(nameIndex, USE_TARGET);
        emit(SYS, 0, 2);
        emit(STO, level - symbolTable[entry].level, symbolTable[entry].address);
    } else if(firstCode == writesym) {
        consume();
        expression(level);
        emit(SYS, 0, 1);
    } else if(firstCode == callsym) {
        consume();
        if(currentCode() != identsym) {
            fail(2, cur);
        }
        consume();
        int nameIndex = tokens[cur - 1].value;
        int entry = lookupHelper(nameIndex, USE_CALL);
        emit(CAL, level - symbolTable[entry].level, symbolTable[entry].address);
    }
    // anything else: empty statement
}

// statement-list -> statement { ";" statement }
void statementList(int level) {
    statement(level);
    while(currentCode() == semicolonsym) {
        consume();
        statement(level);
    }
}

// condition -> expression ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) expression
void condition(int level) {
    expression(level);

    // relational codes are consecutive (eqsym..geqsym)
    if(currentCode() < eqsym || currentCode() > geqsym) {
        fail(9, cur);
    }
    int op = currentCode();
    consume();
    expression(level);

    // eqsym..geqsym and eql..geq share the same order
    emit(OPR, 0, EQL + (op - eqsym));
}

// expression -> [ "+" | "-" ] term { ( "+" | "-" ) term }
void expression(int level) {
    int negate = 0;

    if(currentCode() == plussym || currentCode() == minussym) {
        if(currentCode() == minussym) {
            negate = 1;
        }
        consume();
    }

    // leading minus is 0 - term
    if(negate) {
        emit(LIT, 0, 0);
    }
    term(level);
    if(negate) {
        emit(OPR, 0, SUB);
    }

    while(currentCode() == plussym || currentCode() == minussym) {
        int op = currentCode();
        consume();
        term(level);
        if(op == plussym) {
            emit(OPR, 0, ADD);
        } else {
            emit(OPR, 0, SUB);
        }
    }
}

// term -> factor { ( "*" | "/" ) factor }
void term(int level) {
    factor(level);
    while(currentCode() == multsym || currentCode() == slashsym) {
        int op = currentCode();
        consume();
        factor(level);
        if(op == multsym) {
            emit(OPR, 0, MUL);
        } else {
            emit(OPR, 0, DIV);
        }
    }
}

// factor -> ident | number | "(" expression ")"
void factor(int level) {
    if(currentCode() == identsym) {
        consume();
        int nameIndex = tokens[cur - 1].value;
        int entry = lookupHelper(nameIndex, USE_VALUE);
        if(symbolTable[entry].kind == CONSTANT) {
            emit(LIT, 0, symbolTable[entry].value); // consts are lit values
        } else {
            emit(LOD, level - symbolTable[entry].level, symbolTable[entry].address);
        }
    } else if(currentCode() == numbersym) {
        consume();
        emit(LIT, 0, atoi(tokens[cur - 1].lexeme));
    } else if(currentCode() == lparentsym) {
        consume();
        expression(level);
        if(currentCode() != rparentsym) {
            fail(10, cur);
        }
        consume();
    } else {
        fail(11, cur);
    }
}

// duplicate: unmarked entry, same name, same level
// other levels are shadowed, not duplicates
int isDuplicate(int nameTableIndex, int level){
    for(int i = 0; i < symbolCount; i++){
        if(symbolTable[i].nameIndex == nameTableIndex && symbolTable[i].level == level && symbolTable[i].mark == 0){
            return 1;
        }
    }

    return 0;
}

// newest to oldest, first unmarked match
int lookup(int nameTableIndex){
    for(int i = symbolCount - 1; i >= 0; i--){
        if(symbolTable[i].nameIndex == nameTableIndex && symbolTable[i].mark == 0){
            return i;
        }
    }

    return -1;
}

void insertHelper(int kind, int nameIndex, int value, int address, int level){
    symbolTable[symbolCount].kind = kind;
    symbolTable[symbolCount].nameIndex = nameIndex;
    symbolTable[symbolCount].value = value;
    symbolTable[symbolCount].address = address;
    symbolTable[symbolCount].level = level;
    symbolTable[symbolCount].mark = 0;

    if(kind == VARIABLE)
        nextAddress++; // vars take a slot, consts don't

    symbolCount++;
}