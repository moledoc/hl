# hl


Tool to put (colored) text on the screen.

## Why

* Enable some highlighting in my `acme` workflow, when I need it (screensharing, visual parsing, etc).
* Seemed like an interesting project for me to expand my knowledge and skills.

## Roadmap

- [ ] tokenizing file
	- [ ] golden file testing
- [ ] TUI - print text to terminal
	- intermediate step, serves as backup to GUI
- [ ] GUI with SDL2
	- [ ] non-colored text
	- [ ] font increase/decrease
		- [ ] ctrl+plus(or ctrl+equal for convenience)/ctrl+minus
		- [ ] ctrl+mouse-wheel
	- [ ] horizontal/vertical scrolling
	- [ ] bounded horizontal/vertical scrolling
	- [ ] highlighting text with mouse
	- [ ] keyword based highlighting in GUI
		- [ ] C/C++ (`.c`, `.h`, `cpp`, ...)
		- [ ] go (`.go`)
		- [ ] python (`.py`)
		- [ ] html (`.html`, `.md`)
		- [ ] line and block comments per language
	- [ ] copy-paste highlighted text
	- [ ] UTF handling
- [ ] building
	- `make` vs something inspired by [nobuild](https://github.com/tsoding/nob.h)?
- [ ] placeholder
- [ ] proper readme

## Author

Meelis Utt