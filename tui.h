#pragma once

#include "tokens.h"

void tui_print(Token **tokens, int tokens_count) {
  system("clear"); // NOTE: linux specific atm

  for (int i = 0; i < tokens_count; i += 1) {
    if (tokens[i]->t == TOKEN_STRING) {
      printf("\033[32m"); // GREEN
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK
    } else {
      printf("%s", tokens[i]->v);
    }
  }
}