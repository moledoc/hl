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
	- [x] code keywords
		- [x] c/cpp/h
		- [x] golang
		- [x] python
		- [x] markdown
		- [ ] others when needed
	- [x] golden file testing
- [x] TUI - print text to terminal
	- intermediate step, serves as backup to GUI
	- [x] auto-update code when it's modified
- [ ] GUI with SDL2
	- [x] non-colored text
	- [x] auto-update code when it's modified
	- [x] horizontal/vertical scrolling (unbounded)
	- [x] font increase/decrease
		- [x] ctrl+plus(or ctrl+equal for convenience)/ctrl+minus
		- [x] ctrl+mouse-wheel
		- [x] reset to default
	- [x] keyword based highlighting in GUI
		- [ ] multi-line strings are not properly handling newline chars
	- [ ] bounded horizontal/vertical scrolling
	- [ ] highlighting text with mouse
	- [ ] copy-paste highlighted text
	- [ ] UTF handling
	- [ ] able to specify font or change it on the fly
- [ ] building
	- [x] `make
	- [ ] something inspired by [nobuild](https://github.com/tsoding/nob.h) instead of `make`
- [ ] commandline arguments to toggle different functionality; eg tui vs gui mode, overriding comment/code style, etc
- [ ] placeholder
- [ ] proper readme
- [ ] make it work on other platforms than linux (grep ifdef OSX)

## Some Resources

- http://thenumb.at/cpp-course/sdl2/07/07.html
- http://thenumb.at/cpp-course/sdl2/08/08.html
- https://dev.to/deusinmachina/sdl-tutorial-in-c-part-2-displaying-text-o55
- https://dev.to/noah11012/using-sdl2-opening-a-window-79c


## Author

Meelis Utt