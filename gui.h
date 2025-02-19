#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "consts.h"
#include "tokens.h"
#include "utils.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// NOTE: neg to make scrolling up move page up and vice versa
#define HORIZONTAL_SCROLL_MULT (-20)
#define VERTICAL_SCROLL_MULT 20

#define GUI_FONT "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
#define DEFAULT_FONT_SIZE 20

#define HORIZONTAL_PADDING 10
#define VERTICAL_PADDING 10

#define FRAME_DELAY 33 // in milliseconds; ~30FPS

int FONT_SIZE = DEFAULT_FONT_SIZE;
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {255, 0, 0, 255};
const SDL_Color GREEN = {0, 255, 0, 255};
const SDL_Color BLUE = {0, 0, 255, 255};
const SDL_Color YELLOW = {255, 200, 0, 255};
const SDL_Color MAGENTA = {255, 0, 255, 255};
const SDL_Color GREY = {128, 128, 128, 255};
const SDL_Color MOUSE_HIGHLIGHT = {190, 240, 255, 128};

typedef struct {
  struct SDL_Texture *texture;
  bool is_newline;
  int w;
  int h;
} Texture;

typedef struct {
  int vertical_offset;
  int horizontal_offset;
} Scroll;

// allocs memory
Texture **tokens_to_textures(SDL_Renderer *renderer, TTF_Font *font,
                             int font_size, Token **tokens, int tokens_count,
                             int *textures_count) {
  Texture **textures = calloc(tokens_count, sizeof(Texture *));

  SDL_Color text_color = BLACK;

  for (int i = 0; i < tokens_count; i += 1) {

    SDL_Surface *text_surface =
        TTF_RenderUTF8_Solid(font, tokens[i]->v, text_color);
    if (text_surface == NULL) {
      fprintf(stderr, "failed to create text surface: %s\n", TTF_GetError());
      return NULL;
    }

    SDL_Texture *text_texture =
        SDL_CreateTextureFromSurface(renderer, text_surface);

    if (text_texture == NULL) {
      fprintf(stderr, "failed to create text texture: %s\n", SDL_GetError());
      return NULL;
    }

    Texture *tp = calloc(1, sizeof(Texture));
    tp->texture = text_texture;
    tp->is_newline = tokens[i]->t == TOKEN_NEWLINE;
    tp->w = text_surface->w;
    tp->h = text_surface->h;

    textures[*textures_count] = tp;
    *textures_count += 1;

    SDL_FreeSurface(text_surface);
  }
  return textures;
}

// frees memory
void free_textures(Texture **textures, int textures_count) {
  for (int i = 0; i < textures_count; i++) {
    if (textures[i] != NULL) {
      SDL_DestroyTexture(textures[i]->texture);
      free(textures[i]);
    }
  }
}

// update_textures frees existing textures
// and creates new textures from tokens
// frees and allocs memory
Texture **update_textures(Texture **textures, SDL_Renderer *renderer,
                          TTF_Font *font, int font_size, Token **tokens,
                          int tokens_count, int *textures_count) {
  free_textures(textures, *textures_count);
  *textures_count = 0;
  return tokens_to_textures(renderer, font, FONT_SIZE, tokens, tokens_count,
                            textures_count);
}

int cpy_to_renderer(SDL_Renderer *renderer, Texture **textures,
                    int textures_count, Scroll *scroll) {

  int local_horizontal_offset = 0;
  int local_vertical_offset = 0;

  for (int i = 0; i < textures_count; i += 1) {

    if (textures[i]->is_newline) {
      local_horizontal_offset = 0;
      local_vertical_offset += textures[i]->h;
    } else {
      SDL_Rect text_rect = {HORIZONTAL_PADDING + local_horizontal_offset +
                                scroll->horizontal_offset,
                            VERTICAL_PADDING + local_vertical_offset +
                                scroll->vertical_offset,
                            textures[i]->w, textures[i]->h};
      SDL_RenderCopy(renderer, textures[i]->texture, NULL, &text_rect);
      local_horizontal_offset += textures[i]->w;
    }
  }
  return EXIT_SUCCESS;
}

// TODO: handle the state better than using global vars
bool ctrl_pressed = false;
bool shift_pressed = false;

int handle_sdl_events(SDL_Event sdl_event, SDL_Renderer *renderer,
                      TTF_Font *font, Texture **text_textures,
                      int textures_count, Scroll *scroll,
                      bool *keep_window_open, bool *needs_refreshing) {

  int event_count = 0;
  int err = 0;

  while (SDL_PollEvent(&sdl_event) > 0) {
    event_count += 1;

    SDL_RenderClear(renderer);

    if (sdl_event.type == SDL_QUIT || sdl_event.type == SDL_KEYDOWN &&
                                          sdl_event.key.state == SDL_PRESSED &&
                                          sdl_event.key.keysym.sym == SDLK_q) {
      *keep_window_open = false;
      return event_count;

    } else if (sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_LCTRL ||
                sdl_event.key.keysym.sym == SDLK_RCTRL)) {
      ctrl_pressed = true;
    } else if (sdl_event.type == SDL_KEYUP &&
               sdl_event.key.state == SDL_RELEASED &&
               (sdl_event.key.keysym.sym == SDLK_LCTRL ||
                sdl_event.key.keysym.sym == SDLK_RCTRL)) {
      ctrl_pressed = false;

    } else if (sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_LSHIFT ||
                sdl_event.key.keysym.sym == SDLK_RSHIFT)) {
      shift_pressed = true;
    } else if (sdl_event.type == SDL_KEYUP &&
               sdl_event.key.state == SDL_RELEASED &&
               (sdl_event.key.keysym.sym == SDLK_LSHIFT ||
                sdl_event.key.keysym.sym == SDLK_RSHIFT)) {
      shift_pressed = false;

    } else if (!ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.y != 0) {
      scroll->vertical_offset += VERTICAL_SCROLL_MULT * sdl_event.wheel.y;
    } else if (!ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.x != 0) {
      scroll->horizontal_offset += HORIZONTAL_SCROLL_MULT * sdl_event.wheel.x;

    } else if (ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.y != 0) {
      FONT_SIZE += 5 * sign(sdl_event.wheel.y);
      FONT_SIZE = (5 <= FONT_SIZE && FONT_SIZE <= 64) * FONT_SIZE +
                  (FONT_SIZE < 5) * 5 + (64 < FONT_SIZE) * 64;
      TTF_SetFontSize(font, FONT_SIZE);
      *needs_refreshing = true;
    } else if (ctrl_pressed && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_EQUALS ||
                sdl_event.key.keysym.sym == SDLK_MINUS)) {
      FONT_SIZE += 5 * (sdl_event.key.keysym.sym == SDLK_EQUALS) -
                   5 * (sdl_event.key.keysym.sym == SDLK_MINUS);
      FONT_SIZE = (5 <= FONT_SIZE && FONT_SIZE <= 64) * FONT_SIZE +
                  (FONT_SIZE < 5) * 5 + (64 < FONT_SIZE) * 64;
      TTF_SetFontSize(font, FONT_SIZE);
      *needs_refreshing = true;
    }

    SDL_RenderClear(renderer);
    err = cpy_to_renderer(renderer, text_textures, textures_count, scroll);
    if (err != EXIT_SUCCESS) {
      *keep_window_open = false;
      return event_count;
    }
    SDL_RenderPresent(renderer);
  }
  return event_count;
}

int gui_loop(char *filename, TokenizerConfig *tokenizer_config) {

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "SDL_Init failed: '%s'\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  if (TTF_Init() < 0) {
    fprintf(stderr, "TTF_Init failed: '%s'\n", TTF_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Window *window = SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED,
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
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

  int contents_len = 0;
  char *contents = read_contents(filename, &contents_len);
  time_t last_modified = get_last_modified(filename);

  int tokens_count = 0;
  Token **tokens =
      tokenize(contents, contents_len, tokenizer_config, &tokens_count);

  TTF_Font *font = TTF_OpenFont(GUI_FONT, FONT_SIZE);
  if (font == NULL) {
    fprintf(stderr, "failed to load font: %s\n", TTF_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }

  int textures_count = 0;
  Texture **text_textures = tokens_to_textures(
      renderer, font, FONT_SIZE, tokens, tokens_count, &textures_count);

  SDL_RenderClear(renderer);

  Scroll *scroll = calloc(1, sizeof(Scroll));
  int err = EXIT_SUCCESS;
  err = cpy_to_renderer(renderer, text_textures, textures_count, scroll);
  if (err != EXIT_SUCCESS) {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return err;
  }

  SDL_RenderPresent(renderer);

  bool keep_window_open = true;
  bool was_refreshed = false;
  bool needs_refreshing = false;

  Uint32 start = SDL_GetTicks();
  Uint32 end = SDL_GetTicks();
  float elapsed = 0;

  int handled_event_count = 0;

  while (keep_window_open) {

    start = SDL_GetTicks();

    SDL_Event sdl_event;
    handled_event_count = handle_sdl_events(
        sdl_event, renderer, font, text_textures, textures_count, scroll,
        &keep_window_open, &needs_refreshing);
    if (!keep_window_open) {
      break;
    }

    contents = check_contents(filename, contents, &contents_len, &last_modified,
                              &was_refreshed);
    if (was_refreshed) {
      was_refreshed = false;

      tokens = update_tokens(tokens, contents, contents_len, tokenizer_config,
                             &tokens_count);

      text_textures = update_textures(text_textures, renderer, font, FONT_SIZE,
                                      tokens, tokens_count, &textures_count);

      SDL_RenderClear(renderer);
      err = cpy_to_renderer(renderer, text_textures, textures_count, scroll);
      if (err != EXIT_SUCCESS) {
        break;
      }
    } else if (needs_refreshing) {
      needs_refreshing = false;

      text_textures = update_textures(text_textures, renderer, font, FONT_SIZE,
                                      tokens, tokens_count, &textures_count);

      SDL_RenderClear(renderer);
      err = cpy_to_renderer(renderer, text_textures, textures_count, scroll);
      if (err != EXIT_SUCCESS) {
        break;
      }
    }

    end = SDL_GetTicks();
    elapsed = end - start;
    if (elapsed <= FRAME_DELAY) {
      SDL_Delay(FRAME_DELAY - elapsed);
    }
    SDL_RenderPresent(renderer); // NOTE: always present current renderer
  }

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();

  free_textures(text_textures, textures_count);
  free_tokens(tokens, tokens_count);
  free_contents(contents);
  if (scroll != NULL) {
    free(scroll);
  }

  return err;
}