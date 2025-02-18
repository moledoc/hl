#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

long file_size(char *filename) {
  FILE *fptr = fopen(filename, "r");
  fseek(fptr, 0, SEEK_END);
  long contents_length = ftell(fptr);
  fclose(fptr);
  return contents_length;
}

// allocs memory
char *read_contents(char *filename, int *content_len) {
  long contents_length = file_size(filename);
  FILE *fptr = fopen(filename, "r");
  char *contents = calloc(contents_length + 1, sizeof(char));
  size_t read_bytes = fread(contents, sizeof(char), contents_length, fptr);
  fclose(fptr);
  assert(read_bytes == contents_length);
  if (content_len != NULL) {
    *content_len = contents_length;
  }
  return contents;
}

// frees memory
void free_contents(char *contents) {
  if (contents != NULL) {
    free(contents);
  }
}

time_t get_last_modified(char *filename) {
  struct stat status;
  stat(filename, &status);
#ifdef OSX
  return status.st_mtimespec
      .tv_sec; // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html
#else
  return status.st_mtim.tv_sec;
#endif
}

bool _is_updated(char *filename, time_t *last_modified) {
  return last_modified != NULL && *last_modified < get_last_modified(filename);
}

// frees memory of contents
// allocs new memory for contents
char *update_contents(char *filename, char *contents, int *contents_len) {
  free_contents(contents);
  return read_contents(filename, contents_len);
}

char *check_contents(char *filename, char *contents, int *contents_len,
                     time_t *last_modified, bool *was_refreshed) {

  assert(last_modified != NULL);
  assert(was_refreshed != NULL);

  if (_is_updated(filename, last_modified)) {
    contents = update_contents(filename, contents, contents_len);
    *last_modified = get_last_modified(filename);
    *was_refreshed = true;
    return contents;
  }
  return contents;
}
