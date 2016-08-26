mini-c
======

I set myself a challenge: write a self-hosting C compiler in 10 hours. This is the result, plus lots of cleanup (check "releases" for the 10 hour version).

Implementation:
- Generates 32-bit x86 assembly, which is then assembled and linked by GCC.
- It is all implemented in a single pass. Code generation is mixed with parsing. This requires some creativity.
- The parser peeks at the next token to decide whether to generate an lvalue.

Language:
- Local and global variables, parameters.
- Functions, `if`, `while`, `do``while`, `return`.
- `=`, `?:` (ternary), `||`, `&&`, `==`, `!=`, `<`, `>=`, `+`, `-`, `*`, `++`, `--` (post-ops), `!`, `-` (unary), `[]`, `()`
- Integer, character, `true` and `false` literals. String literals, with automatic concatenation.
- The language it implements is typeless. Everything is a 4 byte signed integer.
- Pointer indexing works in increments of 4 bytes, pointer arithmetic is byte-by-byte.

The general philosophy was: only include a feature if it reduces the total code size. This is taken to its extreme in the `insane` branch.

Building and running it
-----------------------

    git clone http://github.com/Fedjmike/mini-c
    cd mini-c
    make selftest

This will first produce `cc` by compiling mini-c with GCC. Then it makes `ccself` by compiling mini-c with `cc`. Finally it makes `test/triangular` using `ccself`, and checks the result. You should get something like this:

    $ make selftest
    gcc -std=c11 -Werror -Wall cc.c -o cc
    cc cc.c
    gcc -m32 a.s -o ccself
    ccself tests/triangular.c
    gcc -m32 a.s -o triangular; triangular 5; [ $? -eq 15 ]

If you are on Windows, you will need to checkout the `windows` branch. On 64 bit Linux, you may need to `sudo apt-get install gcc-multilib` to be able to compile and run the 32 bit code.

Related
-------

Another microscopic C compiler is [c4](https://github.com/rswier/c4) by `rswier`. He implemented more C, but I'd say mine is simpler :). Also, c4 generates code for its own VM, whereas mine has the significant difficulty of working with x86 assembly and cdecl.

I wrote another, much more advanced C compiler. [Check it out](https://github.com/Fedjmike/fcc). It too is self-hosting, but with a much more complete feature set, and even some experimental additions like lambdas.

License
-------

Copyright (c) 2015 Sam Nipps

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
