all: cc.c
	gcc -std=c11 -Werror -Wall cc.c -o cc

selfhost: cc
	cc cc.c