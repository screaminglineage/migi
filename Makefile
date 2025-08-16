main: main.c src/*.h
	gcc -Wall -Wextra -ggdb -I./src main.c -o main -lm -fsanitize=undefined

test_hashmap: test_hashmap.c src/*.h
	gcc -Wall -Wextra -Wno-unused-function -ggdb -I./src test_hashmap.c -o test_hashmap -fsanitize=undefined

test_lexer: test_lexer.c src/*.h
	gcc -Wall -Wextra -Wno-unused-function -ggdb -I./src test_lexer.c -o test_lexer -fsanitize=undefined

struct_printer: tools/struct_printer.c src/*.h
	gcc -Wall -Wextra -Wno-unused-function -ggdb -I./src tools/struct_printer.c -o struct_printer -fsanitize=undefined

test_struct_printer: struct_printer test_struct_printer.c
	./struct_printer test_struct_printer.c gen                                                 \
		&& gcc -Wall -Wextra -Wno-unused-function test_struct_printer.c -o test_struct_printer \
		&& ./test_struct_printer

@PHONY=release
release: main.c src/*.h
	gcc -Wall -Wextra -O3 -I./src -DMIGI_DISABLE_ASSERTS main.c -o main

@PHONY=hashmap-release
hashmap_release: test_hashmap.c src/*.h
	gcc -Wall -Wextra -O3 -Wno-unused-function -I./src -DMIGI_DISABLE_ASSERTS test_hashmap.c -o test_hashmap

@PHONY=run
run: main
	./main
