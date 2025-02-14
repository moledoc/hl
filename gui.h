#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tokens.h"

// TODO: horizontal/vertical scrollbars
// TODO: bound horizontal/vertical scrolling
// TODO: soft-wrap
// TODO: highlighting with mouse
// TODO: refactor once have working solution
// TODO: when and what should be _Quit?
// TODO: fix mem leaks
// TODO: check performance on diff machines; eg laptop w/ debian is fine, but
// win+wsl2 is slow when zooming; MAYBE: theres a better way to put text on
// screen in SDL2

/*
TODO: notes for documentation
`sdl2-config --cflags --libs`
clang <file> -I/usr/include/SDL2 -D_REENTRANT -lSDL2 -lSDL2_ttf && ./a.out
libsdl2-2.0-0 libsdl2-dev libsdl2-ttf-2.0-0 libsdl2-ttf-dev

Used these as templates/starting point:
- https://dev.to/deusinmachina/sdl-tutorial-in-c-part-2-displaying-text-o55
- https://dev.to/noah11012/using-sdl2-opening-a-window-79c

Additional materials:
- http://thenumb.at/cpp-course/sdl2/07/07.html
- http://thenumb.at/cpp-course/sdl2/08/08.html

*/

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define HORIZONTAL_SCROLL_MULT                                                 \
  (-10) // NOTE: neg to make scrolling up move page up and vice versa
#define VERTICAL_SCROLL_MULT 20

#define GUI_FONT "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
#define DEFAULT_FONT_SIZE 20

#define HORIZONTAL_PADDING 10
#define VERTICAL_PADDING 10

#define FRAME_DELAY 33 // in milliseconds

typedef struct {
  struct SDL_Texture *texture;
  bool is_newline;
  int w;
  int h;
} TexturePlus;

int sign(int a) { return -1 * (a < 0) + 1 * (a > 0); }

// allocs memory
TexturePlus **tokens_to_textures(SDL_Renderer *renderer, TTF_Font *font,
                                 Token **tokens, int tokens_count,
                                 int font_size, int *textures_count) {
  TexturePlus **textures = calloc(tokens_count, sizeof(TexturePlus *));

  int local_horizontal_offset = 0;
  int local_vertical_offset = 0;

  SDL_Color black = {0, 0, 0, 255};
  SDL_Color red = {255, 0, 0, 255};
  SDL_Color green = {0, 255, 0, 255};
  SDL_Color blue = {0, 0, 255, 255};
  SDL_Color yellow = {255, 200, 0, 255};
  SDL_Color magenta = {255, 0, 255, 255};
  SDL_Color grey = {128, 128, 128};

  for (int i = 0; i < tokens_count; i += 1) {
    SDL_Color textColor;
    if (tokens[i]->t == TOKEN_STRING) {
      textColor = green;
    } else if (tokens[i]->t == TOKEN_NUMBER) {
      textColor = magenta;
    } else if (tokens[i]->t == TOKEN_KEYWORD) {
      textColor = blue;
    } else if (tokens[i]->t == TOKEN_COMMENT_KEYWORD) {
      textColor = yellow;
      // FIXME: on block comments newline chars are not properly handled
    } else if (tokens[i]->t == TOKEN_COMMENT) {
      textColor = grey;
    } else {
      textColor = black;
    }

    SDL_Surface *textSurface =
        TTF_RenderText_Solid(font, tokens[i]->v, textColor);
    if (textSurface == NULL) {
      fprintf(stderr, "failed to create text surface: %s\n", TTF_GetError());
      return NULL;
    }

    SDL_Texture *textTexture =
        SDL_CreateTextureFromSurface(renderer, textSurface);

    if (textTexture == NULL) {
      fprintf(stderr, "failed to create text texture: %s\n", SDL_GetError());
      return NULL;
    }

    TexturePlus *tp = calloc(1, sizeof(TexturePlus));
    tp->texture = textTexture;
    tp->is_newline = tokens[i]->t == TOKEN_NEWLINE;
    tp->w = textSurface->w;
    tp->h = textSurface->h;

    textures[*textures_count] = tp;
    *textures_count += 1;
    SDL_FreeSurface(textSurface);
  }
  return textures;
}

// frees memory
void free_textures(TexturePlus **textures, int textures_count) {
  for (int i = 0; i < textures_count; i++) {
    if (textures[i] != NULL) {
      free(textures[i]);
    }
  }
}

int gui_print(SDL_Renderer *renderer, TexturePlus **textures,
              int textures_count, int vertical_offset, int horizontal_offset) {

  int local_horizontal_offset = 0;
  int local_vertical_offset = 0;
  for (int i = 0; i < textures_count; i += 1) {

    if (textures[i]->is_newline) {
      local_horizontal_offset = 0;
      local_vertical_offset += textures[i]->h;
    } else {
      SDL_Rect textRect = {
          HORIZONTAL_PADDING + local_horizontal_offset + horizontal_offset,
          VERTICAL_PADDING + local_vertical_offset + vertical_offset,
          textures[i]->w, textures[i]->h};
      SDL_RenderCopy(renderer, textures[i]->texture, NULL, &textRect);

      local_horizontal_offset += textures[i]->w;
    }
  }

  return EXIT_SUCCESS;
}

int gui_loop(const char *prog_name, char *filename, const char **keywords,
             const int keyword_count, bool comment_kw, Comment *line_comment,
             Comment *block_comment) {

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "SDL_Init failed: '%s'\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  if (TTF_Init() < 0) {
    fprintf(stderr, "TTF_Init failed: '%s'\n", TTF_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Window *window = SDL_CreateWindow(prog_name, SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
                                        SCREEN_HEIGHT, SDL_WINDOW_RESIZABLE);

  if (window == NULL) {
    fprintf(stderr, "failed to create window: '%s'\n", SDL_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

  if (renderer == NULL) {
    fprintf(stderr, "failed to create renderer: '%s'\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }

  int font_size = DEFAULT_FONT_SIZE;
  int vertical_offset = 0;
  int horizontal_offset = 0;
  int err = EXIT_SUCCESS;

  char *contents = read_contents(filename);
  time_t last_modified = get_last_modified(filename);
  int tokens_count = 0;
  Token **tokens =
      tokenize(contents, strlen(contents), keywords, keyword_count, comment_kw,
               &tokens_count, line_comment, block_comment);

  // print_buffer = calloc(tokens_count, sizeof(char *)); // REMOVEME:
  // print_tokens(tokens, tokens_count);                  // REMOVEME:

  TTF_Font *font = TTF_OpenFont(GUI_FONT, font_size);
  if (font == NULL) {
    fprintf(stderr, "failed to load font: %s\n", TTF_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  int textures_count = 0;
  TexturePlus **text_textures = tokens_to_textures(
      renderer, font, tokens, tokens_count, font_size, &textures_count);

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

  SDL_RenderClear(renderer);

  err = gui_print(renderer, text_textures, textures_count, vertical_offset,
                  horizontal_offset);
  if (err != EXIT_SUCCESS) {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return err;
  }

  SDL_RenderPresent(renderer);

  bool keep_window_open = true;
  bool ctrl_is_pressed = false;
  bool needs_refreshing = false;

  // REMOVEME: when zooming with mouse is removed
  // int zoom_counter = 0;

  while (keep_window_open) {

    Uint32 start = SDL_GetTicks();

    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event) > 0) {

      SDL_RenderClear(renderer);

      if (sdl_event.type == SDL_QUIT ||
          sdl_event.type == SDL_KEYDOWN && sdl_event.key.state == SDL_PRESSED &&
              sdl_event.key.keysym.sym == SDLK_q) {
        keep_window_open = false;
      } else if (sdl_event.type == SDL_KEYDOWN &&
                 sdl_event.key.state == SDL_PRESSED &&
                 (sdl_event.key.keysym.sym == SDLK_LCTRL ||
                  sdl_event.key.keysym.sym == SDLK_RCTRL ||
                  sdl_event.key.keysym.sym ==
                      KMOD_CTRL)) { // TODO: doesnt work properly on trackpad
        ctrl_is_pressed = true;
      } else if (sdl_event.type == SDL_KEYUP &&
                 sdl_event.key.state == SDL_RELEASED &&
                 (sdl_event.key.keysym.sym == SDLK_LCTRL ||
                  sdl_event.key.keysym.sym == SDLK_RCTRL)) {
        ctrl_is_pressed = false;

        /*
          // NOTE: zooming with mouse wheel introduced a performance issue Im
          not ready to deal with } else if (ctrl_is_pressed && sdl_event.type
          == SDL_MOUSEWHEEL && sdl_event.wheel.y != 0) { font_size += 5 *
          sign(sdl_event.wheel.y);
                // TODO: improve boundaries
                if (font_size <= 5) {
                  font_size = 5;
                } else if (font_size >= 64) {
                  font_size = 64;
                }
                TTF_SetFontSize(font, font_size);
                needs_refreshing = true;
                zoom_counter += 1;
                if (zoom_counter > 1) {
                  zoom_counter = 0;
                  continue;
                }
        */

      } else if (ctrl_is_pressed && sdl_event.type == SDL_KEYDOWN &&
                 sdl_event.key.state == SDL_PRESSED &&
                 sdl_event.key.keysym.sym == SDLK_EQUALS) {
        font_size += 5;
        // TODO: improve boundaries
        // REFACTORME:
        if (font_size <= 5) {
          font_size = 5;
        } else if (font_size >= 64) {
          font_size = 64;
        }
        TTF_SetFontSize(font, font_size);
        needs_refreshing = true;
      } else if (ctrl_is_pressed && sdl_event.type == SDL_KEYDOWN &&
                 sdl_event.key.state == SDL_PRESSED &&
                 sdl_event.key.keysym.sym == SDLK_MINUS) {
        font_size -= 5;
        // TODO: improve boundaries
        // REFACTORME:
        if (font_size <= 5) {
          font_size = 5;
        } else if (font_size >= 64) {
          font_size = 64;
        }
        TTF_SetFontSize(font, font_size);
        needs_refreshing = true;

      } else if (!ctrl_is_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
                 sdl_event.wheel.y != 0) {
        vertical_offset += VERTICAL_SCROLL_MULT * sdl_event.wheel.y;
      } else if (!ctrl_is_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
                 sdl_event.wheel.x != 0) {
        horizontal_offset += HORIZONTAL_SCROLL_MULT * sdl_event.wheel.x;
      }

      bool was_refreshed = false;
      contents =
          check_contents(filename, contents, &last_modified, &was_refreshed);
      if (was_refreshed) {
        tokens_count = 0;
        tokens =
            tokenize(contents, strlen(contents), keywords, keyword_count,
                     comment_kw, &tokens_count, line_comment, block_comment);
        // free_textures(text_textures, textures_count); // TODO: handle free
        // properly
        textures_count = 0;
        text_textures = tokens_to_textures(renderer, font, tokens, tokens_count,
                                           font_size, &textures_count);
      } else if (needs_refreshing) {
        needs_refreshing = false;
        textures_count = 0;
        text_textures = tokens_to_textures(renderer, font, tokens, tokens_count,
                                           font_size, &textures_count);
      }

      err = gui_print(renderer, text_textures, textures_count, vertical_offset,
                      horizontal_offset);
      if (err != EXIT_SUCCESS) {
        // free_textures(text_textures, textures_count); // TODO: handle free
        // properly
        break;
      }

      SDL_RenderPresent(renderer);
      SDL_UpdateWindowSurface(window);
    }

    Uint32 end = SDL_GetTicks();
    float elapsed = end - start;
    if (elapsed > FRAME_DELAY) {
      continue;
    }

    SDL_Delay(FRAME_DELAY - elapsed); // ~30FPS
  }

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();

  return err;
}