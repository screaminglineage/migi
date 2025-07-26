main: main.c *.h
	gcc -Wall -Wextra -Wno-unused-function -ggdb main.c -o main

@PHONY=release
release: main.c *.h
	gcc -Wall -Wextra -Wno-unused-function -O3 -DMIGI_DISABLE_ASSERTS main.c -o main

@PHONY=run
run: main
	./main
