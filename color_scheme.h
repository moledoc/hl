#pragma once

#include <SDL2/SDL.h>

#define WHITE ((SDL_Color){255, 255, 255, 255})
#define BLACK ((SDL_Color){0, 0, 0, 255})
#define RED ((SDL_Color){255, 0, 0, 255})
#define GREEN ((SDL_Color){0, 255, 0, 255})
#define BLUE ((SDL_Color){0, 0, 255, 255})
#define YELLOW ((SDL_Color){255, 200, 0, 255})
#define MAGENTA ((SDL_Color){255, 0, 255, 255})
#define GREY ((SDL_Color){128, 128, 128, 255})

typedef struct {
  SDL_Color bg;
  SDL_Color fg;
  SDL_Color mouse_highlight;
  SDL_Color scrollbar_bg;
  SDL_Color scrollbar_fg;
  SDL_Color strings;
  SDL_Color numbers;
  SDL_Color code_keywords;
  SDL_Color comment_keywords;
  SDL_Color comments;
} ColorScheme;

ColorScheme *default_light_color_scheme = &(ColorScheme){
    .bg = (SDL_Color)WHITE,
    .fg = (SDL_Color)BLACK,
    .mouse_highlight = (SDL_Color){200, 200, 200, 128},
    .scrollbar_bg = (SDL_Color){192, 192, 192, 64},
    .scrollbar_fg = (SDL_Color){128, 128, 128, 128},
    .strings = (SDL_Color){0, 186, 0, 255},
    .numbers = (SDL_Color)MAGENTA,
    .code_keywords = (SDL_Color)BLUE,
    .comment_keywords = (SDL_Color){255, 196, 0, 255},
    .comments = (SDL_Color)GREY,
};

ColorScheme *default_dark_color_scheme = &(ColorScheme){
    .bg = (SDL_Color)BLACK,
    .fg = (SDL_Color)WHITE,
    .mouse_highlight = (SDL_Color){100, 100, 100, 128},
    .scrollbar_bg = (SDL_Color){192, 192, 192, 64},
    .scrollbar_fg = (SDL_Color){128, 128, 128, 128},
    .strings = (SDL_Color){0, 186, 0, 255},
    .numbers = (SDL_Color)MAGENTA,
    .code_keywords = (SDL_Color){0, 169, 255, 255},
    .comment_keywords = (SDL_Color){255, 196, 0, 255},
    .comments = (SDL_Color)GREY,
};

ColorScheme *color_scheme = NULL;

ColorScheme *get_color_scheme() {
  if (color_scheme == NULL) {
    color_scheme = default_dark_color_scheme;
  }
  return color_scheme;
}