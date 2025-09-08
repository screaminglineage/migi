CC := gcc
# CC := clang -fdiagnostics-color=always

INCLUDE := -I./src
CFLAGS := -Wall -Wextra -Wno-unused-function -Wno-override-init
DEBUGFLAGS := -ggdb
SANITIZERS := -fsanitize=undefined,address
RELEASEFLAGS := -O3 -DMIGI_DISABLE_ASSERTS

main: main.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} main.c -lm -o main

test_arena: test_arena.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} test_arena.c -lm -o test_arena

test_hashmap: test_hashmap.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} -Wstrict-aliasing=2 -fstrict-aliasing test_hashmap.c -lm -o test_hashmap

test_lexer: test_lexer.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} test_lexer.c -o test_lexer

struct_printer: tools/struct_printer.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} tools/struct_printer.c -o struct_printer

test_struct_printer: struct_printer test_struct_printer.c
	./struct_printer test_struct_printer.c gen                                                   \
		&& ${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} test_struct_printer.c -o test_struct_printer \
		&& ./test_struct_printer

@PHONY=release
release: main.c src/*.h
	${CC} ${CFLAGS} ${RELEASEFLAGS} ${INCLUDE} main.c -lm -o main

@PHONY=hashmap_release
hashmap_release: test_hashmap.c src/*.h
	${CC} ${CFLAGS} ${RELEASEFLAGS} ${INCLUDE} test_hashmap.c -lm -o test_hashmap

@PHONY=run
run: main
	./main
