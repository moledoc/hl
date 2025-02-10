#include "file_contents.h"
#include "tokens.h"
#include "tui.h"

int main(int argc, char **argv) {
  char *filename = argv[1];
  tui_loop(filename);
}
