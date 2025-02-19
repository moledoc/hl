#pragma once

#include <stdbool.h>

#include "comments.h"
#include "keywords.h"

enum TOKEN_TYPE {
  TOKEN_WORD = 0,
  TOKEN_STRING,
  TOKEN_NUMBER,
  TOKEN_CODE_KEYWORD,
  TOKEN_COMMENT_KEYWORD,
  TOKEN_COMMENT,
  TOKEN_NEWLINE,
  TOKEN_SPACES,
  TOKEN_TABS,
  TOKEN_TOKEN_COUNT
};

static const char *TOKEN_NAMES[] = {
    "TOKEN_WORD",         "TOKEN_STRING",          "TOKEN_NUMBER",
    "TOKEN_CODE_KEYWORD", "TOKEN_COMMENT_KEYWORD", "TOKEN_COMMENT",
    "TOKEN_NEWLINE",      "TOKEN_SPACES",          "TOKEN_TABS"};

int TAB_WIDTH = 4;

typedef struct {
  enum TOKEN_TYPE t;
  char *v;
  int vlen;
} Token;

typedef struct {
  const char **code_keywords;
  int code_keywords_count;
  const Comment *line_comment;
  const Comment *block_comment;
  const bool color_comment_keywords;
} TokenizerConfig;

TokenizerConfig DEFAULT_TOKENIZER_CONFIG = {
    .code_keywords = (const char **)default_keywords,
    .code_keywords_count = DEFAULT_KEYWORDS_COUNT,
    .line_comment = (const Comment *)NULL,
    .block_comment = (const Comment *)NULL,
};

void free_tokenizer_config(TokenizerConfig *tokenizer_config) {
  if (tokenizer_config == NULL) {
    return;
  }
  if (tokenizer_config->line_comment != NULL) {
    free_comment((Comment *)tokenizer_config->line_comment);
  }
  if (tokenizer_config->block_comment != NULL) {
    free_comment((Comment *)tokenizer_config->block_comment);
  }
  free(tokenizer_config);
}

// NOTE: only handles decimals that use '.' as the separator
bool _is_number(const char *s) {
  char *s_cpy = (char *)s;
  int s_len = 0;
  int dot_count = 0;
  int minus_count = 0;
  char c;
  while ((c = *(s_cpy++)) != '\0') {
    if (c == '-' && s_len != 0) {
      return false;
    }

    s_len += 1;
    if (c == '.') {
      dot_count += 1;
    } else if (c == '-') {
      minus_count += 1;
    } else if (c < '0' || '9' < c) {
      return false;
    }
    if (dot_count > 1) {
      return false;
    }
  }
  if (s_len == 1 && minus_count == 1) {
    return false;
  }
  return true;
}

bool _strcmp_window(const char *s1, const char *s2, int window_size) {
  char *s1_cpy = (char *)s1;
  char *s2_cpy = (char *)s2;

  for (int i = 0; i < window_size; i += 1) {
    if (s1_cpy[i] == '\0' || s2_cpy[i] == '\0' || s1_cpy[i] != s2_cpy[i]) {
      return false;
    }
  }
  return true;
}

// tokenize takes in content, its length and tokenizer configuration
// and produces tokens based on that, token count is returned through
// tokens_count variable. allocs memory
Token **tokenize(char *contents, int contents_length,
                 TokenizerConfig *tokenizer_config, int *tokens_count) {

  // TODO: make better decision about this
  Comment *line_comment = NULL;
  Comment *block_comment = NULL;
  char **code_keywords = (char **)default_keywords;
  int code_keywords_count = DEFAULT_KEYWORDS_COUNT;
  bool color_comment_keywords = false;
  if (tokenizer_config != NULL) {
    line_comment = (Comment *)tokenizer_config->line_comment;
    block_comment = (Comment *)tokenizer_config->block_comment;
    code_keywords = (char **)tokenizer_config->code_keywords;
    code_keywords_count = (int)tokenizer_config->code_keywords_count;
    color_comment_keywords = (bool)tokenizer_config->color_comment_keywords;
  }

  Token **tokens =
      calloc(contents_length + 1,
             sizeof(Token *)); // NOTE: +1 for possible extra newline

  int prev_offset = 0;
  int offset = 0;

  bool is_line_comment = false;
  bool is_block_comment = false;

  while (offset < contents_length) {

    Token *token = calloc(1, sizeof(Token));
    token->t = TOKEN_WORD;

    // { STRING
    if (!is_line_comment && !is_block_comment && contents[offset] == '"' ||
        contents[offset] == '\'') {
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
      offset += 1; // NOTE: account for closing quote
      // } STRING

      // { LINE_COMMENT
    } else if (line_comment != NULL &&
               offset + line_comment->begin_len < contents_length &&
               _strcmp_window((const char *)(contents + offset),
                              (const char *)line_comment->begin,
                              line_comment->begin_len) == 1) {
      is_line_comment = true;
      offset += line_comment->begin_len;
    } else if (line_comment != NULL &&
               offset + line_comment->end_len < contents_length &&
               _strcmp_window((const char *)(contents + offset),
                              (const char *)line_comment->end,
                              line_comment->end_len) == 1) {
      is_line_comment = false;
      offset += line_comment->end_len;
      // } LINE_COMMENT

      // { BLOCK_COMMENT
    } else if (block_comment != NULL &&
               offset + block_comment->begin_len < contents_length &&
               _strcmp_window((const char *)(contents + offset),
                              (const char *)block_comment->begin,
                              block_comment->begin_len) == 1) {
      is_block_comment = true;
      offset += block_comment->begin_len;
    } else if (block_comment != NULL &&
               offset + block_comment->end_len < contents_length &&
               _strcmp_window((const char *)(contents + offset),
                              (const char *)block_comment->end,
                              block_comment->end_len) == 1) {
      is_block_comment = false;
      offset += block_comment->end_len;
      token->t = TOKEN_COMMENT; // NOTE: here is fine to add, since block
                                // comment doesn't end with newline
      // } BLOCK_COMMENT

      // { SPACES
    } else if (offset < contents_length && contents[offset] == ' ') {
      char c;
      while (offset < contents_length && (c = contents[offset]) && c == ' ') {
        offset += 1;
      }
      token->t = TOKEN_SPACES;
      // } SPACES

      // { TABS
    } else if (offset < contents_length && contents[offset] == '\t') {
      char c;
      while (offset < contents_length && (c = contents[offset]) && c == '\t') {
        offset += 1;
      }
      token->t = TOKEN_TABS;
      // } TABS

      // { WORD
    } else {
      char c;

      // NOTE: single-quote is added so words with apostrophies are tokenized
      // correctly (in comment and otherwise).

      // NOTE: having double-quote here makes f-strings in python tricky, so
      // currently not included (might cause weird behavior in the future, but
      // currently tests pass).

      // MAYBE: not sure if this causes some issues, so keep an eye out and note
      // them down when encountered
      while (offset < contents_length && (c = contents[offset]) &&
             ('a' <= c && c <= 'z' || 'A' <= c && c <= 'Z' ||
              '0' <= c && c <= '9' || c == '+' || c == '-' || c == '.' ||
              c == '\'' || c == '_')) {
        offset += 1;
      }
      // } WORD
    }

    if (offset >= contents_length) {
      offset = contents_length;
    }

    if (prev_offset == offset) {
      offset += 1;
    }

    int vlen = offset - prev_offset;

    if (token->t == TOKEN_TABS) {
      token->v = calloc(TAB_WIDTH * vlen + 1, sizeof(char));
      token->vlen = TAB_WIDTH * vlen;
      memset(token->v, ' ', TAB_WIDTH * vlen);
    } else {
      token->v = calloc(vlen + 1, sizeof(char));
      token->vlen = vlen;
      memcpy(token->v, contents + prev_offset, vlen);
    }

    {
      if (_is_number((const char *)token->v)) {
        token->t = TOKEN_NUMBER;
      }
    }

    {
      if (token->t == TOKEN_WORD && code_keywords != NULL) {
        for (int i = 0; i < code_keywords_count; i += 1) {
          if (strcmp(token->v, code_keywords[i]) == 0) {
            token->t = TOKEN_CODE_KEYWORD;
            break;
          }
        }
      }
    }

    {
      if (color_comment_keywords && token->t == TOKEN_COMMENT ||
          token->t == TOKEN_WORD) {
        for (int i = 0; i < COMMENT_KEYWORDS_COUNT; i += 1) {
          if (strcmp(token->v, comment_keywords[i]) == 0) {
            token->t = TOKEN_COMMENT_KEYWORD;
            break;
          }
        }
      }
    }

    {
      if (token->t == TOKEN_WORD && token->vlen == 1 && *(token->v) == '\n') {
        token->t = TOKEN_NEWLINE;
      }
    }

    {
      if ((is_line_comment || is_block_comment) &&
          (token->t != TOKEN_COMMENT_KEYWORD && token->t != TOKEN_NEWLINE)) {
        token->t = TOKEN_COMMENT;
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
    eof_token->t = TOKEN_NEWLINE;
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

// update_tokens frees existing tokens
// and creates new tokens from content.
// frees and allocs memory
Token **update_tokens(Token **tokens, char *contents, int contents_length,
                      TokenizerConfig *tokenizer_config, int *tokens_count) {
  free_tokens(tokens, *tokens_count);
  *tokens_count = 0;
  return tokenize(contents, contents_length, tokenizer_config, tokens_count);
}

// {
#ifdef TESTING

#define PRINT_BUFFER_SIZE 1024
char *print_buffer =
    NULL; // NOTE: how to set: calloc(PRINT_BUFFER_SIZE, sizeof(char));

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
#endif // TESTING
       // }