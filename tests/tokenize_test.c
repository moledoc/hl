#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../file_contents.h"
#include "../utils.h"

#ifndef TESTING
#define TESTING
#endif // TESTING
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

  // TODO: other platforms
  if (access(filename, F_OK) != 0) {
    fprintf(stderr, "specified file '%s' doesn't exist\n", filename);
    return 1;
  }

  char ext[32] = {0};
  if (*ext == 0) {
    char *filename_dup = strdup(filename); // allocs memory
    const char *dot = strrchr(filename_dup, '.');
    if (dot != NULL && dot != filename_dup) {
      memcpy(ext, dot + 1, min(sizeof(ext), strlen(dot + 1)));
    }
    free(filename_dup); // free strdup
  }

  Comment *line_comment = NULL;
  Comment *block_comment = NULL;
  char **code_keywords = NULL;
  int code_keywords_count = 0;

  if (strcmp(ext, "c") == 0 || strcmp(ext, "cpp") == 0 ||
      strcmp(ext, "h") == 0) {
    line_comment = calloc(1, sizeof(Comment));
    line_comment->begin = "//";
    line_comment->begin_len = strlen("//");
    line_comment->end = "\n";
    line_comment->end_len = strlen("\n");
    //
    block_comment = calloc(1, sizeof(Comment));
    block_comment->begin = "/*";
    block_comment->begin_len = strlen("/*");
    block_comment->end = "*/";
    block_comment->end_len = strlen("*/");
    //
    code_keywords = (char **)c_keywords;
    code_keywords_count = C_KEYWORD_COUNT;

  } else if (strcmp(ext, "py") == 0) {
    line_comment = calloc(1, sizeof(Comment));
    line_comment->begin = "#";
    line_comment->begin_len = strlen("#");
    line_comment->end = "\n";
    line_comment->end_len = strlen("\n");
    //
    // TODO: python keywords

  } else if (strcmp(ext, "html") == 0 || strcmp(ext, "md") == 0) {
    block_comment = calloc(1, sizeof(Comment));
    block_comment->begin = "<!--";
    block_comment->begin_len = strlen("<!--");
    block_comment->end = "-->";
    block_comment->end_len = strlen("-->");
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

  if (line_comment != NULL) {
    free(line_comment);
  }
  if (block_comment != NULL) {
    free(block_comment);
  }
  if (tokenizer_config != NULL) {
    free(tokenizer_config);
  }
}