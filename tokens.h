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
  int s_until;
} Token;

typedef struct {
  const char **code_keywords;
  int code_keywords_count;
  //
  const char **comment_keywords;
  int comment_keywords_count;
  //
  const Comment *line_comment;
  const Comment *block_comment;
  //
  bool color_code_keywords;
  bool color_comment_keywords;
  bool color_numbers;
  bool color_strings;
} TokenizerConfig;

TokenizerConfig DEFAULT_TOKENIZER_CONFIG = {
    .code_keywords = (const char **)default_keywords,
    .code_keywords_count = DEFAULT_KEYWORDS_COUNT,
    .comment_keywords = (const char **)default_keywords,
    .comment_keywords_count = DEFAULT_KEYWORDS_COUNT,
    .line_comment = (const Comment *)NULL,
    .block_comment = (const Comment *)NULL,
    .color_code_keywords = false,
    .color_comment_keywords = false,
    .color_numbers = false,
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

bool is_word(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
         ('0' <= c && c <= '9') || c == '_';
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

void handle_keyword(Token *token, const char **keywords, int keywords_count,
                    enum TOKEN_TYPE token_type) {
  if (token == NULL || keywords == NULL) {
    return;
  }
  for (int i = 0; i < keywords_count; i += 1) {
    if (strcmp(token->v, keywords[i]) == 0) {
      token->t = token_type;
      return;
    }
  }
}

void handle_string(Token **tokens, int *offset, int tokens_count) {
  char quote = *tokens[*offset]->v;
  tokens[*offset]->t = TOKEN_STRING;
  *offset += 1;
  while (*offset < tokens_count) {

    if (tokens[*offset]->t == TOKEN_WORD) {
      tokens[*offset]->t = TOKEN_STRING;
    }

    if (tokens[*offset]->vlen == 1 && *tokens[*offset]->v == quote &&
        ((*offset - 1 > -1 &&
          *tokens[*offset - 1]->v != '\\') || // NOTE: \\ before closing quote
         (*offset - 2 > -1 && *tokens[*offset - 1]->v == '\\' &&
          *tokens[*offset - 2]->v ==
              '\\'))) { // NOTE: no \ before closing quote
      break;
    }
    *offset += 1;
  }
}

void handle_number(Token **tokens, int *offset, int tokens_count) {
  tokens[*offset]->t = TOKEN_NUMBER;

  // NOTE: check if negative
  if (*offset - 1 > -1 && tokens[*offset - 1]->vlen == 1 &&
      *(tokens[*offset - 1]->v) == '-') {
    tokens[*offset - 1]->t = TOKEN_NUMBER;
  }

  // NOTE: check if decimal
  // NOTE: also include 'nr.'-repeating (eg ip-addresses) as valid numbers
  while (true) {
    if (*offset + 2 < tokens_count && tokens[*offset + 1]->vlen == 1 &&
        *(tokens[*offset + 1]->v) == '.' && is_nr(tokens[*offset + 2])) {
      tokens[*offset + 1]->t = TOKEN_NUMBER;
      tokens[*offset + 2]->t = TOKEN_NUMBER;
      *offset += 2;
    } else {
      break;
    }
  }
}

bool is_comment_start(Token **tokens, int offset, int tokens_count,
                      const Comment *comment) {

  if (tokens == NULL || tokens_count == 0 || comment == NULL) {
    return false;
  }
  if (offset + comment->begin_len >= tokens_count) {
    return false;
  }

  for (int i = 0; i < comment->begin_len; i += 1) {
    if (tokens[offset + i]->vlen != 1) {
      return false;
    }
    if (*(tokens[offset + i]->v) != comment->begin[i]) {
      return false;
    }
  }
  return true;
}

bool is_comment_end(Token **tokens, int offset, int tokens_count,
                    const Comment *comment) {

  if (tokens == NULL || tokens_count == 0 || comment == NULL) {
    return false;
  }
  if (offset + comment->end_len >= tokens_count) {
    return false;
  }

  for (int i = 0; i < comment->end_len; i += 1) {
    if (tokens[offset + i]->vlen != 1) {
      return false;
    }
    if (*(tokens[offset + i]->v) != comment->end[i]) {
      return false;
    }
  }
  return true;
}

void handle_comment(Token **tokens, int *offset, int tokens_count,
                    const Comment *comment, TokenizerConfig *tokenizer_config) {

  if (tokens == NULL || offset == NULL || tokens_count == 0 ||
      comment == NULL) {
    return;
  }

  // NOTE: mark comment begin tokens
  for (int i = 0; i < comment->begin_len; i += 1) {
    if (tokens[*offset]->t == TOKEN_WORD) {
      tokens[*offset]->t = TOKEN_COMMENT;
    }
    *offset += 1;
  }

  while (*offset < tokens_count &&
         !is_comment_end(tokens, *offset, tokens_count, comment)) {
    if (tokens[*offset]->t == TOKEN_WORD) {
      tokens[*offset]->t = TOKEN_COMMENT;

      // COMMENT_KEYWORD start
      if (tokenizer_config->color_comment_keywords) {
        handle_keyword(tokens[*offset], tokenizer_config->comment_keywords,
                       tokenizer_config->comment_keywords_count,
                       TOKEN_COMMENT_KEYWORD);
      }
      // COMMENT_KEYWORD end
    }
    *offset += 1;
  }

  // NOTE: file ended before we reached comment closing char
  // might happen when line comment is on the last line without newline
  if (*offset >= tokens_count) {
    return;
  }

  // NOTE: mark comment end tokens
  for (int i = 0; i < comment->end_len; i += 1) {
    if (tokens[*offset]->t == TOKEN_WORD) {
      tokens[*offset]->t = TOKEN_COMMENT;
    }
    *offset += 1;
  }
  *offset -= 1; // NOTE: account for loop iteration
}

int handle_scope_brackets(Token **tokens, int offset, int tokens_count,
                          bool is_string_bracket) {
  char c = *(tokens[offset]->v);
  char end_c;
  switch (c) {
  case '(':
    end_c = ')';
    break;
  case '[':
    end_c = ']';
    break;
  case '{':
    end_c = '}';
    break;
  case '<':
    end_c = '>';
    break;
  default:
    fprintf(stderr, "unreachable - bracket scope");
    return -1;
  }
  int open_count = 0;
  for (; 0 <= offset && offset < tokens_count; offset += 1) {
    if (!is_string_bracket && tokens[offset]->t == TOKEN_STRING) {
      continue;
    }
    if (tokens[offset]->vlen == 1 && *tokens[offset]->v == end_c &&
        open_count == 1) {
      return offset;
    }
    if (tokens[offset]->vlen == 1 && *tokens[offset]->v == c) {
      open_count += 1;
    }
    if (tokens[offset]->vlen == 1 && *tokens[offset]->v == end_c &&
        open_count > 1) {
      open_count -= 1;
    }
  }
  return -1;
}

// NOTE: unclosed brackets will highlight only current token
void handle_scope(Token **tokens, int *offset, int tokens_count) {
  // NOTE: set s_until as current token - will be overwritten if is actual scope
  // otherwise the scope is token itself
  if (tokens[*offset]->s_until >= 0) {
    return;
  }
  tokens[*offset]->s_until = *offset;

  char c = *tokens[*offset]->v;
  if (*offset < 0 ||
      !(c == '\'' || c == '"' || c == '`' || c == '(' || c == '[' || c == '{' ||
        c == '<') ||
      // REVIEWME: ignore single quote in comments, because english
      (tokens[*offset]->t == TOKEN_COMMENT && c == '\'')) {
    return;
  }

  int s_until = *offset;
  int local_offset = *offset;

  if (c == '\'' || c == '"' || c == '`') {
    local_offset += 1;
    while (local_offset < tokens_count) {

      if (tokens[local_offset]->vlen == 1 && *tokens[local_offset]->v == c &&
          ((local_offset - 1 > -1 &&
            *tokens[local_offset - 1]->v !=
                '\\') || // NOTE: \\ before closing quote
           (local_offset - 2 > -1 && *tokens[local_offset - 1]->v == '\\' &&
            *tokens[local_offset - 2]->v ==
                '\\'))) { // NOTE: no \ before closing quote
        break;
      }
      local_offset += 1;
    }

    // NOTE: scope string brackets separately
    char cc;
    for (int i = *offset + 1; i < local_offset; i += 1) {
      if (tokens[i]->s_until < 0) {
        tokens[i]->s_until = i;
      }
      if (tokens[i]->vlen > 1) {
        continue;
      }
      cc = *(tokens[i]->v);

      if (cc == '(' || cc == '[' || cc == '{' || cc == '<') {
        int s_until = i;

        // NOTE: tokens count as the nr of tokens until end of string
        // to keep the bracket search inside the string
        int local_i_offset =
            handle_scope_brackets(tokens, i, local_offset, true);

        if (0 < local_i_offset && local_i_offset < local_offset) {
          tokens[local_i_offset]->s_until = s_until;
          tokens[s_until]->s_until = local_i_offset;
        }
      }
    }

    *offset = local_offset;
  } else if (c == '(' || c == '[' || c == '{' || c == '<') {
    local_offset =
        handle_scope_brackets(tokens, local_offset, tokens_count, false);
  }

  if (0 < local_offset && local_offset < tokens_count) {
    tokens[local_offset]->s_until = s_until;
    tokens[s_until]->s_until = local_offset;
  }
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

  while (offset < contents_length) {

    Token *token = calloc(1, sizeof(Token));
    token->t = TOKEN_WORD;
    token->s_until = -1;

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

  // NOTE: other token type processing
  for (int offset = 0; offset < *tokens_count; offset += 1) {

    // CODE_KEYWORD start
    if (tokenizer_config->color_code_keywords &&
        tokens[offset]->t == TOKEN_WORD) {
      handle_keyword(tokens[offset], tokenizer_config->code_keywords,
                     tokenizer_config->code_keywords_count, TOKEN_CODE_KEYWORD);
    }
    // CODE_KEYWORD end

    // NOTE: if it's not TOKEN_WORD, then we already parsed it,
    // skip further processing
    if (tokens[offset]->t != TOKEN_WORD) {
      continue;

      // STRING start
    } else if (tokenizer_config->color_strings && tokens[offset]->vlen == 1 &&
               (*tokens[offset]->v == '\'' || *tokens[offset]->v == '"' ||
                *tokens[offset]->v == '`')) {
      handle_string(tokens, &offset, *tokens_count);
      // STRING end

      // NUMBER start
    } else if (tokenizer_config->color_numbers && is_nr(tokens[offset])) {
      handle_number(tokens, &offset, *tokens_count);
      // NUMBER end

      // LINE_COMMENT start
    } else if (is_comment_start(tokens, offset, *tokens_count,
                                tokenizer_config->line_comment)) {
      handle_comment(tokens, &offset, *tokens_count,
                     tokenizer_config->line_comment, tokenizer_config);
      // LINE_COMMENT end

      // BLOCK_COMMENT start
    } else if (is_comment_start(tokens, offset, *tokens_count,
                                tokenizer_config->block_comment)) {
      handle_comment(tokens, &offset, *tokens_count,
                     tokenizer_config->block_comment, tokenizer_config);
      // BLOCK_COMMENT end
    }
  }

  // NOTE: scope processing
  for (int offset = 0; offset < *tokens_count; offset += 1) {
    handle_scope(tokens, &offset, *tokens_count);
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