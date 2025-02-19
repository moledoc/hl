#pragma once

// TODO: other OS support
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include "file_contents.h"
#include "tokens.h"

#define TUI_REFRESH_RATE 250000 // in microseconds

static volatile bool TUI_KEEP_RUNNING = true;

void graceful_shutdown(int _) { TUI_KEEP_RUNNING = false; }

void tui_print(Token **tokens, int tokens_count) {
  system("clear"); // NOTE: linux specific atm // TODO: support other os

  for (int i = 0; i < tokens_count; i += 1) {
    if (tokens[i]->t == TOKEN_STRING) {
      printf("\033[32m"); // GREEN FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else if (tokens[i]->t == TOKEN_NUMBER) {
      printf("\033[35m"); // MAGENTA FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else if (tokens[i]->t == TOKEN_CODE_KEYWORD) {
      printf("\033[34m"); // BLUE FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else if (tokens[i]->t == TOKEN_COMMENT_KEYWORD) {
      printf("\033[33m"); // YELLOW FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else if (tokens[i]->t == TOKEN_COMMENT) {
      printf("\033[90m"); // GREY FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else {
      printf("%s", tokens[i]->v);
    }
  }
}

int tui_loop(char *filename, TokenizerConfig *tokenizer_config) {

  if (tokenizer_config == NULL) {
    fprintf(stderr, "empty tokenizer config\n");
    return 1;
  }

  struct sigaction act;
  act.sa_handler = graceful_shutdown;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGKILL, &act, NULL);

  int contents_len = 0;
  char *contents = read_contents(filename, &contents_len);
  time_t last_modified = get_last_modified(filename);
  int tokens_count = 0;
  Token **tokens =
      tokenize(contents, contents_len, tokenizer_config, &tokens_count);

  tui_print(tokens, tokens_count);

  while (TUI_KEEP_RUNNING) {
    usleep(TUI_REFRESH_RATE);
    bool was_refreshed = false;
    contents = check_contents(filename, contents, &contents_len, &last_modified,
                              &was_refreshed);
    if (was_refreshed) {
      tokens = update_tokens(tokens, contents, contents_len, tokenizer_config,
                             &tokens_count);
      tui_print(tokens, tokens_count);
    }
  }
  free_tokens(tokens, tokens_count);
  free_contents(contents);
  return 0;
}