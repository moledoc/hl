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

enum COLOR_SCHEME_NAMES {
  COLOR_SCHEME_LIGHT,
  COLOR_SCHEME_DARK,
  COLOR_SCHEME_GRUVBOX_LIGHT,
  COLOR_SCHEME_GRUVBOX_DARK,
  COLOR_SCHEME_SOLARIZED_LIGHT,
  COLOR_SCHEME_SOLARIZED_DARK,
  COLOR_SCHEME_COUNT
};

#define rgb_to_sdl_color(rgb)                                                  \
  ((SDL_Color){rgb >> (8 * 2) & 0xFF, rgb >> (8 * 1) & 0xFF,                   \
               rgb >> (8 * 0) & 0xFF, 0xFF})

#define rgba_to_sdl_color(rgba)                                                \
  ((SDL_Color){                                                                \
      rgba >> (8 * 3) & 0xFF,                                                  \
      rgba >> (8 * 2) & 0xFF,                                                  \
      rgba >> (8 * 1) & 0xFF,                                                  \
      rgba >> (8 * 0) & 0xFF,                                                  \
  })

ColorScheme *color_scheme = NULL;
int color_scheme_idx = 0;

ColorScheme color_schemes[COLOR_SCHEME_COUNT] =
    (ColorScheme[COLOR_SCHEME_COUNT]){

        // LIGHT
        {
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
        },

        // DARK
        {
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
        },

        // GRUVBOX LIGHT
        {
            .bg = rgb_to_sdl_color(0xfbf1c7),
            .fg = rgb_to_sdl_color(0x3c3836),
            .mouse_highlight = rgb_to_sdl_color(0xebdbb2),
            .scrollbar_bg = rgb_to_sdl_color(0xd5c4a1),
            .scrollbar_fg = rgb_to_sdl_color(0x928374),
            .strings = rgb_to_sdl_color(0x689d6a),
            .numbers = rgb_to_sdl_color(0xb16286),
            .code_keywords = rgb_to_sdl_color(0x458588),
            .comment_keywords = rgb_to_sdl_color(0xd79921),
            .comments = rgb_to_sdl_color(0x928374),
        },

        // GRUVBOX DARK
        {
            .bg = rgb_to_sdl_color(0x282828),
            .fg = rgb_to_sdl_color(0xebdbb2),
            .mouse_highlight = rgb_to_sdl_color(0xa88984),
            .scrollbar_bg = rgb_to_sdl_color(0xbdae93),
            .scrollbar_fg = rgb_to_sdl_color(0x928374),
            .strings = rgb_to_sdl_color(0x689d6a),
            .numbers = rgb_to_sdl_color(0xb16286),
            .code_keywords = rgb_to_sdl_color(0x458588),
            .comment_keywords = rgb_to_sdl_color(0xd79921),
            .comments = rgb_to_sdl_color(0x928374),
        },

        // SOLARIZED LIGHT
        {
            .bg = rgb_to_sdl_color(0xfdf6e3),
            .fg = rgb_to_sdl_color(0x839496),
            .mouse_highlight = rgb_to_sdl_color(0xeee8d5),
            .scrollbar_bg = rgb_to_sdl_color(0x839496),
            .scrollbar_fg = rgb_to_sdl_color(0x586e75),
            .comments = rgb_to_sdl_color(0x93a1a1),
            .strings = rgb_to_sdl_color(0x859900),
            .numbers = rgb_to_sdl_color(0xd33682),
            .code_keywords = rgb_to_sdl_color(0x268bd2),
            .comment_keywords = rgb_to_sdl_color(0xb58900),
        },

        // SOLARIZED DARK
        {
            .bg = rgb_to_sdl_color(0x002b36),
            .fg = rgb_to_sdl_color(0x839496),
            .mouse_highlight = rgb_to_sdl_color(0x073642),
            .scrollbar_bg = rgb_to_sdl_color(0x93a1a1),
            .scrollbar_fg = rgb_to_sdl_color(0x657b83),
            .comments = rgb_to_sdl_color(0x586e75),
            .strings = rgb_to_sdl_color(0x859900),
            .numbers = rgb_to_sdl_color(0xd33682),
            .code_keywords = rgb_to_sdl_color(0x268bd2),
            .comment_keywords = rgb_to_sdl_color(0xb58900),
        },

    };
