#pragma once

#include "tokens.h"

void tui_print(Token **tokens, int tokens_count) {
  system("clear"); // NOTE: linux specific atm

  for (int i = 0; i < tokens_count; i += 1) {
    if (tokens[i]->t == TOKEN_STRING) {
      printf("\033[32m"); // GREEN FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else if (tokens[i]->t == TOKEN_NUMBER) {
      printf("\033[35m"); // BLUE FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else if (tokens[i]->t == TOKEN_KEYWORD) {
      printf("\033[34m"); // <TODO> FOREGROUND
      printf("%s", tokens[i]->v);
      printf("\033[30m"); // BLACK FOREGROUND
    } else {
      printf("%s", tokens[i]->v);
    }
  }
}