main: main.c *.h
	gcc -Wall -Wextra -ggdb main.c -o main -lm

@PHONY=release
release: main.c *.h
	gcc -Wall -Wextra -O3 -DMIGI_DISABLE_ASSERTS main.c -o main

@PHONY=run
run: main
	./main
