#pragma once

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
    } else if (tokens[i]->t == TOKEN_KEYWORD) {
      printf("\033[34m"); // BLUE FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else if (tokens[i]->t == TOKEN_COMMENT_KEYWORD) {
      printf("\033[33m"); // YELLOW FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else {
      printf("%s", tokens[i]->v);
    }
  }
}

int tui_loop(char *filename, const char **keywords, const int keyword_count,
             bool comment_kw) {
  struct sigaction act;
  act.sa_handler = graceful_shutdown;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGKILL, &act, NULL);

  char *contents = read_contents(filename);
  time_t last_modified = get_last_modified(filename);
  int tokens_count = 0;
  Token **tokens = tokenize(contents, strlen(contents), keywords, keyword_count,
                            comment_kw, &tokens_count);

  tui_print(tokens, tokens_count);
  // print_buffer = calloc(tokens_count, sizeof(char *)); // REMOVEME:
  // print_tokens(tokens, tokens_count); // REMOVEME:

  while (TUI_KEEP_RUNNING) {
    usleep(TUI_REFRESH_RATE);
    bool was_refreshed = false;
    contents =
        check_contents(filename, contents, &last_modified, &was_refreshed);
    if (was_refreshed) {
      tokens_count = 0;
      tokens = tokenize(contents, strlen(contents), keywords, keyword_count,
                        comment_kw, &tokens_count);
      tui_print(tokens, tokens_count);
    }
  }
  free_tokens(tokens, tokens_count);
  free_contents(contents);
  return 0;
}