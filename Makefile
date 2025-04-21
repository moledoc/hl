
INCLUDES =
DEFINES =
CCFLAGS =
LDFLAGS = -lm
SDLFLAGS =

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
build: dirs
INCLUDES += 
DEFINES += -D_REENTRANT
# SDLFLAGS = `pkg-config --cflags --libs sdl2 SDL2_ttf`
endif
ifeq ($(UNAME_S),Darwin)
# CCFLAGS += -arch arm64
INCLUDES += -I/opt/homebrew/include
DEFINES += -DOSX -D_THREAD_SAFE
LDFLAGS += -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf
# SDLFLAGS = `sdl2-config --libs --cflags` -lSDL2_ttf
endif

dirs:
	mkdir -p bin tests/out

build: dirs
	clang -Wall -o ./bin/hl ./main.c ${CCFLAGS} ${INCLUDES} ${DEFINES} ${LDFLAGS} ${SDLFLAGS}

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

