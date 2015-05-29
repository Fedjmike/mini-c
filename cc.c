#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

char* strdup (char* str);

int true = 1;
int false = 0;

int ptr_size = 4;
int word_size = 4;

FILE* output;

/*==== Lexer ====*/

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
    if (curch == 10)
        curln++;

    curch = fgetc(input);
}

void eat_char () {
    buffer = buffer+buflength;
    buffer[0] = curch;
    buffer = buffer-buflength;
    buflength++;
    next_char();
}

void next () {
    /*Skip whitespace*/
    while (curch == ' ' || curch == 13 || curch == 10 || curch == 9)
        next_char();

    /*Treat preprocessor lines as line comments*/
    if (curch == '#') {
        next_char();

        while (curch != 10 && !feof(input))
            next_char();

        /*Restart the function (to skip subsequent whitespace and pp)*/
        next();
        return;
    }

    buflength = 0;
    token = token_other;

    /*Identifier or keyword*/
    if (isalpha(curch)) {
        token = token_ident;
        eat_char();

        while ((isalnum(curch) || curch == '_') && !feof(input))
            eat_char();

    /*Integer literal*/
    } else if (isdigit(curch)) {
        token = token_int;
        eat_char();

        while (isdigit(curch) && !feof(input))
            eat_char();

    /*String or character literal*/
    } else if (curch == 39 || curch == '"') {
        if (curch == '"')
            token = token_str;

        else
            token = token_char;

        eat_char();

        while (curch != buffer[0] && !feof(input)) {
            if (curch == 92)
                eat_char();

            eat_char();
        }

        eat_char();

    /*Operators which form a new operator when duplicated e.g. '++'*/
    } else if (curch == '+' || curch == '-' || curch == '=' || curch == '|' || curch == '&') {
        eat_char();

        if (curch == buffer[0])
            eat_char();

    /*Operators which may be followed by a '='*/
    } else if (curch == '!' || curch == '>' || curch == '<') {
        eat_char();

        if (curch == '=')
            eat_char();

    /*Either a comment or a '/' operator*/
    } else if (curch == '/') {
        eat_char();

        /*Comment*/
        if (curch == '*') {
            next_char();

            buflength = 0;

            while (!feof(input)) {
                if (curch == '*') {
                    next_char();

                    if (curch == '/') {
                        next_char();

                        /*Restart the function, overwriting any chars eaten so far*/
                        next();
                        return;
                    }

                } else
                    next_char();
            }
        }

    } else
        eat_char();

    buffer = buffer+buflength;
    buffer[0] = 0;
    buffer = buffer-buflength;
    buflength++;
}

void lex_init (char* filename, int maxlen) {
    inputname = strdup(filename);
    input = fopen(filename, "r");

    /*Get the lexer into a usable state for the parser*/
    curln = 1;
    buffer = malloc(maxlen);
    next_char();
    next();
}

/*==== Parser helper functions ====*/

int errors;

void error (char* format) {
    printf("%s:%d: error: ", inputname, curln);
    /*Accepting an untrusted format string? Naughty!*/
    printf(format, buffer);
    errors++;
}

int see (char* look) {
    return !strcmp(buffer, look);
}

/*Does the next token imply the start of a declaration?
  Used for disambiguating between local variable decls
  and expressions as statements.*/
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
        printf("%s:%d: error: ", inputname, curln);
        printf("expected '%s', found '%s'\n", look, buffer);
        errors++;
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

/*==== Symbol table ====*/

char** globals;
int global_no;
int* is_fn;

char** params;
int param_no;

char** locals;
int local_no;

void sym_init (int max) {
    globals = malloc(ptr_size*max);
    global_no = 0;
    is_fn = calloc(ptr_size, max);

    params = malloc(ptr_size*max);
    param_no = 0;

    locals = malloc(ptr_size*max);
    local_no = 0;
}

void new_global (char* ident) {
    globals[global_no] = strdup(ident);
    global_no++;
}

void new_fn (char* ident) {
    is_fn[global_no] = true;
    new_global(ident);
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
    /*At and above the base pointer, in order, are:
       1. the old base pointer, [ebp]
       2. the return address, [ebp-W]
       3. the first parameter, [ebp-2W]
         and so on*/
    return word_size*(index+2);
}

int local_offset (int index) {
    /*The first local variable is directly below the base pointer*/
    return word_size*(index+1);
}

/*Enter the scope of a new function body*/
void new_scope () {
    param_no = 0;
    local_no = 0;
}

int sym_lookup (char** table, int table_size, char* look) {
    int i = 0;

    while (i < table_size) {
        if (!strcmp(table[i], look))
            return i;

        i++;
    }

    return -1;
}

/*==== Codegen labels ====*/

int label_no = 0;

/*The labels to jump to on `return` and `break`*/
int return_to;
int break_to;

int new_label () {
    int label = label_no;
    label_no++;
    return label;
}

/*==== One-pass parser and code generator ====*/

int lvalue;

void expr ();

void factor () {
    if (token == token_ident) {
        int global = sym_lookup(globals, global_no, buffer);
        int param = sym_lookup(params, param_no, buffer);
        int local = sym_lookup(locals, local_no, buffer);

        if (global >= 0) {
            if (is_fn[global] || lvalue)
                fprintf(output, "push offset _%s\n", globals[global]);

            else
                fprintf(output, "push dword ptr [_%s]\n", globals[global]);

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

        } else
            error("no symbol '%s' declared\n");

        accept();

    } else if (token == token_int) {
        fprintf(output, "push %d\n", atoi(buffer));
        accept();

    } else if (token == token_char) {
        fprintf(output, "push %d\n", buffer[1]);
        accept();

    } else if (token == token_str) {
        int str = new_label();

        fputs(".section .rodata\n", output);
        fprintf(output, "_%08d:\n", str);
        fprintf(output, ".asciz %s\n", buffer);
        fputs(".section .text\n", output);

        fprintf(output, "push offset _%08d\n", str);

        accept();

    } else if (try_match("(")) {
        expr();
        match(")");

    } else
        error("expected an expression, found '%s'\n");
}

void object () {
    factor();

    while (true) {
        if (try_match("(")) {
            lvalue = false;

            int arg_no = 0;

            if (waiting_for(")")) {
                expr();
                arg_no++;

                while (try_match(",")) {
                    expr();
                    arg_no++;
                }
            }

            match(")");

            /*Reverse the parameters as per cdecl*/

            if (arg_no == 2) {
                fputs("pop eax\n", output);
                fputs("pop ebx\n", output);
                fputs("push eax\n", output);
                fputs("push ebx\n", output);

            } else if (arg_no == 3) {
                fputs("mov eax, dword ptr [esp]\n", output);
                fputs("mov ebx, dword ptr [esp+8]\n", output);
                fputs("mov dword ptr [esp+8], eax\n", output);
                fputs("mov dword ptr [esp], ebx\n", output);

            } else if (arg_no >= 4)
                error("too many parameters\n");

            fprintf(output, "call dword ptr [esp+%d]\n", arg_no*word_size);
            fprintf(output, "add esp, %d\n", (arg_no+1)*word_size);
            fputs("push eax\n", output);

        } else if (try_match("[")) {
            int was_lvalue = lvalue;
            lvalue = false;

            expr();
            match("]");

            lvalue = was_lvalue;

            fputs("pop ebx\n", output);
            fputs("pop ecx\n", output);

            if (was_lvalue)
                fputs("mov ecx, dword ptr [ecx]\n", output);

            fprintf(output, "lea ebx, dword ptr [ebx*%d+ecx]\n", word_size);

            if (lvalue)
                fputs("push ebx\n", output);

            else
                fputs("push dword ptr [ebx]\n", output);

        } else
            break;
    }
}

void unary () {
    if (try_match("!")) {
        /*Recurse to allow chains of unary operations, LIFO order*/
        unary();

        int true_label = new_label();
        int join_label = new_label();

        fputs("cmp dword ptr [esp], 0\n", output);
        fprintf(output, "je _%08d\n", true_label);

        fputs("mov dword ptr [esp], 0\n", output);
        fprintf(output, "jmp _%08d\n", join_label);

        fprintf(output, "\t_%08d:\n", true_label);
        fputs("mov dword ptr [esp], 1\n", output);

        fprintf(output, "\t_%08d:\n", join_label);

    } else if (try_match("-")) {
        unary();
        fputs("neg dword ptr [esp]\n", output);

    } else {
        object();

        if (see("++") || see("--")) {
            if (!lvalue)
                error("unanticipated assignment\n");

            fputs("pop ebx\n", output);
            fputs("push dword ptr [ebx]\n", output);

            if (see("++"))
                fputs("add dword ptr [ebx], 1\n", output);

            else
                fputs("sub dword ptr [ebx], 1\n", output);

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

        else {
            fputs("imul ebx, dword ptr [esp]\n", output);
            fputs("mov dword ptr [esp], ebx\n", output);
        }

        free(op);
    }
}

void expr_2 () {
    expr_3();

    while (see("==") || see("!=") || see("<") || see(">=")) {
        char* op = strdup(buffer);

        accept();
        expr_3();

        fputs("pop ebx\n", output);
        fputs("cmp dword ptr [esp], ebx\n", output);

        int true_label = new_label();
        int join_label = new_label();

        if (!strcmp(op, "=="))
            fprintf(output, "je _%08d\n", true_label);

        else if (!strcmp(op, "!="))
            fprintf(output, "jne _%08d\n", true_label);

        else if (!strcmp(op, "<"))
            fprintf(output, "jl _%08d\n", true_label);

        else
            fprintf(output, "jge _%08d\n", true_label);

        fputs("mov dword ptr [esp], 0\n", output);
        fprintf(output, "jmp _%08d\n", join_label);
        fprintf(output, "\t_%08d:\n", true_label);
        fputs("mov dword ptr [esp], 1\n", output);
        fprintf(output, "\t_%08d:\n", join_label);

        free(op);
    }
}

void expr_1 () {
    expr_2();

    while (see("||") || see("&&")) {
        int shortcircuit = new_label();

        fputs("cmp dword ptr [esp], 0\n", output);

        if (see("||"))
            fprintf(output, "jnz _%08d\n", shortcircuit);

        else
            fprintf(output, "jz _%08d\n", shortcircuit);

        fputs("pop ebx\n", output);

        accept();
        expr_2();

        fprintf(output, "\t_%08d:\n", shortcircuit);
    }
}

void expr () {
    expr_1();

    if (try_match("=")) {
        if (!lvalue)
            error("unanticipated assignment\n");

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
    int join = new_label();

    match("if");
    match("(");

    expr();

    fputs("pop ebx\n", output);
    fputs("cmp ebx, 0\n", output);
    fprintf(output, "je _%08d\n", false_branch);

    match(")");
    line();

    fprintf(output, "jmp _%08d\n", join);
    fprintf(output, "\t_%08d:\n", false_branch);

    if (try_match("else"))
        line();

    fprintf(output, "\t_%08d:\n", join);
}

void while_loop () {
    int loop_to = new_label();
    int old_break_to = break_to;
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

    /*Restore the break label for any outer loop*/
    break_to = old_break_to;
}

void block () {
    if (try_match("{")) {
        while (waiting_for("}"))
            line();

        match("}");

    } else
        line();
}

void decl (int decl_case);

int decl_module = 1;
int decl_local = 2;
int decl_param = 3;

void line () {
    if (see("if"))
        if_branch();

    else if (see("while"))
        while_loop();

    else if (see("{"))
        block();

    else if (see_decl())
        decl(decl_local);

    else {
        if (try_match("return")) {
            if (waiting_for(";")) {
                expr();
                fputs("pop eax\n", output);
                fprintf(output, "jmp _%08d\n", return_to);
            }

        } else if (try_match("break")) {
            fprintf(output, "jmp _%08d\n", break_to);

        } else if (waiting_for(";")) {
            lvalue = true;
            expr();
            lvalue = false;
        }

        match(";");
    }
}

void function (char* ident) {
    /*Prologue*/

    fprintf(output, ".globl _%s\n", ident);
    fprintf(output, "_%s:\n", ident);

    fputs("push ebp\n", output);
    fputs("mov ebp, esp\n", output);

    /*Body*/

    return_to = new_label();

    block();

    /*Epilogue*/

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

    while (try_match("*"))
        ;

    char* ident = strdup(buffer);
    accept();

    /*Functions*/
    if (try_match("(")) {
        if (decl_case == decl_module)
            new_scope();

        /*Params*/
        if (waiting_for(")")) {
            decl(decl_param);

            while (try_match(","))
                decl(decl_param);
        }

        match(")");

        new_fn(ident);

        fn = true;

        /*Body*/
        if (see("{")) {
            if (decl_case != decl_module)
                error("a function implementation is illegal here\n");

            fn_impl = true;
            function(ident);
        }

    /*Add it to the symbol table*/
    } else {
        if (decl_case == decl_param) {
            new_param(ident);

        } else if (decl_case == decl_local) {
            local = new_local(ident);
            fprintf(output, "sub esp, %d\n", word_size);

        } else
            new_global(ident);
    }

    /*Initialization*/
    if (try_match("=")) {
        if (fn)
            error("cannot initialize a function\n");

        if (decl_case == decl_module) {
            fputs(".section .data\n", output);
            fprintf(output, "_%s:\n", ident);

            if (token == token_int) {
                fprintf(output, ".quad %d\n", atoi(buffer));
                accept();

            } else
                error("expected a constant expression, found '%s'\n");

            fputs(".section .text\n", output);

        } else {
            expr();

            if (decl_case == decl_local) {
                fputs("pop ebx\n", output);
                fprintf(output, "mov dword ptr [ebp-%d], ebx\n", local_offset(local));

            } else
                error("a variable initialization is illegal here\n");
        }

    } else {
        if (decl_case == decl_module && !fn) {
            fputs(".section .data\n", output);
            fprintf(output, "_%s:\n", ident);
            /*Static data defaults to zero if no initializer*/
            fputs(".quad 0\n", output);
            fputs(".section .text\n", output);
        }
    }

    if (!fn_impl && decl_case != decl_param)
        match(";");
}

void program () {
    fputs(".intel_syntax noprefix\n", output);

    errors = 0;
    lvalue = false;

    while (!feof(input))
        decl(decl_module);
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
    new_fn("strdup");

    new_fn("isalpha");
    new_fn("isdigit");
    new_fn("isalnum");

    new_fn("puts");
    new_fn("printf");

    new_fn("getchar");

    new_fn("fopen");
    new_fn("fgetc");
    new_fn("feof");

    new_fn("fputs");
    new_fn("fprintf");

    program();

    return errors != 0;
}
