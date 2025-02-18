# TODO: make better or better yet, use alternative

CC = clang
CFLAGS = -Wall -Wsanitize

dirs:
	mkdir -p bin tests/out

record_all: dirs
	clang -o ./bin/tokenize_test ./tests/tokenize_test.c -DTESTING
	find tests/in -type f | parallel 'export filename=$$(basename {}) && test -n $${filename} && ./bin/tokenize_test {} > ./tests/golden/$${filename} && echo "recorded {} to ./tests/golden/$${filename} - done"'

test_out:
	clang -o ./bin/tokenize_test ./tests/tokenize_test.c -DTESTING
	for filename in $$(find tests/in -type f | parallel 'basename {}');do ./bin/tokenize_test ./tests/in/$${filename} > ./tests/out/$${filename}; done;

test: test_out
	diff ./tests/golden ./tests/out

clean:
	rm -r ./tests/out ./bin

debug: dirs
	clang -g -o ./bin/debugging ./main.c -I/usr/include/SDL2 -D_REENTRANT -lSDL2 -lSDL2_ttf
    # gdb --args ./bin/debugging tests/manual_cases.txt

build: dirs
	clang -o ./bin/hl ./main.c -I/usr/include/SDL2 -D_REENTRANT -lSDL2 -lSDL2_ttf
