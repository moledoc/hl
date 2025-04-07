# hl

**This project is WIP and used for exploration**

Text displaying tool (to enhance acme workflow) - might become something else in the future.

## Why

* Enable some highlighting in my `acme` workflow, when I need it (screensharing, visual parsing, etc).
* Seemed like an interesting project for me to expand my knowledge and skills.

## TODO:

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
	- MAYBE: scoping tokens. i.e. currently stuff in backticks are not easily highlightable via double click
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
		- [x] multi-line strings are not properly handling newline chars
	- [x] bounded horizontal/vertical scrolling
		- [x] extension: horizontal scrolling only when text doesn't fit the screen
		- [x] extension: able to specify scrolling factor
		- [x] extension: snap back if text fits on screen, but horizontal scroll is non-zero
	- [x] scrollbar (horizontal/vertical)
		- [ ] acme inspired movements on scrollbar
	- [ ] ~~when font is resized, keep same place in view~~
		- [x] smaller font increment instead
	- [x] highlighting text with mouse
		- [x] token based highlighting
		- [x] keep highlighting after letting go, stop highlighting after click
		- [x] char based highlighting
		- [x] better accuracy on highlighting
		- [x] mouse scrolling highlight previously highlighted area, instead of moving along
			 - [x] store beginning token instead of beginning mouse pos
		- [x] acme-inspired highlighting (inside quotes, parenthesis, brackets etc)
		- [x] double-click to highlight a word
		- [ ] triple-click or double click beginning/end of line to highlight a line
	- [x] copy-paste highlighted text
	- [x] colorschemes; currently:
		- [x] light (default)
		- [x] dark
		- [x] gruvbox-light
		- [x] gruvbox-dark
		- [x] solarized-light
		- [x] solarized-dark
	- [ ] ~~soft-wrap text~~
	- [x] row and col numbers
		- [x] goto line nr
		- MAYBE: `jump to <path>:<line>:<col>` support
	- [x] text search
	- [ ] display cursor
	- [ ] vendor SDL2 and SDL2_ttf to simplify dependencies
	- [ ] move tokens and textures to state
	- [ ] use linked lists for holding tokens/textures instead of array
		- rationale: my hope is that this would enable reasonable enough text editing for this project
	- [ ] UTF handling
	- [ ] able to specify font
		- [ ] cmd args 
		- [ ] on the fly
	- [ ] specify color schemes
		- [ ] cmd args
		- [ ] on the fly
	- [ ] some acme-inspired mouse actions
		- TBD
	- [ ] some keyboard shortcuts for navigation
		- [x] ctrl+d/ctrl+u = jump half a page down/up (vim inspired)
		- [x] ctrl+a/ctrl+e = jump to beginning/end of file
		- TBD
	- [ ] MAYBE: MAYBE: lsp integration
	- [ ] MAYBE: possible performance optimization
		- [x] when font changes scale by factor first, and when the font persists for some time, calculate the textures again.
		- MAYBE: instead of list (of textures/tokens):
			- linked list
			- map
		- MAYBE: compute values/obj only once, that don't need to be recomputed
		- [x] render only rows fitting on window
		- [x] render only columns fitting on window
- [ ] fix mem-leaks
- [ ] building
	- [x] `make
	- [ ] something inspired by [nobuild](https://github.com/tsoding/nob.h) instead of `make`
- [x] merge tokenize_test into the main file
- [x] commandline arguments to toggle different functionality; eg tui vs gui mode, overriding comment/code style, etc
	- [x] override what to color/what not to color
- [ ] resolve TODOs and others
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