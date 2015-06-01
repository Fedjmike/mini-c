mini-c
======

I set myself a challenge: write a self-hosting C compiler in 10 hours. This is the result, plus lots of cleanup (check "releases" for the 10 hour version).

Implementation:
- Generates 32-bit x86 assembly, which is then assembled and linked by GCC.
- It is all implemented in a single pass. Code generation is mixed with parsing. This requires some creativity.
- The compiler has to guess whether an expression will be used as an lvalue ahead of time. Trouble!

Language:
- Local and global variables, parameters.
- Functions, `if`, `while`, `return`, `break`.
- `=`, `?:` (ternary), `||`, `&&`, `==`, `!=`, `<`, `>=`, `+`, `-`, `*`, `++`, `--` (post-ops), `!`, `-`, `*` (unary), `[]`, `()`
- The language it implements is typeless. Everything is a 4 byte signed integer.
- Pointer indexing works in increments of 4 bytes, pointer arithmetic is byte-by-byte.

The general philosophy was: only include a feature if it reduces the total code size.

Building and running it
=======================

    make selftest

This will first produce `cc` by compiling mini-c with GCC. Then it makes `ccself` by compiling mini-c with `cc`. Finally it makes `test/triangular` using `ccself`, and checks the result. You should get something like this:

    $ make selftest
    gcc -std=c11 -Werror -Wall cc.c -o cc
    cc cc.c
    gcc -m32 a.s -o ccself
    ccself tests/triangular.c
    gcc -m32 a.s -o triangular; triangular 5; [ $? -eq 15 ]

If you are on Windows, you will need to checkout the `windows` branch.

Related
=======

Another microscopic C compiler is [c4](https://github.com/rswier/c4) by `rswier`. He implemented more C, but I'd say mine is simpler :). Also, c4 generates code for its own VM, whereas mine has the significant difficulty of working with x86 assembly and cdecl.

I wrote another, much more advanced C compiler. [Check it out](https://github.com/Fedjmike/fcc). It too is self-hosting, but with a much more complete feature set, and even some experimental additions like lambdas.