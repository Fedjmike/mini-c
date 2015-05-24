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



FILE* output;



char* inputname;
FILE* input;

int curln;
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
    if (curch == '\n')
        curln++;

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

    curln = 1;
    buffer = malloc(maxlen);
    next_char();
    next();
}



int errors;

void error () {
    printf("%s:%d: error: ", inputname, curln);
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
    int label = label_no;
    label_no++;
    return label;
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
            fprintf(output, "push offset _%s\n", fns[fn]);

        } else if (global >= 0) {
            fprintf(output, "push _%s\n", globals[global]);

        } else if (param >= 0) {
            if (lvalue) {
                fprintf(output, "lea ebx, dword ptr [ebp+%d]\n", param_offset(param));
                fputs("push ebx\n", output);

            } else
                fprintf(output, "push dword ptr [ebp+%d]\n", param_offset(param));

        } else if (local >= 0) {
            if (lvalue) {
                fprintf(output, "lea ebx, dword ptr [ebp-%d]\n", local_offset(local));
                fputs("push ebx\n", output);

            } else
                fprintf(output, "push dword ptr [ebp-%d]\n", local_offset(local));

        } else {
            error();
            printf("no symbol '%s' declared\n", buffer);
        }

        accept();

    } else if (token == token_int) {
        fprintf(output, "push %d\n", atoi(buffer));
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
        printf("expected an expression, found '%s'\n", buffer);
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

            fprintf(output, "call dword ptr [esp+%d]\n", arg_no*word_size);
            fprintf(output, "add esp, %d\n", (arg_no+1)*word_size);
            fputs("push eax\n", output);

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
        fputs("not dword ptr [esp]\n", output);

    } else if (try_match("-")) {
        unary();
        fputs("neg dword ptr [esp]\n", output);

    } else {
        object();

        if (see("++") || see("--")) {
            if (!lvalue) {
                error();
                puts("unanticipated assignment");
            }

            fputs("pop ebx\n", output);

            if (see("++"))
                fputs("add dword ptr [ebx], 1\n", output);

            else
                fputs("sub dword ptr [ebx], 1\n", output);

            fputs("push dword ptr [ebx]\n", output);

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

        fputs("pop ebx\n", output);

        if (!strcmp(op, "+"))
            fputs("add dword ptr [esp], ebx\n", output);

        else if (!strcmp(op, "-"))
            fputs("sub dword ptr [esp], ebx\n", output);

        else
            fputs("imul dword ptr [esp], ebx\n", output);

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

        fputs("pop ebx\n", output);
        fputs("pop ecx\n", output);
        fputs("mov dword ptr [ecx], ebx\n", output);
    }
}

void line ();

void if_branch () {
    int false_branch = new_label();

    match("if");
    match("(");

    expr();

    fputs("pop ebx\n", output);
    fputs("cmp ebx, 0\n", output);
    fprintf(output, "je _%08d\n", false_branch);

    match(")");
    line();

    fprintf(output, "\t_%08d:\n", false_branch);

    if (try_match("else"))
        line();
}

void while_loop () {
    int loop_to = new_label();
    break_to = new_label();

    match("while");
    match("(");

    fprintf(output, "\t_%08d:\n", loop_to);

    expr();

    fputs("pop ebx\n", output);
    fputs("cmp ebx, 0\n", output);
    fprintf(output, "je _%08d\n", break_to);

    match(")");

    line();

    fprintf(output, "jmp _%08d\n", loop_to);
    fprintf(output, "\t_%08d:\n", break_to);
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
                fputs("pop eax\n", output);
                fprintf(output, "jmp _%08d\n", return_to);
            }

        } else if (try_match("break")) {
            fprintf(output, "jmp _%08d\n", break_to);

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
    fprintf(output, ".globl _%s\n", ident);
    fprintf(output, "_%s:\n", ident);

    fputs("push ebp\n", output);
    fputs("mov ebp, esp\n", output);

    return_to = new_label();

    block();

    fprintf(output, "\t_%08d:\n", return_to);

    fputs("mov esp, ebp\n", output);
    fputs("pop ebp\n", output);
    fputs("ret\n", output);
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

        if (waiting_for(")")) {
            decl(decl_param);

            while (try_match(","))
                decl(decl_param);
        }

        match(")");

        new_fn(ident);

        fn = true;

        if (see("{")) {
            if (decl_case != decl_module) {
                error();
                puts("a function implementation is illegal here");
            }

            fn_impl = true;
            function(ident);
        }

    } else {
        if (decl_case == decl_param) {
            new_param(ident);

        } else if (decl_case == decl_local) {
            local = new_local(ident);
            fprintf(output, "sub esp, %d\n", word_size);

        } else
            new_global(ident);
    }

    if (try_match("=")) {
        if (fn) {
            error();
            puts("cannot initialize a function");
        }

        if (decl_case == decl_module) {
            fputs(".section .data\n", output);
            fprintf(output, "%s:\n", ident);

            if (token == token_int) {
                fprintf(output, ".quad %d\n", atoi(buffer));
                accept();

            } else {
                error();
                printf("expected a constant expression, found '%s'", buffer);
            }

            fputs(".section .code\n", output);

        } else {
            expr();

            if (decl_case == decl_local) {
                fputs("pop ebx\n", output);
                fprintf(output, "mov dword ptr [ebp-%d], ebx\n", local_offset(local));

            } else {
                error();
                puts("a variable initialization is illegal here");
            }
        }

    } else {
        if (decl_case == decl_module && !fn) {
            fputs(".section .data\n", output);
            fprintf(output, "%s:\n", ident);
            fputs(".quad 0\n", output);
            fputs(".section .code\n", output);
        }
    }

    if (!fn_impl && decl_case != decl_param)
        match(";");
}

void program () {
    fputs(".intel_syntax noprefix\n", output);

    errors = 0;
    lvalue = false;

    while (!feof(input)) {
        decl(decl_module);
    }
}



int main (int argc, char** argv) {
    if (argc != 2) {
        puts("Usage: cc <file>");
        return 1;
    }

    output = fopen("a.s", "w");

    lex_init(argv[1], 256);

    sym_init(256);

    new_fn("malloc");
    new_fn("free");
    new_fn("atoi");

    new_fn("strlen");
    new_fn("strcmp");
    new_fn("strcpy");

    new_fn("isalpha");
    new_fn("isdigit");
    new_fn("isalnum");

    new_fn("puts");
    new_fn("printf");

    new_fn("fopen");
    new_fn("fgetc");
    new_fn("feof");

    new_fn("fputs");
    new_fn("fprintf");

    program();

    return 0;
}
