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
int peekChar(const char *buf, size_t bytesRead, size_t i, int offset);
void advance(const char *buf, size_t *i, int *line, int *col);
void setError(int *errCode, int *errLine, int *errCol, char *errMsg, int code, int line, int col, const char* msg);
void printError(int errCode, int errLine, int errCol, const char* errMsg);

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

    size_t i = 0;   // index into buf
    int line = 1;   // current line number
    int col = 1;    // current column number

    // first lexical error found, errCode stays 0 if the whole file scans
    int errCode = 0, errLine = 0, errCol = 0;
    char errMsg[300];
    char msg[300];      // scratch buffer for messages that include the lexeme

    // scanning loop: skip whitespace, then read one token (or skip one comment) per pass
    // stops at the end of buf or at the first error, so nothing after an error is scanned
    while (i < bytesRead && errCode == 0) {
        // skip any run of whitespace
        while (i < bytesRead && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\r' || buf[i] == '\n')) {
            advance(buf, &i, &line, &col);
        }

        // if EOF break
        if (i >= bytesRead) {
            break;
        }

        // save position of token
        int startLine = line;
        int startCol = col;

        char c = buf[i];

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            // collect the whole run: first char is a letter, then any letters/digits follow
            char run[256];
            int len = 0;

            while (i < bytesRead) {
                char cur = buf[i];
                int isLetter = (cur >= 'a' && cur <= 'z') || (cur >= 'A' && cur <= 'Z');
                int isDigit  = (cur >= '0' && cur <= '9');
                if (!isLetter && !isDigit) {
                    break;   // run has ended, stop consuming
                }
                if (len < 255) {          // guard overflow in run[]
                    run[len] = cur;
                }
                len++;
                advance(buf, &i, &line, &col);
            }
            run[(len < 255) ? len : 255] = '\0';    // Put null terminator at end of run

            if(len > MAX_IDENT_LEN) {
                // error 2 names the whole run
                snprintf(msg, sizeof(msg), "identifier too long '%s'", run);
                setError(&errCode, &errLine, &errCol, errMsg, 2, startLine, startCol, msg);
            } else {
                int match = 0;
                for(int j = 0; j < numReserved; j++) {
                    if(strcmp(run, reservedWords[j].word) == 0) {
                        addToken(reservedWords[j].code, 0, run);
                        match = 1;
                        break;
                    }
                }
                if(!match) {
                    int idx = findOrAddName(run, startLine, startCol);
                    addToken(identsym, idx, run);
                }
            }
        } else if (c >= '0' && c <= '9') {
            char run[256];
            int len = 0;

            while (i < bytesRead && buf[i] >= '0' && buf[i] <= '9') {
                if (len < 255) {
                    run[len] = buf[i];
                }
                len++;
                advance(buf, &i, &line, &col);
            }
            run[(len < 255) ? len : 255] = '\0';

            int nextIsLetter = (i < bytesRead) && ((buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= 'A' && buf[i] <= 'Z'));

            if (nextIsLetter) {
                // error 6, keep consuming the rest of the alphanumeric run so the message has all of it
                // checked before the length, so 1234567abc is error 6 and not error 3
                while (i < bytesRead) {
                    char cur = buf[i];
                    int isLetter = (cur >= 'a' && cur <= 'z') || (cur >= 'A' && cur <= 'Z');
                    int isDigit  = (cur >= '0' && cur <= '9');
                    if (!isLetter && !isDigit)  {
                        break;
                    }
                    if (len < 255) {
                        run[len] = cur;
                    }
                    len++;
                    advance(buf, &i, &line, &col);
                }
                run[(len < 255) ? len : 255] = '\0';
                snprintf(msg, sizeof(msg), "number followed by a letter '%s'", run);
                setError(&errCode, &errLine, &errCol, errMsg, 6, startLine, startCol, msg);
            } else {
                run[(len < 255) ? len : 255] = '\0';
                if (len > MAX_NUM_LEN) {
                    // error 3, leading zeros count as digits (0000001 is too long)
                    snprintf(msg, sizeof(msg), "number too long '%s'", run);
                    setError(&errCode, &errLine, &errCol, errMsg, 3, startLine, startCol, msg);
                } else {
                    addToken(numbersym, 0, run);
                }
            }
        } 
        else if (c == '/' && peekChar(buf, bytesRead, i, 1) == '*'){
            // comment skipper: everything from /* to the next */ is discarded, no token
            // startLine/startCol hold the opener's position, which is where error 7 is reported

            // consume both chars of the opener, so the '/' in /*/ can't be reused to close it
            advance(buf, &i, &line, &col);
            advance(buf, &i, &line, &col);

            // local flag, starts at 0 for every comment so a closed comment leaves no trace
            int closed = 0;

            while (i < bytesRead){
                int next = peekChar(buf, bytesRead, i, 1);

                // close only on '*' immediately followed by '/', and consume both
                // "* /" or a '*' ending one line with '/' starting the next doesn't close
                if (buf[i] == '*' && next == '/'){
                    advance(buf, &i, &line, &col);
                    advance(buf, &i, &line, &col);
                    closed = 1;
                    break;
                }

                // comments don't nest
                // error 9 is at the inner '/', break so the main loop sees errCode and stops
                if (buf[i] == '/' && next == '*'){
                    setError(&errCode, &errLine, &errCol, errMsg, 9, line, col, "'/*' inside a comment");
                    break;
                }

                // anything else is comment text and isn't examined (weird bytes, @, etc.)
                // advance still counts lines & columns inside the comment
                advance(buf, &i, &line, &col);
            }

            // hit EOF before the close, error 7 is at the opener not EOF
            // (errCode check so an error 9 above isn't reported as error 7)
            if (!closed && errCode == 0){
                setError(&errCode, &errLine, &errCol, errMsg, 7, startLine, startCol, "comment is not closed before end of file");
            }
        } else {
            // operators & punctuation
            // peek at the char after the one we're holding before consuming anything
            // next = -1 at EOF, so a file can end right after an operator without reading past buf
            int next = peekChar(buf, bytesRead, i, 1);
            int code = 0;   // stays 0 if c doesn't start a token
            int len = 1;    // becomes 2 only when the longer operator matches

            switch (c){
                // single char symbols, no lookahead needed
                case '+': code = plussym; break;
                case '-': code = minussym; break;
                case '(': code = lparentsym; break;
                case ')': code = rparentsym; break;
                case ',': code = commasym; break;
                case ';': code = semicolonsym; break;
                case '.': code = periodsym; break;

                // '*' right before '/' outside a comment closes a comment that was never opened
                // (b*/c is error 8 too, a space is needed to multiply then divide)
                case '*':
                    if (next == '/'){
                        setError(&errCode, &errLine, &errCol, errMsg, 8, startLine, startCol, "'*/' without a matching '/*'");
                    }
                    else{
                        code = multsym;
                    }
                    break;

                // "/*" was already taken by the comment skipper above, so this '/' is division
                case '/': code = slashsym; break;

                // longest match: if next is '=' take the 2 char operator
                // alone, =, < and > are still complete tokens
                case '=':
                    if (next == '='){
                        code = eqsym;
                        len = 2;
                    } else{
                        code = assignsym;
                    }
                    break;

                case '<':
                    if (next == '='){
                        code = leqsym;
                        len = 2;
                    } else{
                        code = lessym;
                    }
                    break;

                case '>':
                    if (next == '='){
                        code = geqsym;
                        len = 2;
                    } else{
                        code = gtrsym;
                    }
                    break;

                // alone, ! and : aren't tokens, so a missing '=' is an error instead
                case '!':
                    if (next == '='){
                        code = neqsym;
                        len = 2;
                    } else{
                        setError(&errCode, &errLine, &errCol, errMsg, 5, startLine, startCol, "'!' must be followed by '='");
                    }
                    break;

                case ':':
                    if (next == '='){
                        code = initsym;
                        len = 2;
                    } else{
                        setError(&errCode, &errLine, &errCol, errMsg, 4, startLine, startCol, "':' must be followed by '='");
                    }
                    break;

                // c doesn't start any token
                default: {
                    // cast so bytes above 0x7F compare & print as 0x80-0xFF, not negative
                    unsigned char uc = (unsigned char)c;

                    if (uc >= 0x20 && uc <= 0x7E){
                        // error 1: a printable char this language doesn't use (@, #, $, _, ...)
                        snprintf(msg, sizeof(msg), "invalid character '%c'", c);
                        setError(&errCode, &errLine, &errCol, errMsg, 1, startLine, startCol, msg);
                    } else{
                        // error 10: anything else (0x00, control bytes, 0x7F, non-ASCII)
                        // print the byte's value, never the byte itself
                        snprintf(msg, sizeof(msg), "byte 0x%02X is not part of this language", uc);
                        setError(&errCode, &errLine, &errCol, errMsg, 10, startLine, startCol, msg);
                    }
                    break;
                }
            }

            // lexeme is the held char, plus the '=' only if the pair matched
            char lexeme[3] = {c, '\0', '\0'};
            if (len == 2){
                lexeme[1] = (char)next;
            }

            // consume exactly the chars the token used, never the peeked char unless it matched
            // (on an error nothing is added and the main loop stops on errCode)
            for (int k = 0; k < len; k++){
                advance(buf, &i, &line, &col);
            }

            if (code != 0){
                addToken(code, 0, lexeme);
            }
        }
    }

    //PRINT RESULTS

    // Source Program
    printf("Source Program:\n\n");
    fwrite(buf, 1, bytesRead, stdout);
    if (bytesRead == 0 || buf[bytesRead - 1] != '\n') {
        printf("\n");
    }
    printf("\n");

    // Lexeme Table
    printf("Lexeme Table:\n\n");
    printf("lexeme\ttoken\n");
    for (int t = 0; t < tokenCount; t++) {
        printf("%s\t%d\n", tokens[t].lexeme, tokens[t].code);
    }
    printf("\n");

    // Name Table
    printf("Name Table:\n\n");
    printf("index\tname\tline\tcolumn\n");
    for (int n = 0; n < nameCount; n++) {
        printf("%d\t%s\t%d\t%d\n", n, names[n].name, names[n].line, names[n].col);
    }
    printf("\n");

    // Token List
    printf("Token List:\n\n");
    for (int t = 0; t < tokenCount; t++) {
        if (tokens[t].code == identsym) {
            printf("%d %d", tokens[t].code, tokens[t].value);
        } else if (tokens[t].code == numbersym) {
            printf("%d %s", tokens[t].code, tokens[t].lexeme);
        } else {
            printf("%d", tokens[t].code);
        }
        if (t < tokenCount - 1) printf(" ");
    }
    printf("\n");

    // error 11: no tokens in the source program (empty, only whitespace, or only a comment)
    // always at line 1, column 1; setError ignores it if an earlier error was already found
    if (tokenCount == 0) {
        setError(&errCode, &errLine, &errCol, errMsg, 11, 1, 1, "no tokens in the source program");
    }

    if(errCode != 0)
        printError(errCode, errLine, errCol, errMsg);

    // WRITE OUTPUT FILES

    // tokens.txt
    FILE *tokOut = fopen("tokens.txt", "w");
    if (tokOut != NULL) {
        for (int t = 0; t < tokenCount; t++) {
            if (tokens[t].code == identsym) {
                fprintf(tokOut, "%d %d\n", tokens[t].code, tokens[t].value);
            } else if (tokens[t].code == numbersym) {
                fprintf(tokOut, "%d %s\n", tokens[t].code, tokens[t].lexeme);
            } else {
                fprintf(tokOut, "%d\n", tokens[t].code);
            }
        }
        fclose(tokOut);
    }

    // nametable.txt
    FILE *nameOut = fopen("nametable.txt", "w");
    if (nameOut != NULL) {
        for (int n = 0; n < nameCount; n++) {
            fprintf(nameOut, "%d %s %d %d\n", n, names[n].name, names[n].line, names[n].col);
        }
        fclose(nameOut);
    }

    // Close file
    fclose(fp);

    // Free buf, token list and name table
    free(buf);
    free(tokens);
    free(names);

    // exit status: non-zero on any lexical error, 0 only if the whole file scanned
    if(errCode != 0){
        return 1;
    }
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

// Returns char at buf[i + offset], or -1 if s past EOF
int peekChar(const char *buf, size_t bytesRead, size_t i, int offset) {
    size_t target = i + (size_t)offset;
    if (target >= bytesRead) {
        return -1;
    }
    return (unsigned char)buf[target];
}

void advance(const char *buf, size_t *i, int *line, int *col) {
    char c = buf[*i];

    if (c == '\n') {
        (*line)++;
        (*col) = 1;
    } else if (c == '\r') {
    } else {
        (*col)++;
    }

    (*i)++;
}

// records an error, but only the first one: once *errCode is set, later calls do nothing
void setError(int *errCode, int *errLine, int *errCol, char* errMsg, int code, int line, int col, const char* msg){
    if(*errCode != 0)
        return;

    (*errCode) = code;
    (*errLine) = line;
    (*errCol) = col;
    strcpy(errMsg, msg);
}

void printError(int errCode, int errLine, int errCol, const char* errMsg){
    printf("Error %d at line %d, column %d: %s\n", errCode, errLine, errCol, errMsg);
}