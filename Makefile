cc: cc.c
	gcc -std=c11 -Werror -Wall cc.c -o cc

tests/%: tests/%.c cc
	cc $<
	gcc -m32 a.s -o $@

ccself: cc
	rm -f a.s ccself
	cc cc.c
	gcc -m32 a.s -o ccself

selfhost: ccself

selftest: ccself tests/triangular.c
	ccself tests/triangular.c
	gcc -m32 a.s -o triangular; triangular 5; [ $$? -eq 15 ]

clean:
	rm -f {cc,ccself,triangular}{,.exe} a.s

.PHONY: selfhost selftest clean