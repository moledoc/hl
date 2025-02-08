#include "file_contents.h"
#include "tokens.h"
#include "tui.h"

int main(int argc, char **argv) {
  char *filename = argv[1];
  char *contents = read_contents(filename);
  time_t last_modified = get_last_modified(filename);

  // printf("HERE:\n-----------\n%s\n", contents);

  int tokens_count = 0;
  Token **tokens = tokenize(contents, strlen(contents), &tokens_count);
  // print_buffer = calloc(PRINT_BUFFER_SIZE, sizeof(char));
  // print_tokens(tokens, tokens_count);
  tui_print(tokens, tokens_count);
  // free(print_buffer);
  // free_tokens(tokens, tokens_count);
  // return 0;

  for (int i = 0; i < 10; i += 1) {
    bool was_refreshed = false;
    contents =
        check_contents(filename, contents, &last_modified, &was_refreshed);
    if (was_refreshed) {
      tokens_count = 0;
      tokens = tokenize(contents, strlen(contents), &tokens_count);
      tui_print(tokens, tokens_count);
      // printf("UPDATED\n");
      // printf("HERE:\n-----------\n%s\n", contents);
    }
    sleep(5);
  }
}
