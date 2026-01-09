BUILD := ./build

ifeq ($(OS), Windows_NT)
	# Extremely cursed but cant find any other way to solve this
	# vcvars64.bat needs to be ran to prepare the environment for `cl` and `link` to work properly (find includes, etc.)
	# However this cannot be a separate recipe since Make runs each recipe in a subshell and any changes made by
	# the script to the environment will be lost after it exits. Thus each command needs to be prefixed with running this script
	CC := "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cl
	INCLUDE := /I./src
	DEBUGFLAGS := /Zi
	CFLAGS := /nologo /W4 /wd4200 /wd4146 /wd4127
	LINKMATH :=
	LINKFLAGS := /link /INCREMENTAL:NO
	SANITIZERS := /fsanitize=address
	OUT := /Fe${BUILD}
	RELEASEFLAGS := /O2 /DMIGI_DISABLE_ASSERTS
else
	CC := gcc
	# CC := clang -fdiagnostics-color=always
	INCLUDE := -I./src
	DEBUGFLAGS := -ggdb
	CFLAGS := -Wall -Wextra -Wno-unused-function -Wno-override-init
	LINKMATH := -lm
	LINKFLAGS := 
	SANITIZERS := -fsanitize=undefined,address
	OUT := -o ${BUILD}
	RELEASEFLAGS := -O3 -DMIGI_DISABLE_ASSERTS
endif


main: scratch/main.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} scratch/main.c ${LINKMATH}  ${OUT}/main ${LINKFLAGS}

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

${BUILD}/struct_printer: tools/struct_printer.c src/*.h
	${CC} ${CFLAGS} ${DEBUGFLAGS} ${SANITIZERS} ${INCLUDE} tools/struct_printer.c ${OUT}/struct_printer ${LINKFLAGS}

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
