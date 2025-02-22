#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comments.h"
#include "consts.h"
#include "file_contents.h"
#include "gui.h"
#include "tokens.h"
#include "tui.h"

void help() {
  printf("NAME\n\t%s - put (colored) text to screen\n", PROG_NAME);
  printf("\nSYNOPSIS\n\t%s FILE\n", PROG_NAME);
  printf("\nOPTIONS\n");
  printf("\t%s\n\t\tprint help\n", "-h, -help, --help, help");
  printf("\t%s\n\t\tprint version\n", "-v, -version, --version, version");
  printf("\nEXAMPLES\n");
  printf("\t* TODO:\n");
  printf("\nAUTHOR\n");
  printf("\tMeelis Utt (meelis.utt@gmail.com)\n");
}

enum MODE { MODE_GUI = 0, MODE_TUI, MODE_TOKENS, MODE_COUNT };

int main(int argc, char **argv) {

  char *filename = NULL;
  enum MODE mode = MODE_GUI;

  for (int i = 1; i < argc; ++i) {
    char *flag = argv[i];
    if (strcmp("-h", flag) == 0 || strcmp("help", flag) == 0 ||
        strcmp("-help", flag) == 0 || strcmp("--help", flag) == 0) {
      help();
      return 0;
    } else if (strcmp("-v", flag) == 0 || strcmp("version", flag) == 0 ||
               strcmp("-version", flag) == 0 ||
               strcmp("--version", flag) == 0) {
      printf("version: %s\n", VERSION);
      return 0;
    } else if (strcmp("--tui", flag) == 0) {
      mode = MODE_TUI;
    } else if (strcmp("--tokens", flag) == 0) {
      mode = MODE_TOKENS;
    } else if (i == argc - 1) {
      filename = flag;
    } else {
      fprintf(stderr, "unexpected flag '%s'\n", flag);
      return 1;
    }
  }

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
    code_keywords = (char **)go_keywords;
    code_keywords_count = GO_KEYWORDS_COUNT;

  } else if (strcmp(ext, "py") == 0) {
    line_comment = py_style_line_comment();
    code_keywords = (char **)py_keywords;
    code_keywords_count = PY_KEYWORDS_COUNT;

  } else if (strcmp(ext, "md") == 0) {
    block_comment = html_style_block_comment();
    code_keywords = (char **)md_keywords;
    code_keywords_count = MD_KEYWORDS_COUNT;

  } else if (strcmp(ext, "html") == 0) {
    block_comment = html_style_block_comment();
  }

  int contents_len = 0;
  char *contents = read_contents(filename, &contents_len);

  TokenizerConfig *tokenizer_config = calloc(1, sizeof(TokenizerConfig));
  //
  tokenizer_config->code_keywords = (const char **)code_keywords;
  tokenizer_config->code_keywords_count = (const int)code_keywords_count;
  //
  tokenizer_config->comment_keywords = (const char **)comment_keywords;
  tokenizer_config->comment_keywords_count = (const int)COMMENT_KEYWORDS_COUNT;
  //
  tokenizer_config->line_comment = (const Comment *)line_comment;
  tokenizer_config->block_comment = (const Comment *)block_comment;
  //
  tokenizer_config->color_comment_keywords = true;
  tokenizer_config->color_strings = true;

  int tokens_count = 0;
  Token **tokens =
      tokenize(contents, contents_len, tokenizer_config, &tokens_count);

  int ret = 0;
  switch (mode) {
  case MODE_GUI:
    ret = gui_loop(filename, tokenizer_config);
    break;
  case MODE_TUI:
    ret = tui_loop(filename, tokenizer_config);
    break;
  case MODE_TOKENS:
    print_buffer = calloc(PRINT_BUFFER_SIZE, sizeof(char));
    print_tokens(tokens, tokens_count);
    if (print_buffer != NULL) {
      free(print_buffer);
    }
  case MODE_COUNT:
    break;
  }

  if (ext != NULL) {
    free(ext);
  }
  free_tokenizer_config(tokenizer_config);

  return ret;
}