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
  bool color_comment_keywords;
  bool color_strings;
} TokenizerConfig;

TokenizerConfig DEFAULT_TOKENIZER_CONFIG = {
    .code_keywords = (const char **)default_keywords,
    .code_keywords_count = DEFAULT_KEYWORDS_COUNT,
    .line_comment = (const Comment *)NULL,
    .block_comment = (const Comment *)NULL,
    .color_comment_keywords = false,
    .color_strings = false};

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
bool _is_number(const char *s, int length) {
  char *s_cpy = (char *)s;
  int s_len = 0;
  int dot_count = 0;
  int minus_count = 0;
  char c;
  int i = 0;
  while ((c = *(s_cpy++)) != '\0' && i < length) {
    i += 1;
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
  if (s_len == 1 && (minus_count == 1 || dot_count == 1)) {
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

bool is_word_end(char c) {
  return c == '\n' || c == ' ' || c == '\t' || c == '.' || c == ',' ||
         c == ':' || c == ';' || c == '(' || c == ')' || c == '[' || c == ']' ||
         c == '{' || c == '}' || c == '<' || c == '>' || c == '\\' || c == '@';
}

void handle_string(Token *token, char *contents, int contents_length,
                   int *offset, char *is_string) {
  token->t = TOKEN_STRING;
  bool escaped = false;
  char c;
  while (*offset < contents_length && (c = contents[*offset]) &&
         (c == *is_string && escaped || c != *is_string)) {
    if (c == '\n' || is_word_end(c)) {
      return;
    }
    escaped = contents[*offset - 1] != '\\' && c == '\\';
    *offset += 1;
  }
  *offset += 1; // NOTE: account for closing quote
  if (c == *is_string) {
    *is_string = 0;
  }
}

void handle_line_comment(Token *token, char *contents, int contents_length,
                         int *offset, bool *is_line_comment) {
  token->t = TOKEN_COMMENT;
  char c;
  while (*offset < contents_length && (c = contents[*offset])) {
    if (c == '\n') {
      *is_line_comment = false;
      return;
    }
    if (is_word_end(c)) {
      break;
    }
    *offset += 1;
  }
}

void handle_block_comment(Token *token, char *contents, int contents_length,
                          int *offset, bool *is_block_comment,
                          Comment *block_comment) {
  token->t = TOKEN_COMMENT;
  char c;
  while (*offset < contents_length && (c = contents[*offset]) &&
         !is_word_end(c)) {
    if (*offset + block_comment->end_len < contents_length &&
        _strcmp_window((const char *)(contents + *offset),
                       (const char *)block_comment->end,
                       block_comment->end_len) == 1) {
      *is_block_comment = false;
      *offset += block_comment->end_len;
      return;
    }

    if (c == '\n' || is_word_end(c)) {
      return;
    }
    *offset += 1;
  }
}

bool is_word(char c) {
  return 'a' <= c && c <= 'z' || 'A' <= c && c <= 'Z' || '0' <= c && c <= '9' ||
         c == '_';
}

bool is_int(char c) { return '0' <= c && c <= '9'; }

bool is_nr(Token *token) {
  for (int i = 0; i < token->vlen; i += 1) {
    if (!is_int((token->v)[i])) {
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

  if (tokenizer_config == NULL) {
    tokenizer_config = &DEFAULT_TOKENIZER_CONFIG;
  }

  Token **tokens =
      calloc(contents_length + 1,
             sizeof(Token *)); // NOTE: +1 for possible extra newline

  int prev_offset = 0;
  int offset = 0;

  bool is_line_comment = false;
  bool is_block_comment = false;
  char is_string = 0;

  while (offset < contents_length) {

    Token *token = calloc(1, sizeof(Token));
    token->t = TOKEN_WORD;

    // NEWLINE start
    if (contents[offset] == '\n') {
      token->t = TOKEN_NEWLINE;
      offset += 1;
      // NEWLINE end

      // SPACES start
    } else if (offset < contents_length && contents[offset] == ' ') {
      token->t = TOKEN_SPACES;
      char c;
      while (offset < contents_length && (c = contents[offset]) && c == ' ') {
        offset += 1;
      }
      // SPACES end

      // TABS start
    } else if (offset < contents_length && contents[offset] == '\t') {
      token->t = TOKEN_TABS;
      char c;
      while (offset < contents_length && (c = contents[offset]) && c == '\t') {
        offset += 1;
      }
      // TABS end

      // WORD start
    } else {
      char c;
      while (offset < contents_length && (c = contents[offset]) && is_word(c)) {
        offset += 1;
      }
      // WORD end
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

    tokens[*tokens_count] = token;
    *tokens_count += 1;

    prev_offset = offset;
  }

  // second round
  for (int i = 0; i < *tokens_count; i += 1) {
    if (0) {

      // CODE_KEYWORD start
    } else if (tokens[i]->t == TOKEN_WORD &&
               tokenizer_config->code_keywords != NULL) {
      for (int j = 0; j < tokenizer_config->code_keywords_count; j += 1) {
        if (strcmp(tokens[i]->v, tokenizer_config->code_keywords[j]) == 0) {
          tokens[i]->t = TOKEN_CODE_KEYWORD;
          break;
        }
      }
      // CODE_KEYWORD end

      // STRING start
    } else if (tokens[i]->t == TOKEN_WORD && tokens[i]->vlen == 1 &&
               (*tokens[i]->v == '\'' || *tokens[i]->v == '"')) {
      char quote = *tokens[i]->v;
      tokens[i]->t = TOKEN_STRING;
      i += 1;
      while (i < *tokens_count) {

        if (tokens[i]->t == TOKEN_WORD) {
          tokens[i]->t = TOKEN_STRING;
        }

        if (tokens[i]->vlen == 1 && *tokens[i]->v == quote &&
            ((i - 1 > -1 &&
              *tokens[i - 1]->v != '\\') || // NOTE: \\ before closing quote
             (i - 2 > -1 && *tokens[i - 1]->v == '\\' &&
              *tokens[i - 2]->v == '\\'))) { // NOTE: no \ before closing quote
          break;
        }
        i += 1;
      }
      // STRING end

      // NUMBER start
    } else if (tokens[i]->t == TOKEN_WORD && is_nr(tokens[i])) {
      tokens[i]->t = TOKEN_NUMBER;

      // NOTE: check if negative
      if (i - 1 > -1 && tokens[i - 1]->vlen == 1 &&
          *(tokens[i - 1]->v) == '-') {
        tokens[i - 1]->t = TOKEN_NUMBER;
      }

      // NOTE: check if decimal
      // NOTE: also include 'nr.'-repeating (eg ip-addresses) as valid numbers
      while (true) {
        if (i + 2 < *tokens_count && tokens[i + 1]->vlen == 1 &&
            *(tokens[i + 1]->v) == '.' && is_nr(tokens[i + 2])) {
          tokens[i + 1]->t = TOKEN_NUMBER;
          tokens[i + 2]->t = TOKEN_NUMBER;
          i += 2;
        } else {
          break;
        }
      }
      // NUMBER end

      // LINE_COMMENT start
      // LINE_COMMENT end

      // BLOCK_COMMENT start
      // BLOCK_COMMENT end

      // END
    }
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

// tokenize takes in content, its length and tokenizer configuration
// and produces tokens based on that, token count is returned through
// tokens_count variable. allocs memory
Token **tokenize2(char *contents, int contents_length,
                  TokenizerConfig *tokenizer_config, int *tokens_count) {

  if (tokenizer_config == NULL) {
    tokenizer_config = &DEFAULT_TOKENIZER_CONFIG;
  }

  Token **tokens =
      calloc(contents_length + 1,
             sizeof(Token *)); // NOTE: +1 for possible extra newline

  int prev_offset = 0;
  int offset = 0;

  bool is_line_comment = false;
  bool is_block_comment = false;
  char is_string = 0;

  while (offset < contents_length) {

    Token *token = calloc(1, sizeof(Token));
    token->t = TOKEN_WORD;

    // NEWLINE start
    if (contents[offset] == '\n') {
      token->t = TOKEN_NEWLINE;
      offset += 1;
      // NEWLINE end

      /*
            // STRING start
          } else if (tokenizer_config->color_strings && is_string == 0 &&
                         !is_line_comment && !is_block_comment &&
                         contents[offset] == '"' ||
                     contents[offset] == '\'') {
            is_string = contents[offset];
            offset += 1;
            handle_string(token, contents, contents_length, &offset,
         &is_string); } else if (tokenizer_config->color_strings && is_string !=
         0) { handle_string(token, contents, contents_length, &offset,
         &is_string);
            // STRING end
      */

      // LINE_COMMENT start
    } else if (tokenizer_config->line_comment != NULL && !is_line_comment &&
               offset + tokenizer_config->line_comment->begin_len <
                   contents_length &&
               _strcmp_window(
                   (const char *)(contents + offset),
                   (const char *)tokenizer_config->line_comment->begin,
                   tokenizer_config->line_comment->begin_len) == 1) {
      is_line_comment = true;
      offset += tokenizer_config->line_comment->begin_len;
      handle_line_comment(token, contents, contents_length, &offset,
                          &is_line_comment);
    } else if (tokenizer_config->line_comment != NULL && is_line_comment) {
      handle_line_comment(token, contents, contents_length, &offset,
                          &is_line_comment);
      // LINE_COMMENT end

      // BLOCK_COMMENT start
    } else if (tokenizer_config->block_comment != NULL &&
               offset + tokenizer_config->block_comment->begin_len <
                   contents_length &&
               _strcmp_window(
                   (const char *)(contents + offset),
                   (const char *)tokenizer_config->block_comment->begin,
                   tokenizer_config->block_comment->begin_len) == 1) {
      is_block_comment = true;
      offset += tokenizer_config->block_comment->begin_len;
      handle_block_comment(token, contents, contents_length, &offset,
                           &is_block_comment,
                           (Comment *)tokenizer_config->block_comment);
    } else if (tokenizer_config->block_comment != NULL && is_block_comment) {
      handle_block_comment(token, contents, contents_length, &offset,
                           &is_block_comment,
                           (Comment *)tokenizer_config->block_comment);

      // BLOCK_COMMENT end

      // SPACES start
    } else if (offset < contents_length && contents[offset] == ' ') {
      token->t = TOKEN_SPACES;
      char c;
      while (offset < contents_length && (c = contents[offset]) && c == ' ') {
        offset += 1;
      }
      // SPACES end

      // TABS start
    } else if (offset < contents_length && contents[offset] == '\t') {
      token->t = TOKEN_TABS;
      char c;
      while (offset < contents_length && (c = contents[offset]) && c == '\t') {
        offset += 1;
      }
      // TABS end

      // NUMBER start
    } else if ('0' <= contents[offset] && contents[offset] <= '9' ||
               offset + 1 < contents_length && contents[offset] == '-' &&
                   '0' <= contents[offset + 1] && contents[offset + 1] <= '9') {
      offset += 1;
      char c;
      while (offset < contents_length && (c = contents[offset]) &&
             (!is_word_end(c) || c == '.')) {
        offset += 1;
      }

      if (_is_number(contents + prev_offset, offset - prev_offset)) {
        token->t = TOKEN_NUMBER;
      }

      // NUMBER end

      // WORD start
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
              '0' <= c && c <= '9' || c == '+' || c == '-' || c == '\'' ||
              c == '_')) {
        offset += 1;
      }
      // WORD end
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
      if (token->t == TOKEN_WORD && tokenizer_config->code_keywords != NULL) {
        for (int i = 0; i < tokenizer_config->code_keywords_count; i += 1) {
          if (strcmp(token->v, tokenizer_config->code_keywords[i]) == 0) {
            token->t = TOKEN_CODE_KEYWORD;
            break;
          }
        }
      }
    }

    {
      if (tokenizer_config->color_comment_keywords &&
          token->t == TOKEN_COMMENT) {
        for (int i = 0; i < COMMENT_KEYWORDS_COUNT; i += 1) {
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

// -----------------------------------

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