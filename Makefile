cc: cc.c
	gcc -std=c11 -Werror -Wall cc.c -o cc

tests/%: tests/%.c cc
	cc $<
	gcc -m32 a.s -o $@

selfhost: cc
	cc cc.c
	gcc -m32 a.s -o ccself
	ccself tests/triangular.c