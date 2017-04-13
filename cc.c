//---------------
// mini-c, by Sam Nipps (c) 2015
// MIT license
//---------------

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

//No enums :(
int ptr_size = 4;
int word_size = 4;

FILE* output;

//==== Lexer ====

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

char next_char () {
    if (curch == '\n')
        curln++;

    return curch = fgetc(input);
}

bool prev_char (char before) {
    ungetc(curch, input);
    curch = before;
    return false;
}

void eat_char () {
    //The compiler is typeless, so as a compromise indexing is done
    //in word size jumps, and pointer arithmetic in byte jumps.
    (buffer + buflength++)[0] = curch;
    next_char();
}

void next () {
    //Skip whitespace
    while (curch == ' ' || curch == '\r' || curch == '\n' || curch == '\t')
        next_char();

    //Treat preprocessor lines as line comments
    if (   curch == '#'
        || (curch == '/' && (next_char() == '/' || prev_char('/')))) {
        while (curch != '\n' && !feof(input))
            next_char();

        //Restart the function (to skip subsequent whitespace, comments and pp)
        next();
        return;
    }

    buflength = 0;
    token = token_other;

    //Identifier, keyword or integer literal
    if (isalpha(curch) || isdigit(curch)) {
        token = isalpha(curch) ? token_ident : token_int;

        while (token == token_ident ? (isalnum(curch) || curch == '_') && !feof(input)
                                    : isdigit(curch) && !feof(input))
            eat_char();

    //String or character literal
    } else if (curch == '\'' || curch == '"') {
        token = curch == '"' ? token_str : token_char;
        //Can't retrieve this from the buffer - mini-c only has int reads
        char delimiter = curch;
        eat_char();

        while (curch != delimiter && !feof(input)) {
            if (curch == '\\')
                eat_char();

            eat_char();
        }

        eat_char();

    //Two char operators
    } else if (   curch == '+' || curch == '-' || curch == '|' || curch == '&'
               || curch == '=' || curch == '!' || curch == '>' || curch == '<') {
        eat_char();

        if ((curch == buffer[0] && curch != '!') || curch == '=')
            eat_char();

    } else
        eat_char();

    (buffer + buflength++)[0] = 0;
}

void lex_init (char* filename, int maxlen) {
    inputname = filename;
    input = fopen(filename, "r");

    //Get the lexer into a usable state for the parser
    curln = 1;
    buffer = malloc(maxlen);
    next_char();
    next();
}

//==== Parser helper functions ====

int errors;

void error (char* format) {
    printf("%s:%d: error: ", inputname, curln);
    //Accepting an untrusted format string? Naughty!
    printf(format, buffer);
    errors++;
}

void require (bool condition, char* format) {
    if (!condition)
        error(format);
}

bool see (char* look) {
    return !strcmp(buffer, look);
}

bool waiting_for (char* look) {
    return !see(look) && !feof(input);
}

void match (char* look) {
    if (!see(look)) {
        printf("%s:%d: error: expected '%s', found '%s'\n", inputname, curln, look, buffer);
        errors++;
    }

    next();
}

bool try_match (char* look) {
    bool saw = see(look);

    if (saw)
        next();

    return saw;
}

//==== Symbol table ====

char** globals;
int global_no = 0;
bool* is_fn;

char** locals;
int local_no = 0;
int param_no = 0;
int* offsets;

void sym_init (int max) {
    globals = malloc(ptr_size*max);
    is_fn = calloc(max, ptr_size);

    locals = malloc(ptr_size*max);
    offsets = calloc(max, word_size);
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
    //The first local variable is directly below the base pointer
    offsets[local_no] = -word_size*(var_index+1);
    return local_no++;
}

void new_param (char* ident) {
    int local = new_local(ident);

    //At and above the base pointer, in order, are:
    // 1. the old base pointer, [ebp]
    // 2. the return address, [ebp+W]
    // 3. the first parameter, [ebp+2W]
    //   and so on
    offsets[local] = word_size*(2 + param_no++);
}

//Enter the scope of a new function
void new_scope () {
    local_no = 0;
    param_no = 0;
}

int sym_lookup (char** table, int table_size, char* look) {
    int i = 0;

    while (i < table_size)
        if (!strcmp(table[i++], look))
            return i-1;

    return -1;
}

//==== Codegen labels ====

int label_no = 0;

//The label to jump to on `return`
int return_to;

int new_label () {
    return label_no++;
}

int emit_label (int label) {
    fprintf(output, "_%08d:\n", label);
    return label;
}

//==== One-pass parser and code generator ====

bool lvalue;

void needs_lvalue (char* msg) {
    if (!lvalue)
        error(msg);

    lvalue = false;
}

void expr (int level);

//The code generator for expressions works by placing the results
//in eax and backing them up to the stack.

//Regarding lvalues and assignment:

//An expression which can return an lvalue looks head for an
//assignment operator. If it finds one, then it pushes the
//address of its result. Otherwise, it dereferences it.

//The global lvalue flag tracks whether the last operand was an
//lvalue; assignment operators check and reset it.

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
        fputs(".section .rodata\n", output);
        int str = emit_label(new_label());

        //Consecutive string literals are concatenated
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
                //cdecl requires arguments to be pushed on backwards
                
                int start_label = new_label();
                int end_label = new_label();
                int prev_label = end_label;

                fprintf(output, "jmp _%08d\n", start_label);

                do {
                    int next_label = emit_label(new_label());
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
    if (try_match("!")) {
        //Recurse to allow chains of unary operations, LIFO order
        unary();

        fputs("cmp eax, 0\n"
              "mov eax, 0\n"
              "sete al\n", output);

    } else if (try_match("-")) {
        unary();
        fputs("neg eax\n", output);

    } else {
        //This function call compiles itself
        object();

        if (see("++") || see("--")) {
            fprintf(output, "mov ebx, eax\n"
                            "mov eax, [ebx]\n"
                            "%s dword ptr [ebx], 1\n", see("++") ? "add" : "sub");

            needs_lvalue("assignment operator '%s' requires a modifiable object\n");
            next();
        }
    }
}

void branch (bool expr);

void expr (int level) {
    if (level == 5) {
        unary();
        return;
    }

    expr(level+1);

    while (  level == 4 ? see("+") || see("-") || see("*")
           : level == 3 ? see("==") || see("!=") || see("<") || see(">=")
           : false) {
        fputs("push eax\n", output);

        char* instr = see("+") ? "add" : see("-") ? "sub" : see("*") ? "imul" :
                      see("==") ? "e" : see("!=") ? "ne" : see("<") ? "l" : "ge";

        next();
        expr(level+1);

        if (level == 4)
            fprintf(output, "mov ebx, eax\n"
                            "pop eax\n"
                            "%s eax, ebx\n", instr);

        else
            fprintf(output, "pop ebx\n"
                            "cmp ebx, eax\n"
                            "mov eax, 0\n"
                            "set%s al\n", instr);
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
    int loop_to = emit_label(new_label());
    int break_to = new_label();

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

//See decl() implementation
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
    //Body
    
    int body = emit_label(new_label());
    return_to = new_label();
    line();

    //Epilogue

    fprintf(output, "\t_%08d:\n", return_to);
    fputs("mov esp, ebp\n"
          "pop ebp\n"
          "ret\n", output);
    
    //Prologue
    //Only after passing the body do we know how much space to allocate for the
    //local variables, so we write the prologue here at the end.
    fprintf(output, ".globl %s\n"
                    "%s:\n", ident, ident);
    fprintf(output, "push ebp\n"
                    "mov ebp, esp\n"
                    "sub esp, %d\n"
                    "jmp _%08d\n", local_no*word_size, body);
}

void decl (int kind) {
    //A C declaration comes in three forms:
    // - Local decls, which end in a semicolon and can have an initializer.
    // - Parameter decls, which do not and cannot.
    // - Module decls, which end in a semicolon unless there is a function body.

    bool fn = false;
    bool fn_impl = false;
    int local;

    next();

    while (try_match("*"))
        ;

    //Owned (freed) by the symbol table
    char* ident = strdup(buffer);
    next();

    //Functions
    if (try_match("(")) {
        if (kind == decl_module)
            new_scope();

        //Params
        if (waiting_for(")")) do {
            decl(decl_param);
        } while (try_match(","));

        match(")");

        new_fn(ident);
        fn = true;

        //Body
        if (see("{")) {
            require(kind == decl_module, "a function implementation is illegal here\n");

            fn_impl = true;
            function(ident);
        }

    //Add it to the symbol table
    } else {
        if (kind == decl_local) {
            local = new_local(ident);

        } else
            (kind == decl_module ? new_global : new_param)(ident);
    }

    //Initialization

    if (see("="))
        require(!fn && kind != decl_param,
                fn ? "cannot initialize a function\n" : "cannot initialize a parameter\n");

    if (kind == decl_module) {
        fputs(".section .data\n", output);

        if (try_match("=")) {
            if (token == token_int)
                fprintf(output, "%s: .quad %d\n", ident, atoi(buffer));

            else
                error("expected a constant expression, found '%s'\n");

            next();

        //Static data defaults to zero if no initializer
        } else if (!fn)
            fprintf(output, "%s: .quad 0\n", ident);

        fputs(".section .text\n", output);

    } else if (try_match("=")) {
        expr(0);
        fprintf(output, "mov dword ptr [ebp%+d], eax\n", offsets[local]);
    }

    if (!fn_impl && kind != decl_param)
        match(";");
}

void program () {
    fputs(".intel_syntax noprefix\n", output);

    errors = 0;

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

    //No arrays? Fine! A 0xFFFFFF terminated string of null terminated strings will do.
    //A negative-terminated null-terminated strings string, if you will
    char* std_fns = "malloc\0calloc\0free\0atoi\0fopen\0fclose\0fgetc\0ungetc\0feof\0fputs\0fprintf\0puts\0printf\0"
                    "isalpha\0isdigit\0isalnum\0strlen\0strcmp\0strchr\0strcpy\0strdup\0\xFF\xFF\xFF\xFF";

    //Remember that mini-c is typeless, so this is both a byte read and a 4 byte read.
    //(char) 0xFF == -1, (int) 0xFFFFFF == -1
    while (std_fns[0] != -1) {
        new_fn(strdup(std_fns));
        std_fns = std_fns+strlen(std_fns)+1;
    }

    program();

    return errors != 0;
}
