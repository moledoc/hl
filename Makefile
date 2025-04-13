
dirs:
	mkdir -p bin tests/out

build: dirs
	clang -Wall -o ./bin/hl ./main.c -I/usr/include/SDL2 -D_REENTRANT -lm -lSDL2 -lSDL2_ttf

vendored-build: dirs
	clang -Wall -o ./bin/hl ./main.c -lm `PKG_CONFIG_PATH="./vendor/SDL2/lib/pkgconfig" pkg-config --cflags --libs sdl2 SDL2_ttf`

record_all: build
	find tests/in -type f | parallel 'export filename=$$(basename {}) && test -n $${filename} && ./bin/hl --tokens --color-numbers -f {} > ./tests/golden/$${filename} && echo "recorded {} to ./tests/golden/$${filename} - done"'

test_out: build
	for filename in $$(find tests/in -type f | parallel 'basename {}');do ./bin/hl --tokens --color-numbers -f ./tests/in/$${filename} > ./tests/out/$${filename}; done;

test: test_out
	diff ./tests/golden ./tests/out

clean:
	rm -r ./tests/out ./bin

