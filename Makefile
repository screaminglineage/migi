BUILD := ./build
CC := gcc
# CC := clang -fdiagnostics-color=always
INCLUDE := -I./src
DEBUGFLAGS := -ggdb -DMIGI_DEBUG_LOGS
CFLAGS := -Wall -Wextra -Wno-unused-function -Wno-override-init
LINKMATH := -lm
LINKFLAGS := 
SANITIZERS := -fsanitize=undefined,address
OUT := -o ${BUILD}
RELEASEFLAGS := -O3 -DMIGI_DISABLE_ASSERTS


main: scratch/main.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/main.c ${LINKMATH}  ${OUT}/main ${LINKFLAGS}

build: scratch/build.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/build.c ${LINKMATH}  ${OUT}/build ${LINKFLAGS}

walk_dir: scratch/walk_dir.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/walk_dir.c ${LINKMATH}  ${OUT}/walk_dir ${LINKFLAGS}

test: scratch/test.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/test.c ${LINKMATH} ${OUT}/test ${LINKFLAGS}

test_array_list: scratch/test_array_list.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/test_array_list.c ${OUT}/test_array_list ${LINKFLAGS}

test_arena: scratch/test_arena.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/test_arena.c ${LINKMATH} ${OUT}/test_arena ${LINKFLAGS}

test_hashmap: scratch/test_hashmap.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} -Wstrict-aliasing=2 -fstrict-aliasing scratch/test_hashmap.c -lm ${OUT}/test_hashmap ${LINKFLAGS}

test_lexer: scratch/test_lexer.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/test_lexer.c ${OUT}/test_lexer ${LINKFLAGS}

msi-hashmap: scratch/msi-hashmap.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/msi-hashmap.c ${OUT}/msi-hashmap ${LINKFLAGS}

${BUILD}/struct_printer: tools/struct_printer.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${INCLUDE} tools/struct_printer.c ${OUT}/struct_printer ${LINKFLAGS}

test_struct_printer: ${BUILD}/struct_printer scratch/test_struct_printer.c
	${BUILD}/struct_printer scratch/test_struct_printer.c gen                                  \
		&& ${CC} ${CFLAGS} ${DEBUGFLAGS} ${INCLUDE} scratch/test_struct_printer.c ${LINKFLAGS} \
			${OUT}/test_struct_printer                                                         \
		&& ${BUILD}/test_struct_printer

@PHONY=release
release: scratch/main.c src/*.h
	${CC} ${CFLAGS} ${RELEASEFLAGS} ${INCLUDE} scratch/main.c ${LINKMATH} ${OUT}/main ${LINKFLAGS}

@PHONY=hashmap_release
hashmap_release: scratch/test_hashmap.c src/*.h
	${CC} ${CFLAGS} ${RELEASEFLAGS} ${INCLUDE} scratch/test_hashmap.c ${LINKMATH} ${OUT}/test_hashmap ${LINKFLAGS}

@PHONY=run
run: main
	${BUILD}/main

@PHONY=test_run
test_run: test
	${BUILD}/test
