#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *begin;
  int begin_len;
  char *end;
  int end_len;
} Comment;

// allocs memory
Comment *_comment(char *begin, char *end) {
  int begin_len = strlen(begin);
  int end_len = strlen(end);

  Comment *comment = calloc(1, sizeof(Comment));
  comment->begin = calloc(begin_len + 1, sizeof(char));
  comment->end = calloc(end_len + 1, sizeof(char));
  memcpy(comment->begin, begin, begin_len);
  memcpy(comment->end, end, end_len);
  comment->begin_len = begin_len;
  comment->end_len = end_len;
  return comment;
}

// allocs memory
Comment *c_style_line_comment() {
  char *begin = "//";
  char *end = "\n";
  return _comment(begin, end);
}

// allocs memory
Comment *c_style_block_comment() {
  char *begin = "/*";
  char *end = "*/";
  return _comment(begin, end);
}

// allocs memory
Comment *py_style_line_comment() {
  char *begin = "#";
  char *end = "\n";
  return _comment(begin, end);
}

// allocs memory
Comment *html_style_block_comment() {
  char *begin = "<!--";
  char *end = "-->";
  return _comment(begin, end);
}

void free_comment(Comment *comment) {
  if (comment == NULL) {
    return;
  }
  if (comment->begin != NULL) {
    free(comment->begin);
  }
  if (comment->end != NULL) {
    free(comment->end);
  }
  free(comment);
}