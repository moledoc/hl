#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../file_contents.h"

#ifndef TESTING
#define TESTING
#endif // TESTING
#include "../comments.h"
#include "../tokens.h"

const int expected_nr_of_args = 2;

int main(int argc, char **argv) {
  if (argc != expected_nr_of_args) {
    fprintf(stderr, "unexpected number of arguments: expected %d, got %d\n",
            expected_nr_of_args, argc);
    return 1;
  }

  char *filename = argv[1];

  if (filename == NULL) {
    fprintf(stderr, "missing required argument 'FILE'\n");
    return 1;
  }

  if (!file_exists(filename)) {
    fprintf(stderr, "specified file '%s' doesn't exist\n", filename);
    return 1;
  }

  char *ext = file_ext(filename);

  Comment *line_comment = NULL;
  Comment *block_comment = NULL;
  char **code_keywords = NULL;
  int code_keywords_count = 0;

  if (strcmp(ext, "c") == 0 || strcmp(ext, "cpp") == 0 ||
      strcmp(ext, "h") == 0) {
    line_comment = c_style_line_comment();
    block_comment = c_style_block_comment();
    code_keywords = (char **)c_keywords;
    code_keywords_count = C_KEYWORDS_COUNT;

  } else if (strcmp(ext, "go") == 0) {
    line_comment = c_style_line_comment();
    block_comment = c_style_block_comment();
    // TODO: go keywords

  } else if (strcmp(ext, "py") == 0) {
    line_comment = py_style_line_comment();
    //
    // TODO: python keywords

  } else if (strcmp(ext, "html") == 0 || strcmp(ext, "md") == 0) {
    block_comment = html_style_block_comment();
  }

  int contents_len = 0;
  char *contents = read_contents(filename, &contents_len);

  TokenizerConfig *tokenizer_config = calloc(1, sizeof(TokenizerConfig));
  tokenizer_config->code_keywords = (const char **)code_keywords;
  tokenizer_config->code_keywords_count = (const int)code_keywords_count;
  tokenizer_config->line_comment = (const Comment *)line_comment;
  tokenizer_config->block_comment = (const Comment *)block_comment;

  int tokens_count = 0;
  Token **tokens =
      tokenize(contents, contents_len, tokenizer_config, &tokens_count);

  print_buffer = calloc(PRINT_BUFFER_SIZE, sizeof(char));
  print_tokens(tokens, tokens_count);
  if (print_buffer != NULL) {
    free(print_buffer);
  }

  if (ext != NULL) {
    free(ext);
  }
  free_comment(line_comment);
  free_comment(block_comment);
  if (tokenizer_config != NULL) {
    free(tokenizer_config);
  }
}