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
  /*
    printf("\nSYNOPSIS\n\t%s <TODO:>\n", PROG_NAME);
    printf("\nOPTIONS\n");
    printf("\t%s\n\t\tprint help\n", "-h, -help, --help, help");
    printf("\t%s\n\t\tprint version\n", "-v, -version, --version, version");
    printf("\nEXAMPLES\n");
  */
  printf("\t* TODO:\n");
  printf("\nAUTHOR\n");
  printf("\tMeelis Utt (meelis.utt@gmail.com)\n");
}

enum MODE { MODE_GUI = 0, MODE_TUI, MODE_TOKENS, MODE_COUNT };
enum COLOR { COLOR_NOT_SET = -1, COLOR_NO = false, COLOR_YES = true };

int main(int argc, char **argv) {

  char *filename = NULL;
  enum MODE mode = MODE_GUI;
  enum COLOR color_code_keywords = COLOR_NOT_SET;
  enum COLOR color_comment_keywords = COLOR_NOT_SET;
  enum COLOR color_numbers = COLOR_NOT_SET;
  enum COLOR color_strings = COLOR_NOT_SET;
  char *color_scheme_name = NULL;

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
      //
    } else if (strcmp("--tui", flag) == 0) {
      mode = MODE_TUI;
    } else if (strcmp("--tokens", flag) == 0) {
      mode = MODE_TOKENS;
      //
    } else if (strcmp("--color-codes", flag) == 0) {
      color_code_keywords = COLOR_YES;
    } else if (strcmp("--color-comments", flag) == 0) {
      color_comment_keywords = COLOR_YES;
    } else if (strcmp("--color-numbers", flag) == 0) {
      color_numbers = COLOR_YES;
    } else if (strcmp("--color-strings", flag) == 0) {
      color_strings = COLOR_YES;
      //
    } else if (strcmp("--no-color-codes", flag) == 0) {
      color_code_keywords = COLOR_NO;
    } else if (strcmp("--no-color-comments", flag) == 0) {
      color_comment_keywords = COLOR_NO;
    } else if (strcmp("--no-color-numbers", flag) == 0) {
      color_numbers = COLOR_NO;
    } else if (strcmp("--no-color-strings", flag) == 0) {
      color_strings = COLOR_NO;
      //
    } else if ((strcmp("-f", flag) == 0 || strcmp("--file", flag) == 0) &&
               i + 1 < argc) {
      filename = argv[i + 1];
      i += 1;
    } else if (strcmp("-cs", flag) == 0 && i + 1 < argc) {
      color_scheme_name = argv[i + 1];
    } else if (filename == NULL && i == argc - 1) {
      filename = flag;
      //
    } else if (filename == NULL) {
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

  TokenizerConfig *tokenizer_config = &DEFAULT_TOKENIZER_CONFIG;

  if (strcmp(ext, "c") == 0 || strcmp(ext, "cpp") == 0 ||
      strcmp(ext, "h") == 0) {
    tokenizer_config->line_comment = c_style_line_comment();
    tokenizer_config->block_comment = c_style_block_comment();
    //
    tokenizer_config->code_keywords = c_keywords;
    tokenizer_config->code_keywords_count = C_KEYWORDS_COUNT;
    //
    tokenizer_config->comment_keywords = comment_keywords;
    tokenizer_config->comment_keywords_count = COMMENT_KEYWORDS_COUNT;
    //
    tokenizer_config->color_code_keywords = true;
    tokenizer_config->color_comment_keywords = true;
    tokenizer_config->color_numbers = true;
    tokenizer_config->color_strings = true;

  } else if (strcmp(ext, "go") == 0) {
    tokenizer_config->line_comment = c_style_line_comment();
    tokenizer_config->block_comment = c_style_block_comment();
    //
    tokenizer_config->code_keywords = go_keywords;
    tokenizer_config->code_keywords_count = GO_KEYWORDS_COUNT;
    //
    tokenizer_config->comment_keywords = comment_keywords;
    tokenizer_config->comment_keywords_count = COMMENT_KEYWORDS_COUNT;
    //
    tokenizer_config->color_code_keywords = true;
    tokenizer_config->color_comment_keywords = true;
    tokenizer_config->color_numbers = true;
    tokenizer_config->color_strings = true;

  } else if (strcmp(ext, "py") == 0) {
    tokenizer_config->line_comment = py_style_line_comment();
    //
    tokenizer_config->code_keywords = py_keywords;
    tokenizer_config->code_keywords_count = PY_KEYWORDS_COUNT;
    //
    tokenizer_config->comment_keywords = comment_keywords;
    tokenizer_config->comment_keywords_count = COMMENT_KEYWORDS_COUNT;
    //
    tokenizer_config->color_code_keywords = true;
    tokenizer_config->color_comment_keywords = true;
    tokenizer_config->color_numbers = true;
    tokenizer_config->color_strings = true;

  } else if (strcmp(ext, "md") == 0) {
    tokenizer_config->block_comment = html_style_block_comment();
    //
    tokenizer_config->code_keywords = md_keywords;
    tokenizer_config->code_keywords_count = MD_KEYWORDS_COUNT;
    //
    tokenizer_config->comment_keywords = comment_keywords;
    tokenizer_config->comment_keywords_count = COMMENT_KEYWORDS_COUNT;
    //
    tokenizer_config->color_code_keywords = true;
    tokenizer_config->color_comment_keywords = true;
    tokenizer_config->color_numbers = true;

  } else if (strcmp(ext, "html") == 0) {
    tokenizer_config->block_comment = html_style_block_comment();
    //
    tokenizer_config->comment_keywords = comment_keywords;
    tokenizer_config->comment_keywords_count = COMMENT_KEYWORDS_COUNT;
    //
    tokenizer_config->color_comment_keywords = true;
    tokenizer_config->color_numbers = true;
  }

  if (ext != NULL) {
    free(ext);
  }

  if (color_code_keywords != COLOR_NOT_SET) {
    tokenizer_config->color_code_keywords = color_code_keywords;
  };
  if (color_comment_keywords != COLOR_NOT_SET) {
    tokenizer_config->color_comment_keywords = color_comment_keywords;
  };
  if (color_numbers != COLOR_NOT_SET) {
    tokenizer_config->color_numbers = color_numbers;
  };
  if (color_strings != COLOR_NOT_SET) {
    tokenizer_config->color_strings = color_strings;
  };

  if (color_scheme_name != NULL) {
    if (strcmp(color_scheme_name, "light") == 0) {
      // color_scheme = &color_schemes[COLOR_SCHEME_LIGHT];
      color_scheme_idx = COLOR_SCHEME_LIGHT;
    } else if (strcmp(color_scheme_name, "dark") == 0) {
      // color_scheme = &color_schemes[COLOR_SCHEME_DARK];
      color_scheme_idx = COLOR_SCHEME_DARK;
    }
  };
  color_scheme = &color_schemes[color_scheme_idx];

  int ret = 0;
  if (mode == MODE_GUI) {
    ret = gui_loop(filename, tokenizer_config);
  } else if (mode == MODE_TUI) {
    ret = tui_loop(filename, tokenizer_config);
  } else if (mode == MODE_TOKENS) {
    int contents_len = 0;
    char *contents = read_contents(filename, &contents_len);

    int tokens_count = 0;
    Token **tokens =
        tokenize(contents, contents_len, tokenizer_config, &tokens_count);
    print_buffer = calloc(PRINT_BUFFER_SIZE, sizeof(char));
    print_tokens(tokens, tokens_count);
    if (print_buffer != NULL) {
      free(print_buffer);
    }
    free_tokens(tokens, tokens_count);
    if (contents != NULL) {
      free(contents);
    }
  }

  return ret;
}