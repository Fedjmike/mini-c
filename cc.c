#include "stdlib.h"
#include "string.h"
#include "ctype.h"
#include "stdio.h"



char* strdup (char* str) {
    return strcpy(malloc(strlen(str)+1), str);
}

int true = 1;
int false = 0;



char* inputname;
FILE* input;

char curch;

char* buffer;
int buflength;
int token;

int token_other = 0;
int token_ident = 1;
int token_int = 2;

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

    } else if (isdigit(curch)) {
        token = token_int;
        eat_char();

        while (isdigit(curch) && !feof(input))
            eat_char();

    } else if (curch == '+') {
        eat_char();

        if (curch == '+')
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
    getchar();
    errors++;
}

int see (char* look) {
    return !strcmp(buffer, look);
}

int waiting_for (char* look) {
    return !see(look) & !feof(input);
}

void accept () {
    printf("accepted: %s\n", buffer);
    next();
}

void match (char* look) {
    if (!see(look)) {
        error();
        printf("expected '%s', found '%s'\n", look, buffer);
    }

    accept();
}

int try_match (char* look) {
    if (see(look)) {
        accept();
        return true;

    } else
        return false;
}

void expr ();

void factor () {
    if (token == token_ident) {
        accept();

    } else if (token == token_int) {
        accept();

    } else {
        error();
        printf("expected an expression, found '%s'", buffer);
    }
}

void object () {
    factor();

    while (true) {
        if (try_match("(")) {
            if (waiting_for(")")) {
                expr();

                while (try_match(","))
                    expr();
            }

            match(")");

        } else if (try_match("[")) {
            expr();
            match("]");

        } else
            break;
    }
}

void unary () {
    object();

    if (see("++"))
        accept();
}

void expr_1 () {
    unary();

    while (try_match("+")) {
        unary();
    }
}

void expr () {
    expr_1();

    while (try_match("=")) {
        expr_1();
    }
}

void line ();

void if_branch () {
    match("if");
    match("(");
    expr();
    match(")");
    line();

    if (try_match("else"))
        line();
}

void while_loop () {
    match("while");
    match("(");
    expr();
    match(")");

    line();
}

void block ();

void line () {
    if (see("if")) {
        if_branch();

    } else if (see("while")) {
        while_loop();

    } else if (see("{")) {
        block();

    } else {
        if (try_match("return")) {
            if (waiting_for(";"))
                expr();

        } else if (!see(";")) {
            expr();
        }

        match(";");
    }
}

void block () {
    if (try_match("{")) {
        while (waiting_for("}"))
            line();

        match("}");

    } else
        line();
}

int decl_module = 1;
int decl_local = 2;
int decl_param = 3;

void decl (int decl_case) {
    puts("decl +");

    int fn_impl = false;

    /*Simple type*/
    accept();

    /*Level of indirection*/

    int ptr = 0;

    while (try_match("*"))
        ptr++;

    /*Identifier*/
    char* ident = strdup(buffer);
    accept();

    /*Function?*/

    if (try_match("(")) {
        while (waiting_for(")"))
            decl(decl_param);

        match(")");

        if (see("{")) {
            if (decl_case != decl_module) {
                error();
                puts("a function implementation is illegal here");
            }

            fn_impl = true;

            block();
        }
    }

    /*Initializer*/

    if (try_match("=")) {
        expr();

        //TODO: check not fn
    }

    if (!fn_impl && decl_case != decl_param)
        match(";");

    (void) ident;

    puts("- decl");
}

void program () {
    errors = 0;

    while (!feof(input)) {
        decl(decl_module);
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
