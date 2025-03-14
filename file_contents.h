#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils.h"

// TODO: other platforms
bool file_exists(char *filename) { return access(filename, F_OK) == 0; }

// allocs memory
char *file_ext(char *filename) {
  char *filename_dup = strdup(filename); // allocs memory
  const char *dot = strrchr(filename_dup, '.');
  int filename_len = strlen(filename);
  char *ext = calloc(filename_len + 1, sizeof(char));
  if (dot != NULL && dot != filename_dup) {
    memcpy(ext, dot + 1, min(filename_len, strlen(dot + 1)));
  }
  free(filename_dup); // free strdup
  return ext;
}

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
  if (read_bytes != contents_length) {
    fprintf(stdout, "[WARNING]: contents_length is %ld, but read %ld\n",
            contents_length, read_bytes);
  }
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

  if (last_modified == NULL || was_refreshed == NULL) {
    fprintf(stdout, "[WARNING]: 'last_modified' or 'was_refreshed' not set\n");
    return contents;
  }

  if (_is_updated(filename, last_modified)) {
    contents = update_contents(filename, contents, contents_len);
    *last_modified = get_last_modified(filename);
    *was_refreshed = true;
    return contents;
  }
  return contents;
}
