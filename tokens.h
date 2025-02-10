#pragma once

#include "keywords.h"

#define PRINT_BUFFER_SIZE 1024
char *print_buffer =
    NULL; // NOTE: how to set: calloc(PRINT_BUFFER_SIZE, sizeof(char));

enum TOKEN_TYPE {
  TOKEN_WORD = 0,
  TOKEN_STRING,
  TOKEN_NUMBER,
  TOKEN_KEYWORD,
  TOKEN_COMMENT_KEYWORD,
  TOKEN_TOKEN_COUNT
};

static const char *TOKEN_NAMES[] = {"TOKEN_WORD", "TOKEN_STRING",
                                    "TOKEN_NUMBER", "TOKEN_KEYWORD",
                                    "TOKEN_COMMENT_KEYWORD"};

typedef struct {
  enum TOKEN_TYPE t;
  char *v;
  int vlen;
} Token;

Token **tokenize(char *contents, int contents_length, const char **keywords,
                 const int keyword_count, bool comment_kw, int *tokens_count) {
  Token **tokens = calloc(contents_length + 1, sizeof(Token *));

  int prev_offset = 0;
  int offset = 0;
  while (offset < contents_length) {

    Token *token = calloc(1, sizeof(Token));
    token->t = TOKEN_WORD;

    if (contents[offset] == '"' || contents[offset] == '\'') {
      char closing_quote = contents[offset];
      token->t = TOKEN_STRING;
      offset += 1; // account for opening quote
      if (offset >= contents_length) {
        offset = contents_length;
      }
      bool escaped = false;
      char c;
      while (offset < contents_length && (c = contents[offset]) &&
             (c == closing_quote && escaped || c != closing_quote)) {
        escaped = contents[offset - 1] != '\\' && c == '\\';
        offset += 1;
      }
      offset += 1; // account for closing quote
    } else {
      char c;
      while (offset < contents_length && (c = contents[offset]) &&
             ('a' <= c && c <= 'z' || 'A' <= c && c <= 'Z' ||
              '0' <= c && c <= '9' || c == '+' || c == '-' || c == '.')) {
        offset += 1;
      }
    }

    if (offset >= contents_length) {
      offset = contents_length;
    }

    if (prev_offset == offset) {
      offset += 1;
    }

    int vlen = offset - prev_offset;

    token->v = calloc(vlen + 1, sizeof(char));
    token->vlen = vlen;
    memcpy(token->v, contents + prev_offset, vlen);

    {
      // TODO: handle zero as string more generally
      if (strtof(token->v, NULL) != 0 ||
          (token->vlen == 1 && *(token->v) == '0')) {
        token->t = TOKEN_NUMBER;
      }
    }

    {
      if (token->t == TOKEN_WORD) {
        for (int i = 0; i < keyword_count; i += 1) {
          if (strcmp(token->v, keywords[i]) == 0) {
            token->t = TOKEN_KEYWORD;
            break;
          }
        }
      }
    }

    {
      if (comment_kw && token->t == TOKEN_WORD) {
        for (int i = 0; i < COMMENT_KEYWORD_COUNT; i += 1) {
          if (strcmp(token->v, comment_keywords[i]) == 0) {
            token->t = TOKEN_COMMENT_KEYWORD;
            break;
          }
        }
      }
    }

    tokens[*tokens_count] = token;
    *tokens_count += 1;

    prev_offset = offset;
  }

  // NOTE: if we dont end with newline token,
  // then the last line is not printed in tui.
  // To get around it, we add the token, if needed.
  if (*tokens_count > 1 && strcmp(tokens[*tokens_count - 1]->v, "\n") != 0) {
    Token *eof_token = calloc(1, sizeof(Token));
    char *v = calloc(1, sizeof(char));
    memcpy(v, "\n", 1);
    eof_token->v = v;
    eof_token->vlen = 1;
    eof_token->t = TOKEN_WORD;
    tokens[*tokens_count] = eof_token;
    *tokens_count += 1;
  }

  return tokens;
}

void free_tokens(Token **tokens, int tokens_count) {
  for (int i = 0; i < tokens_count; i += 1) {
    if (tokens[i] != NULL) {
      if (tokens[i]->v != NULL) {
        free(tokens[i]->v);
      }
      free(tokens[i]);
      tokens[i] = NULL;
    }
  }
  free(tokens);
}

void print_tokens(Token **tokens, int tokens_count) {
  if (print_buffer == NULL) {
    printf("[WARNING]: print_buffer not allocated, not printing any tokens\n");
    return;
  }
  memset(print_buffer, 0, PRINT_BUFFER_SIZE);
  for (int i = 0; i < tokens_count; ++i) {
    Token *token = tokens[i];
    sprintf(print_buffer, "%s(%s)(%d)", TOKEN_NAMES[token->t], token->v,
            token->vlen);
    printf("%s\n", print_buffer);
  }
}
