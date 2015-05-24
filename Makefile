cc: cc.c
	gcc -std=c11 -Werror -Wall cc.c -o cc

tests/%: tests/%.c cc
	cc $< > $@.s
	gcc -m32 $@.s -o $@

selfhost: cc
	cc cc.c > cc.s
	cat cc.s