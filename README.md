# hl


Tool to put (colored) text on the screen.

## Why

* Enable some highlighting in my `acme` workflow, when I need it (screensharing, visual parsing, etc).
* Seemed like an interesting project for me to expand my knowledge and skills.

## Roadmap

- [x] tokenizing file
	- [x] based on extension
		- [ ] able to override the style
	- [x] words/strings/numbers/tabs/spaces/newlines
	- [x] comments
	- [x] comment keywords
	- [ ] code keywords
		- [x] c/cpp/h
		- [x] golang
		- [x] python
		- [x] markdown
		- [ ] others when needed
	- [x] golden file testing
- [x] TUI - print text to terminal
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
	- [ ] copy-paste highlighted text
	- [ ] UTF handling
- [ ] building
	- [x] `make
	- [ ] something inspired by [nobuild](https://github.com/tsoding/nob.h) instead of `make`
- [ ] placeholder
- [ ] proper readme
- [ ] make it work on other platforms than linux (grep ifdef OSX)

## Author

Meelis Utt