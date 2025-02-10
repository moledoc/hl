#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_contents.h"
#include "tokens.h"
#include "tui.h"

#define VERSION "0.0.1"

const char *prog_name = "syhl"; // NOTE: subject to change

// TODO: revise flag names, current ones are probably only placeholders
void help() {
  printf("NAME\n\t%s - code highlighter\n", prog_name);
  printf("\nSYNOPSIS\n\t%s [-h] [-v] [-e EXT] [-m MODE] [-kw] FILE\n",
         prog_name);
  printf("\nOPTIONS\n");
  printf("\t%s\n\t\tprint help\n", "-h, -help, --help, help");
  printf("\t%s\n\t\tprint version\n", "-v, -version, --version, version");
  printf("\t%s\n\t\toverrule keyword highlighting\n", "-e EXT, --ext EXT");
  printf("\t\tNOTE: supported extensions: c, cpp, TBD: go, python, etc\n");
  printf("\t%s\n\t\tspecify which UI mode is used\n", "-m MODE, --mode MODE");
  printf("\t\tNOTE: supported modes: tui, TODO: gui (default)\n");
  printf("\t%s\n\t\tfile to be highlighted (required)\n", "FILE");
  printf("\t\tNOTE: keyword highlighting is based on file extension, can be "
         "overruled by --ext flag\n");
  printf("\t%s\n\t\thighlight more common keywords found in comments\n", "-kw");
  printf("\t\tNOTE: supported common keywords: `TODO`, `NOTE`, `FIXME`, "
         "`REVIEWME`, `HACK`\n");
  printf("\nEXAMPLES\n");
  printf("\t* TODO:\n");
  printf("\nAUTHOR\n");
  printf("\tMeelis Utt (meelis.utt@gmail.com)\n");
}

int min(int a, int b) { return (a < b) * a + (b < a) * b; }

int main(int argc, char **argv) {

  char *filename = NULL;
  char ext[16] = {0};
  char *mode = "gui";
  bool comment_kw = false;
  const char **keywords = NULL;
  int keyword_count = 0;

  for (int i = 1; i < argc; ++i) {
    char *flag = argv[i];
    if (strcmp("-h", flag) == 0 || strcmp("help", flag) == 0 ||
        strcmp("-help", flag) == 0 || strcmp("--help", flag) == 0) {
      help();
      return 0;
    } else if (strcmp("-v", flag) == 0 || strcmp("version", flag) == 0 ||
               strcmp("-version", flag) == 0 ||
               strcmp("--version", flag) == 0) {
      printf("version: %s\n", VERSION);
      return 0;
    } else if ((strcmp("-e", flag) == 0 || strcmp("--ext", flag) == 0) &&
               i + 1 < argc) {
      memcpy(ext, argv[i + 1], min(sizeof(ext), strlen(argv[i + 1])));
      i += 1;
    } else if ((strcmp("-m", flag) == 0 || strcmp("--mode", flag) == 0) &&
               i + 1 < argc) {
      mode = argv[i + 1];
      i += 1;
    } else if (strcmp("-kw", flag) == 0) {
      comment_kw = true;
    } else if (i == argc - 1) {
      filename = flag;
    } else {
      fprintf(stderr, "unexpected flag '%s'\n", flag);
      return 1;
    }
  }

  if (filename == NULL) {
    fprintf(stderr, "missing required argument 'FILE'\n");
    return 1;
  }

  if (*ext == 0) {
    char *filename_dup = strdup(filename); // allocs memory
    const char *dot = strrchr(filename_dup, '.');
    if (dot != NULL && dot != filename_dup) {
      memcpy(ext, dot + 1, min(sizeof(ext), strlen(dot + 1)));
    }
    free(filename_dup); // free strdup
  }

  if (strcmp(ext, "cpp") == 0 || strcmp(ext, "c") == 0 ||
      strcmp(ext, "h") == 0) {
    keywords = (const char **)c_keywords;
    keyword_count = C_KEYWORD_COUNT;
  } else {
    keywords = (const char **)default_keywords;
    keyword_count = DEFAULT_KEYWORD_COUNT;
  }

  if (strcmp(mode, "gui") == 0) {
    printf("unimplemented");
    return 0;
  } else if (strcmp(mode, "tui") == 0) {
    tui_loop(filename, keywords, keyword_count, comment_kw);
  } else {
    fprintf(stderr, "unsupported mode '%s'\n", mode);
    return 1;
  }
}
