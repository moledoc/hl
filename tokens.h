#pragma once

#include <stdbool.h>

#include "keywords.h"

// TODO: handle comments (line comment is just a block comment that ends with
// '\n')

// TODO: REMOVEME:
#define PRINT_BUFFER_SIZE 1024
char *print_buffer =
    NULL; // NOTE: how to set: calloc(PRINT_BUFFER_SIZE, sizeof(char));

enum TOKEN_TYPE {
  TOKEN_WORD = 0,
  TOKEN_STRING,
  TOKEN_NUMBER,
  TOKEN_KEYWORD,
  TOKEN_COMMENT_KEYWORD,
  TOKEN_COMMENT,
  TOKEN_TOKEN_COUNT
};

static const char *TOKEN_NAMES[] = {
    "TOKEN_WORD",    "TOKEN_STRING",          "TOKEN_NUMBER",
    "TOKEN_KEYWORD", "TOKEN_COMMENT_KEYWORD", "TOKEN_COMMENT"};

typedef struct {
  enum TOKEN_TYPE t;
  char *v;
  int vlen;
} Token;

typedef struct {
  char *begin;
  int begin_len;
  char *end;
  int end_len;
} Comment;

// NOTE: only handles decimals that use '.' as the separator
bool is_number(const char *s) {
  char *s_cpy = (char *)s;
  int s_len = 0;
  int dot_count = 0;
  char c;
  while ((c = *(s_cpy++)) != '\0') {
    s_len += 1;
    if (c == '.') {
      dot_count += 1;
    } else if (c < '0' || '9' < c) {
      return false;
    }
    if (dot_count > 1) {
      return false;
    }
  }
  return true;
}

bool strcmp_window(const char *s1, const char *s2, int window_size) {
  char *s1_cpy = (char *)s1;
  char *s2_cpy = (char *)s2;

  for (int i = 0; i < window_size; i += 1) {
    if (s1_cpy[i] == '\0' || s2_cpy[i] == '\0' || s1_cpy[i] != s2_cpy[i]) {
      return false;
    }
  }
  return true;
}

Token **tokenize(char *contents, int contents_length, const char **keywords,
                 const int keyword_count, bool comment_kw, int *tokens_count,
                 Comment *line_comment, Comment *block_comment) {
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

      // TODO: REFACTORME:
    } else if (line_comment != NULL &&
               offset + line_comment->begin_len < contents_length &&
               strcmp_window((const char *)(contents + offset),
                             (const char *)line_comment->begin,
                             line_comment->begin_len) == 1) {
      while (offset + line_comment->end_len < contents_length &&
             strcmp_window((const char *)(contents + offset),
                           (const char *)line_comment->end,
                           line_comment->end_len) != 1) {
        offset += 1;
      }
      offset += line_comment->end_len; // account for line_comment end
      token->t = TOKEN_COMMENT;

      // TODO: REFACTORME:
    } else if (block_comment != NULL &&
               offset + block_comment->begin_len < contents_length &&
               strcmp_window((const char *)(contents + offset),
                             (const char *)block_comment->begin,
                             block_comment->begin_len) == 1) {
      while (offset + block_comment->end_len < contents_length &&
             strcmp_window((const char *)(contents + offset),
                           (const char *)block_comment->end,
                           block_comment->end_len) != 1) {
        offset += 1;
      }
      offset += block_comment->end_len; // account for block_comment end
      token->t = TOKEN_COMMENT;

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
      if (is_number((const char *)token->v)) {
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
  if (*tokens_count > 0 && strcmp(tokens[*tokens_count - 1]->v, "\n") != 0) {
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
