CC := gcc
# CC := clang -fdiagnostics-color=always

INCLUDE := -I./src
CFLAGS := -Wall -Wextra -Wno-unused-function -Wno-override-init
DEBUGFLAGS := -ggdb
SANITIZERS := -fsanitize=undefined,address
RELEASEFLAGS := -O3 -DMIGI_DISABLE_ASSERTS
BUILD := ./build

main: scratch/main.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/main.c -lm -o ${BUILD}/main

test: scratch/test.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/test.c -lm -o ${BUILD}/test

test_array_list: scratch/test_array_list.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/test_array_list.c -o  ${BUILD}/test_array_list

test_arena: scratch/test_arena.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/test_arena.c -lm -o  ${BUILD}/test_arena

test_hashmap: scratch/test_hashmap.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} -Wstrict-aliasing=2 -fstrict-aliasing scratch/test_hashmap.c -lm -o  ${BUILD}/test_hashmap

test_lexer: scratch/test_lexer.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/test_lexer.c -o  ${BUILD}/test_lexer

${BUILD}/struct_printer: tools/struct_printer.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} tools/struct_printer.c -o  ${BUILD}/struct_printer

test_struct_printer: ${BUILD}/struct_printer scratch/test_struct_printer.c
	${BUILD}/struct_printer scratch/test_struct_printer.c gen                     \
		&& ${CC} ${CFLAGS} ${DEBUGFLAGS} ${INCLUDE} scratch/test_struct_printer.c \
			-o ${BUILD}/test_struct_printer                                       \
		&& ${BUILD}/test_struct_printer

@PHONY=release
release: scratch/main.c src/*.h
	${CC} ${CFLAGS} ${RELEASEFLAGS} ${INCLUDE} scratch/main.c -lm -o  ${BUILD}/main

@PHONY=hashmap_release
hashmap_release: scratch/test_hashmap.c src/*.h
	${CC} ${CFLAGS} ${RELEASEFLAGS} ${INCLUDE} scratch/test_hashmap.c -lm -o  ${BUILD}/test_hashmap

@PHONY=run
run: main
	${BUILD}/main

@PHONY=test_run
test_run: test
	${BUILD}/test
