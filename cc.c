#include "stdlib.h"
#include "string.h"
#include "ctype.h"
#include "stdio.h"



char* strdup (char* str) {
    return strcpy(malloc(strlen(str)+1), str);
}

int true = 1;
int false = 0;

int ptr_size = 4;
int word_size = 4;



char* inputname;
FILE* input;

char curch;

char* buffer;
int buflength;
int token;

int token_other = 0;
int token_ident = 1;
int token_int = 2;
int token_char = 3;
int token_str = 4;

void next_char () {
    curch = fgetc(input);
}

void eat_char () {
    buffer[buflength] = curch;
    buflength++;
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

    } else if (curch == '\'' || curch == '"') {
        if (curch == '"')
            token = token_str;

        else
            token = token_char;

        eat_char();

        while (curch != buffer[0] && !feof(input)) {
            if (curch == '\\')
                eat_char();

            eat_char();
        }

        eat_char();

    } else if (curch == '+' || curch == '-' || curch == '=' || curch == '|' || curch == '&') {
        eat_char();

        if (curch == buffer[0])
            eat_char();

    } else if (curch == '!' || curch == '>' || curch == '<') {
        eat_char();

        if (curch == '=')
            eat_char();

    } else
        eat_char();

    buffer[buflength] = '\0';
    buflength++;
}

void lex_init (char* filename, int maxlen) {
    inputname = strdup(filename);
    input = fopen(filename, "r");

    buffer = malloc(maxlen);
    next_char();
    next();
}



int errors;

void error () {
    fprintf(stderr, "%s: error: ", inputname);
    errors++;
}

int see (char* look) {
    return !strcmp(buffer, look);
}

int see_decl () {
    return see("int") || see("char");
}

int waiting_for (char* look) {
    return !see(look) && !feof(input);
}

void accept () {
    next();
}

void match (char* look) {
    if (!see(look)) {
        error();
        fprintf(stderr, "expected '%s', found '%s'\n", look, buffer);
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



char** fns;
int fn_no;

char** globals;
int global_no;

char** params;
int param_no;

char** locals;
int local_no;

void sym_init (int max) {
    fns = malloc(ptr_size*max);
    fn_no = 0;

    globals = malloc(ptr_size*max);
    global_no = 0;

    params = malloc(ptr_size*max);
    param_no = 0;

    locals = malloc(ptr_size*max);
    local_no = 0;
}

void new_fn (char* ident) {
    fns[fn_no] = strdup(ident);
    fn_no++;
}

void new_global (char* ident) {
    globals[global_no] = strdup(ident);
    global_no++;
}

void new_param (char* ident) {
    params[param_no] = strdup(ident);
    param_no++;
}

int new_local (char* ident) {
    locals[local_no] = strdup(ident);
    int index = local_no;
    local_no++;
    return index;
}

int param_offset (int index) {
    return word_size*(index+2);
}

int local_offset (int index) {
    return word_size*(index+1);
}

void new_scope () {
    param_no = 0;
    local_no = 0;
}

int lookup_fn (char* look) {
    int i = 0;

    while (i < fn_no) {
        if (!strcmp(fns[i], look))
            return i;

        i++;
    }

    return -1;
}

int lookup_global (char* look) {
    int i = 0;

    while (i < global_no) {
        if (!strcmp(globals[i], look))
            return i;

        i++;
    }

    return -1;
}

int lookup_param (char* look) {
    int i = 0;

    while (i < param_no) {
        if (!strcmp(params[i], look))
            return i;

        i++;
    }

    return -1;
}

int lookup_local (char* look) {
    int i = 0;

    while (i < local_no) {
        if (!strcmp(locals[i], look))
            return i;

        i++;
    }

    return -1;
}



int label_no = 0;

int return_to;
int break_to;

int new_label () {
    return label_no++;
}



int lvalue;

void expr ();

void factor () {
    if (token == token_ident) {
        int fn = lookup_fn(buffer);
        int global = lookup_global(buffer);
        int param = lookup_param(buffer);
        int local = lookup_local(buffer);

        if (fn >= 0) {
            printf("push offset _%s\n", fns[fn]);

        } else if (global >= 0) {
            printf("push _%s", globals[global]);

        } else if (param >= 0) {
            if (lvalue) {
                printf("lea ebx, dword ptr [ebp+%d]\n", param_offset(param));
                puts("push ebx");

            } else
                printf("push dword ptr [ebp+%d]\n", param_offset(param));

        } else if (local >= 0) {
            if (lvalue) {
                printf("lea ebx, dword ptr [ebp-%d]\n", local_offset(local));
                puts("push ebx");

            } else
                printf("push dword ptr [ebp-%d]\n", local_offset(local));

        } else {
            error();
            fprintf(stderr, "no symbol '%s' declared\n", buffer);
        }

        accept();

    } else if (token == token_int) {
        printf("push %d\n", atoi(buffer));
        accept();

    } else if (token == token_char) {
        accept();

    } else if (token == token_str) {
        accept();

    } else if (try_match("(")) {
        expr();
        match(")");

    } else {
        error();
        fprintf(stderr, "expected an expression, found '%s'\n", buffer);
    }
}

void object () {
    factor();

    while (true) {
        if (try_match("(")) {
            int arg_no = 0;

            if (waiting_for(")")) {
                expr();
                arg_no++;

                while (try_match(",")) {
                    expr();
                    arg_no++;
                }
            }

            printf("call dword ptr [esp+%d]\n", arg_no*word_size);
            printf("add esp, %d\n", (arg_no+1)*word_size);
            puts("push eax");

            match(")");

        } else if (try_match("[")) {
            expr();
            match("]");

        } else
            break;
    }
}

void unary () {
    if (try_match("!")) {
        unary();
        puts("not dword ptr [esp]");

    } else if (try_match("-")) {
        unary();
        puts("neg dword ptr [esp]");

    } else {
        object();

        if (see("++") || see("--")) {
            if (!lvalue) {
                error();
                puts("unanticipated assignment");
            }

            puts("pop ebx");

            if (see("++"))
                puts("add dword ptr [ebx], 1");

            else
                puts("sub dword ptr [ebx], 1");

            puts("push dword ptr [ebx]");

            lvalue = false;

            accept();

        }
    }
}

void expr_3 () {
    unary();

    while (see("+") || see("-") || see("*")) {
        char* op = strdup(buffer);

        accept();
        unary();

        puts("pop ebx");

        if (!strcmp(op, "+"))
            puts("add dword ptr [esp], ebx");

        else if (!strcmp(op, "-"))
            puts("sub dword ptr [esp], ebx");

        else
            puts("imul dword ptr [esp], ebx");

        free(op);
    }
}

void expr_2 () {
    expr_3();

    while (see("==") || see("!=") || see("<") || see(">=")) {
        char* op = strdup(buffer);

        accept();
        expr_3();

        free(op);
    }
}

void expr_1 () {
    expr_2();

    while (try_match("||") || try_match("&&")) {
        expr_2();
    }
}

void expr () {
    expr_1();

    if (try_match("=")) {
        if (!lvalue) {
            error();
            puts("unanticipated assignment");
        }

        lvalue = false;

        expr_1();

        puts("pop ebx");
        puts("pop ecx");
        puts("mov dword ptr [ecx], ebx");
    }
}

void line ();

void if_branch () {
    int false_branch = new_label();

    match("if");
    match("(");

    expr();

    puts("pop ebx");
    puts("cmp ebx, 0");
    printf("je _%08d\n", false_branch);

    match(")");
    line();

    printf("\t_%08d:\n", false_branch);

    if (try_match("else"))
        line();
}

void while_loop () {
    int loop_to = new_label();
    break_to = new_label();

    match("while");
    match("(");

    printf("\t_%08d:\n", loop_to);

    expr();

    puts("pop ebx");
    puts("cmp ebx, 0");
    printf("je _%08d\n", break_to);

    match(")");

    line();

    printf("jmp _%08d\n", loop_to);
    printf("\t_%08d:\n", break_to);
}

void block ();
void decl (int decl_case);

int decl_module = 1;
int decl_local = 2;
int decl_param = 3;

void line () {
    if (see("if")) {
        if_branch();

    } else if (see("while")) {
        while_loop();

    } else if (see("{")) {
        block();

    } else if (see_decl()) {
        decl(decl_local);

    } else {
        if (try_match("return")) {
            if (waiting_for(";")) {
                expr();
                puts("pop eax");
                printf("jmp _%08d\n", return_to);
            }

        } else if (try_match("break")) {
            printf("jmp _%08d\n", break_to);

        } else if (!see(";")) {
            lvalue = true;
            expr();
            lvalue = false;
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

void function (char* ident) {
    printf(".globl _%s\n", ident);
    printf("_%s:\n", ident);

    puts("push ebp");
    puts("mov ebp, esp");

    return_to = new_label();

    block();

    printf("\t_%08d:\n", return_to);

    puts("mov esp, ebp");
    puts("pop ebp");
    puts("ret");
}

void decl (int decl_case) {
    int fn = false;
    int fn_impl = false;
    int local;

    accept();

    int ptr = 0;

    while (try_match("*"))
        ptr++;

    char* ident = strdup(buffer);
    accept();

    if (try_match("(")) {
        if (decl_case == decl_module)
            new_scope();

        while (waiting_for(")"))
            decl(decl_param);

        match(")");

        fn = true;

        if (see("{")) {
            if (decl_case != decl_module) {
                error();
                fputs("a function implementation is illegal here\n", stderr);
            }

            fn_impl = true;
            function(ident);
        }
    }

    if (decl_case == decl_param) {
        new_param(ident);

    } else if (decl_case == decl_local) {
        local = new_local(ident);
        printf("sub esp, %d\n", word_size);

    } else if (fn) {
        new_fn(ident);

    } else
        new_global(ident);

    if (try_match("=")) {
        if (fn) {
            error();
            fputs("cannot initialize a function\n", stderr);
        }

        if (decl_case == decl_module) {
            puts(".section .data");
            printf("%s:\n", ident);

            if (token == token_int) {
                printf(".quad %d\n", atoi(buffer));
                accept();

            } else {
                error();
                fprintf(stderr, "expected a constant expression, found '%s'", buffer);
            }

            puts(".section .code");

        } else {
            expr();

            if (decl_case == decl_local) {
                puts("pop ebx");
                printf("mov dword ptr [ebp-%d], ebx\n", local_offset(local));

            } else {
                error();
                fputs("a variable initialization is illegal here\n", stderr);
            }
        }
    }

    if (!fn_impl && decl_case != decl_param)
        match(";");
}

void program () {
    puts(".intel_syntax noprefix");

    errors = 0;
    lvalue = false;

    while (!feof(input)) {
        decl(decl_module);
    }
}



int main (int argc, char** argv) {
    if (argc != 2) {
        fputs("Usage: cc <file>", stderr);
        return 1;
    }

    lex_init(argv[1], 256);

    sym_init(256);

    program();

    return 0;
}
