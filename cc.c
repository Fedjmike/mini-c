#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

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
    (buffer + buflength++)[0] = curch;
    next_char();
}

void next () {
    while (curch == ' ' || curch == '\r' || curch == '\n' || curch == '\t')
        next_char();

    if (curch == '#') {
        while (curch != '\n' && feof(input) == false)
            next_char();

        next();
        return;
    }

    buflength = 0;
    token = token_other;

    if (isalpha(curch)) {
        token = token_ident;

        while ((isalnum(curch) || curch == '_') && feof(input) == false)
            eat_char();

    } else if (isdigit(curch)) {
        token = token_int;

        while (isdigit(curch) && feof(input) == false)
            eat_char();

    } else if (curch == '\'' || curch == '"') {
        token = curch == '"' ? token_str : token_char;
        eat_char();

        while (curch != buffer[0] && feof(input) == false) {
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

    (buffer + buflength++)[0] = 0;
}

void lex_init (char* filename, int maxlen) {
    inputname = strdup(filename);
    input = fopen(filename, "r");

    curln = 1;
    buffer = malloc(maxlen);
    next_char();
    next();
}

void lex_end () {
    free(buffer);
    fclose(input);
}

int errors;

void error (char* format) {
    printf("%s:%d: error: ", inputname, curln);
    printf(format, buffer);
    errors++;
}

void require (bool condition, char* format) {
    if (condition == false)
        error(format);
}

bool see (char* look) {
    return strcmp(buffer, look) == 0;
}

bool waiting_for (char* look) {
    return see(look) == false && feof(input) == false;
}

void match (char* look) {
    if (see(look) == false) {
        printf("%s:%d: error: ", inputname, curln);
        printf("expected '%s', found '%s'\n", look, buffer);
        errors++;
    }

    next();
}

bool try_match (char* look) {
    if (see(look)) {
        next();
        return true;

    } else
        return false;
}

char** globals;
int global_no;
bool* is_fn;

char** locals;
int local_no;
int param_no;
int* offsets;

void sym_init (int max) {
    globals = malloc(ptr_size*max);
    global_no = 0;
    is_fn = calloc(max, ptr_size);

    locals = malloc(ptr_size*max);
    local_no = 0;
    param_no = 0;
    offsets = calloc(max, word_size);
}

void table_end (char** table, int table_size) {
    int i = 0;

    while (i < table_size)
        free(table[i++]);
}

void sym_end () {
    table_end(globals, global_no);
    free(globals);
    free(is_fn);

    table_end(locals, local_no);
    free(locals);
    free(offsets);
}

void new_global (char* ident) {
    globals[global_no++] = ident;
}

void new_fn (char* ident) {
    is_fn[global_no] = true;
    new_global(ident);
}

int new_local (char* ident) {
    int var_index = local_no - param_no;

    locals[local_no] = ident;
    offsets[local_no] = (0-word_size)*(var_index+1);
    return local_no++;
}

void new_param (char* ident) {
    int local = new_local(ident);
    offsets[local] = word_size*(2 + param_no++);
}

void new_scope () {
    table_end(locals, local_no);
    local_no = 0;
    param_no = 0;
}

int sym_lookup (char** table, int table_size, char* look) {
    int i = 0;

    while (i < table_size)
        if (strcmp(table[i++], look) == 0)
            return i-1;

    return 0-1;
}

int label_no = 0;

int return_to;

int new_label () {
    return label_no++;
}

bool lvalue;

void needs_lvalue (char* msg) {
    if (lvalue == false)
        error(msg);

    lvalue = false;
}

void expr (int level);

void factor () {
    lvalue = false;

    if (see("true") || see("false")) {
        fprintf(output, "mov eax, %d\n", see("true") ? 1 : 0);
        next();

    } else if (token == token_ident) {
        int global = sym_lookup(globals, global_no, buffer);
        int local = sym_lookup(locals, local_no, buffer);

        require(global >= 0 || local >= 0, "no symbol '%s' declared\n");
        next();

        if (see("=") || see("++") || see("--"))
            lvalue = true;

        if (global >= 0)
            fprintf(output, "%s eax, [%s]\n", is_fn[global] || lvalue ? "lea" : "mov", globals[global]);

        else if (local >= 0)
            fprintf(output, "%s eax, [ebp%+d]\n", lvalue ? "lea" : "mov", offsets[local]);

    } else if (token == token_int || token == token_char) {
        fprintf(output, "mov eax, %s\n", buffer);
        next();

    } else if (token == token_str) {
        int str = new_label();

        fprintf(output, ".section .rodata\n"
                        "_%08d:\n", str);

        while (token == token_str) {
            fprintf(output, ".ascii %s\n", buffer);
            next();
        }

        fputs(".byte 0\n"
              ".section .text\n", output);

        fprintf(output, "mov eax, offset _%08d\n", str);

    } else if (try_match("(")) {
        expr(0);
        match(")");

    } else
        error("expected an expression, found '%s'\n");
}

void object () {
    factor();

    while (true) {
        if (try_match("(")) {
            fputs("push eax\n", output);

            int arg_no = 0;

            if (waiting_for(")")) {
                int start_label = new_label();
                int end_label = new_label();
                int prev_label = end_label;

                fprintf(output, "jmp _%08d\n", start_label);

                do {
                    int next_label = new_label();

                    fprintf(output, "_%08d:\n", next_label);
                    expr(0);
                    fprintf(output, "push eax\n"
                                    "jmp _%08d\n", prev_label);
                    arg_no++;

                    prev_label = next_label;
                } while (try_match(","));

                fprintf(output, "_%08d:\n", start_label);
                fprintf(output, "jmp _%08d\n", prev_label);
                fprintf(output, "_%08d:\n", end_label);
            }

            match(")");

            fprintf(output, "call dword ptr [esp+%d]\n", arg_no*word_size);
            fprintf(output, "add esp, %d\n", (arg_no+1)*word_size);

        } else if (try_match("[")) {
            fputs("push eax\n", output);

            expr(0);
            match("]");

            if (see("=") || see("++") || see("--"))
                lvalue = true;

            fprintf(output, "pop ebx\n"
                            "%s eax, [eax*%d+ebx]\n", lvalue ? "lea" : "mov", word_size);

        } else
            return;
    }
}

void unary () {
    object();

    if (see("++") || see("--")) {
        fprintf(output, "mov ebx, eax\n"
                        "mov eax, [ebx]\n"
                        "%s dword ptr [ebx], 1\n", see("++") ? "add" : "sub");

        needs_lvalue("assignment operator '%s' requires a modifiable object\n");
        next();
    }
}

void branch (bool expr);

void expr (int level) {
    if (level == 5) {
        unary();
        return;
    }

    expr(level+1);

    char* instr;

    while ((instr =   level == 4 ? (see("+") ? "add" : see("-") ? "sub" : see("*") ? "imul" : 0)
                    : level == 3 ? (see("==") ? "e" : see("!=") ? "ne" : see("<") ? "l" : see(">=") ? "ge" : 0)
                    : 0)) {
        fputs("push eax\n", output);

        next();
        expr(level+1);

        char* arith = "mov ebx, eax\n"
                      "pop eax\n"
                      "%s eax, ebx\n";

        char* comp = "pop ebx\n"
                     "cmp ebx, eax\n"
                     "mov eax, 0\n"
                     "set%s al\n";

        fprintf(output, level == 4 ? arith : comp, instr);
    }

    if (level == 2) while (see("||") || see("&&")) {
        int shortcircuit = new_label();

        fprintf(output, "cmp eax, 0\n"
                        "j%s _%08d\n", see("||") ? "nz" : "z", shortcircuit);
        next();
        expr(level+1);

        fprintf(output, "\t_%08d:\n", shortcircuit);
    }

    if (level == 1 && try_match("?"))
        branch(true);

    if (level == 0 && try_match("=")) {
        fputs("push eax\n", output);

        needs_lvalue("assignment requires a modifiable object\n");
        expr(level+1);

        fputs("pop ebx\n"
              "mov dword ptr [ebx], eax\n", output);
    }
}

void line ();

void branch (bool isexpr) {
    int false_branch = new_label();
    int join = new_label();

    fprintf(output, "cmp eax, 0\n"
                    "je _%08d\n", false_branch);

    isexpr ? expr(1) : line();

    fprintf(output, "jmp _%08d\n", join);
    fprintf(output, "\t_%08d:\n", false_branch);

    if (isexpr) {
        match(":");
        expr(1);

    } else if (try_match("else"))
        line();

    fprintf(output, "\t_%08d:\n", join);
}

void if_branch () {
    match("if");
    match("(");
    expr(0);
    match(")");
    branch(false);
}

void while_loop () {
    int loop_to = new_label();
    int break_to = new_label();

    fprintf(output, "\t_%08d:\n", loop_to);

    bool do_while = try_match("do");

    if (do_while)
        line();

    match("while");
    match("(");
    expr(0);
    match(")");

    fprintf(output, "cmp eax, 0\n"
                    "je _%08d\n", break_to);

    if (do_while)
        match(";");

    else
        line();

    fprintf(output, "jmp _%08d\n", loop_to);
    fprintf(output, "\t_%08d:\n", break_to);
}

void decl (int kind);

int decl_module = 1;
int decl_local = 2;
int decl_param = 3;

void line () {
    if (see("if"))
        if_branch();

    else if (see("while") || see("do"))
        while_loop();

    else if (see("int") || see("char") || see("bool"))
        decl(decl_local);

    else if (try_match("{")) {
        while (waiting_for("}"))
            line();

        match("}");

    } else {
        bool ret = try_match("return");

        if (waiting_for(";"))
            expr(0);

        if (ret)
            fprintf(output, "jmp _%08d\n", return_to);

        match(";");
    }
}

void function (char* ident) {
    fprintf(output, ".globl %s\n", ident);
    fprintf(output, "%s:\n", ident);

    fputs("push ebp\n"
          "mov ebp, esp\n", output);

    return_to = new_label();

    line();

    fprintf(output, "\t_%08d:\n", return_to);
    fputs("mov esp, ebp\n"
          "pop ebp\n"
          "ret\n", output);
}

void decl (int kind) {
    bool fn = false;
    bool fn_impl = false;
    int local;

    next();

    while (try_match("*"))
        ;

    char* ident = strdup(buffer);
    next();

    if (try_match("(")) {
        if (kind == decl_module)
            new_scope();

        if (waiting_for(")")) do {
            decl(decl_param);
        } while (try_match(","));

        match(")");

        new_fn(ident);
        fn = true;

        if (see("{")) {
            require(kind == decl_module, "a function implementation is illegal here\n");

            fn_impl = true;
            function(ident);
        }

    } else {
        if (kind == decl_local) {
            local = new_local(ident);
            fprintf(output, "sub esp, %d\n", word_size);

        } else
            (kind == decl_module ? new_global : new_param)(ident);
    }

    if (see("="))
        require(fn == false && kind != decl_param,
                fn ? "cannot initialize a function\n" : "cannot initialize a parameter");

    if (kind == decl_module) {
        fputs(".section .data\n", output);

        if (try_match("=")) {
            if (token == token_int)
                fprintf(output, "%s: .quad %d\n", ident, atoi(buffer));

            else
                error("expected a constant expression, found '%s'\n");

            next();

        } else if (fn == false)
            fprintf(output, "%s: .quad 0\n", ident);

        fputs(".section .text\n", output);

    } else if (try_match("=")) {
        expr(0);
        fprintf(output, "mov dword ptr [ebp%+d], eax\n", offsets[local]);
    }

    if (fn_impl == false && kind != decl_param)
        match(";");
}

void program () {
    fputs(".intel_syntax noprefix\n", output);

    errors = 0;

    while (feof(input) == false)
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

    char* std_fns = "malloc\0calloc\0free\0atoi\0fopen\0fclose\0fgetc\0ungetc\0feof\0fputs\0fprintf\0puts\0printf\0"
                    "isalpha\0isdigit\0isalnum\0strlen\0strcmp\0strchr\0strcpy\0strdup\0\xFF\xFF\xFF\xFF";

    while (std_fns[0] != 0-1) {
        new_fn(std_fns);
        std_fns = std_fns+strlen(std_fns)+1;
    }

    program();

    lex_end();
    sym_end();
    fclose(output);

    return errors != 0;
}
