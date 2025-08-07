main: main.c src/*.h
	gcc -Wall -Wextra -ggdb -I./src main.c -o main -lm -fsanitize=undefined

test_hashmap: test_hashmap.c src/*.h
	gcc -Wall -Wextra -Wno-unused-function -ggdb -I./src test_hashmap.c -o test_hashmap -fsanitize=undefined

@PHONY=release
release: main.c src/*.h
	gcc -Wall -Wextra -O3 -I./src -DMIGI_DISABLE_ASSERTS main.c -o main

@PHONY=hashmap-release
hashmap-release: test_hashmap.c src/*.h
	gcc -Wall -Wextra -O3 -Wno-unused-function -I./src -DMIGI_DISABLE_ASSERTS test_hashmap.c -o test_hashmap

@PHONY=run
run: main
	./main
