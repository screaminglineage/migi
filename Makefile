main: main.c src/*.h
	gcc -Wall -Wextra -ggdb -I./src/ main.c -o main -lm

@PHONY=release
release: main.c src/*.h
	gcc -Wall -Wextra -O3 -I./src/ -DMIGI_DISABLE_ASSERTS main.c -o main

@PHONY=run
run: main
	./main
