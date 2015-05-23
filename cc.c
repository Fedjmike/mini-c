#include "stdlib.h"
#include "string.h"
#include "ctype.h"
#include "stdio.h"



char* strdup (char* str) {
    return strcpy(malloc(strlen(str)+1), str);
}



char* inputname;
FILE* input;

char curch;

char* buffer;
int buflength;
int token;

int token_other = 0;
int token_ident = 1;

void next_char () {
    curch = fgetc(input);
}

void eat_char () {
    buffer[buflength++] = curch;
    next_char();
}

void next () {
    while (curch == ' ' || curch == '\r' || curch == '\n' || curch == '\t')
        next_char();

    if (curch == '#') {
        next_char();

        while (curch != '\n' && !feof(input))
            next_char();

        next();
        return;
    }

    buflength = 0;
    token = token_other;

    if (isalpha(curch)) {
        token = token_ident;
        eat_char();

        while ((isalnum(curch) || curch == '_') && !feof(input))
            eat_char();

    } else
        eat_char();

    buffer[buflength++] = '\0';
}

void lex_init (char* filename) {
    inputname = strdup(filename);
    input = fopen(filename, "r");

    buffer = malloc(256);
    next_char();
    next();
}



int errors;

void error () {
    printf("%s: error: ", inputname);
    errors++;
}

int see (char* look) {
    return !strcmp(buffer, look);
}

void accept () {
    printf("%s ", buffer);
    next();
}

void match (char* look) {
    if (!see(look))
        error();
        printf("expected %s, found %s\n", look, buffer);
        errors++;

    accept();
}

int try_match (char* look) {
    if (!see(look)) {
        accept();
        return 1;

    } else
        return 0;
}

void decl () {
    accept();

    int ptr = 0;

    while (try_match("*"))
        ptr++;

    char* ident = strdup(buffer);
    puts(ident);
}

void program () {
    errors = 0;

    while (!feof(input)) {
        if (!see(";"))
            decl();

        match(";");
    }
}



int main (int argc, char** argv) {
    if (argc != 2) {
        puts("Usage: cc <file>");
        return 1;
    }

    lex_init(argv[1]);

    program();

    return 0;
}
