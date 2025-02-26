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
#define FONT_INCREMENT 2
#define FONT_LOWER_BOUND 12
#define FONT_UPPER_BOUND 64

#define HORIZONTAL_PADDING 10
#define VERTICAL_PADDING 10

#define FRAME_DELAY 33 // in milliseconds; ~30FPS

int FONT_SIZE = DEFAULT_FONT_SIZE; // MAYBE: move to state

// TODO: improve colors
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
  Token *token;
  int w;
  int h;
} Texture;

typedef struct {
  int horizontal_offset;
  int horizontal_lower_bound;
  int horizontal_upper_bound;
  //
  int vertical_offset;
  int vertical_lower_bound;
  int vertical_upper_bound;
  //
  int horizontal_text;
  int vertical_text;
  //
  int highlight_start_x;
  int highlight_start_y;
} Scroll;

typedef struct {
  bool ctrl_pressed;
  bool shift_pressed;
  bool keep_window_open;
  bool refresh_tokens;
  bool file_modified;
  bool left_mouse_button_pressed;
} State;

// allocs memory
Texture **tokens_to_textures(SDL_Renderer *renderer, TTF_Font *font,
                             int font_size, Token **tokens, int tokens_count,
                             int *textures_count) {
  Texture **textures = calloc(tokens_count, sizeof(Texture *));

  SDL_Color text_color = BLACK;

  for (int i = 0; i < tokens_count; i += 1) {

    if (tokens[i]->t == TOKEN_STRING) {
      text_color = GREEN;
    } else if (tokens[i]->t == TOKEN_NUMBER) {
      text_color = MAGENTA;
    } else if (tokens[i]->t == TOKEN_CODE_KEYWORD) {
      text_color = BLUE;
    } else if (tokens[i]->t == TOKEN_COMMENT_KEYWORD) {
      text_color = YELLOW;
    } else if (tokens[i]->t == TOKEN_COMMENT) {
      text_color = GREY;
    } else {
      text_color = BLACK;
    }

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
    tp->token = tokens[i];
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

// FIXME: if you go up from longer to shorter line from starting line
// then there is like a pixel where upper line is highlighted, but shouldn't be
void handle_mouse_highlight2(SDL_Window *window, SDL_Renderer *renderer,
                             Texture **textures, int textures_count,
                             Scroll *scroll, State *state) {
  if (!state->left_mouse_button_pressed) {
    return;
  }

  int local_horizontal_offset = 0;
  int local_vertical_offset = 0;

  int window_height = 0;
  int window_width = 0;
  (void)SDL_GetWindowSize(window, &window_width, &window_height);

  int mouse_x = 0;
  int mouse_y = 0;
  (void)SDL_GetMouseState(&mouse_x, &mouse_y);

  int highlight_start_x = scroll->highlight_start_x;
  int highlight_start_y = scroll->highlight_start_y;
  int highlight_end_x = mouse_x;
  int highlight_end_y = mouse_y;

  // mouse is higher than starting point
  if (mouse_y <= scroll->highlight_start_y) {
    highlight_end_y = scroll->highlight_start_y;
    highlight_start_y = mouse_y;
    highlight_end_x = scroll->highlight_start_x;
    highlight_start_x = mouse_x;
  }

  int abs_mouse_height_diff = abs(mouse_y - scroll->highlight_start_y);

  // starting and end is on the same line
  // to avoid glitches on top-right and bottow-left on the line,
  // assign starting with min and ending with max vals.
  // why: mentioned areas will otherwise be out-of-highlight zone technically
  if (abs_mouse_height_diff < FONT_SIZE) {
    highlight_start_x = min(scroll->highlight_start_x, mouse_x);
    highlight_end_x = max(scroll->highlight_start_x, mouse_x);
  }

  // mouse-highlighting
  for (int i = 0; i < textures_count; i += 1) {
    if (textures[i]->token->t == TOKEN_NEWLINE) {
      local_horizontal_offset = 0;
      local_vertical_offset += textures[i]->h;
      continue;
    }

    int texture_start_width = HORIZONTAL_PADDING + local_horizontal_offset +
                              scroll->horizontal_offset;
    int texture_start_height =
        VERTICAL_PADDING + local_vertical_offset + scroll->vertical_offset;

    // NOTE: only render what fits on screen
    if (!(texture_start_height <= window_height &&
          texture_start_width <= window_width)) {
      local_horizontal_offset += textures[i]->w;
      continue;
    }

    int texture_char_size = textures[i]->w / textures[i]->token->vlen;
    int highlight_start_offset = 0;
    int hightlight_end_offset = 0;

    bool highlight = false;

    // highlight rows
    if (highlight_start_y < texture_start_height + textures[i]->h &&
        texture_start_height < highlight_end_y) {
      highlight = true;
    }
    // don't highlight beginning row before mouse
    if (texture_start_height < highlight_start_y &&
        highlight_start_y < texture_start_height + textures[i]->h &&
        texture_start_width + textures[i]->w < highlight_start_x) {
      highlight = false;
    }
    // don't highlight ending row after mouse
    if (texture_start_height < highlight_end_y &&
        highlight_end_y < texture_start_height + textures[i]->h &&
        highlight_end_x < texture_start_width) {
      highlight = false;
    }
    // char-based highlighting on beginning token
    if (texture_start_height < highlight_start_y &&
        highlight_start_y < texture_start_height + textures[i]->h &&
        highlight_start_x < texture_start_width + textures[i]->w) {
      highlight_start_offset =
          ((highlight_start_x - texture_start_width) -
           (highlight_start_x - texture_start_width) % texture_char_size) %
          textures[i]->w;
    }
    // char-based highlighting on ending token
    if (texture_start_height < highlight_end_y &&
        highlight_end_y < texture_start_height + textures[i]->h &&
        texture_start_width < highlight_end_x) {
      hightlight_end_offset =
          ((texture_start_width + textures[i]->w - highlight_end_x) -
           (texture_start_width + textures[i]->w - highlight_end_x) %
               texture_char_size) %
          textures[i]->w;
    }

    if (highlight) {
      SDL_Rect highlight_rect = {
          texture_start_width + highlight_start_offset, texture_start_height,
          textures[i]->w - (highlight_start_offset + hightlight_end_offset),
          textures[i]->h};
      SDL_Color prev = {0};
      SDL_GetRenderDrawColor(renderer, (Uint8 *)&prev.r, (Uint8 *)&prev.g,
                             (Uint8 *)&prev.b, (Uint8 *)&prev.a);
      SDL_SetRenderDrawColor(renderer, MOUSE_HIGHLIGHT.r, MOUSE_HIGHLIGHT.g,
                             MOUSE_HIGHLIGHT.b, MOUSE_HIGHLIGHT.a);
      SDL_RenderFillRect(renderer, &highlight_rect);
      SDL_SetRenderDrawColor(renderer, prev.r, prev.g, prev.b, prev.a);
    }

    local_horizontal_offset += textures[i]->w;
  }
}

// TODO: FIXME: same-line is still bit glitchy
// TODO: comment the solution (why, not what)
// TODO: cleanup the solution
// TODOO: FIXME: jitteriness when highlighting upwards
void handle_mouse_highlight(SDL_Window *window, SDL_Renderer *renderer,
                            Texture **textures, int textures_count,
                            Scroll *scroll, State *state) {
  if (!state->left_mouse_button_pressed || textures_count == 0) {
    return;
  }

  int start_idx = -1;
  int end_idx = textures_count;

  int start_idx_offset_w = 0;
  int start_idx_offset_h = 0;

  int local_horizontal_offset = 0;
  int local_vertical_offset = 0;

  int window_height = 0;
  int window_width = 0;
  (void)SDL_GetWindowSize(window, &window_width, &window_height);

  int mouse_x = 0;
  int mouse_y = 0;
  (void)SDL_GetMouseState(&mouse_x, &mouse_y);

  int highlight_start_x = scroll->highlight_start_x;
  int highlight_start_y = scroll->highlight_start_y;
  int highlight_end_x = mouse_x;
  int highlight_end_y = mouse_y;

  int abs_mouse_height_diff = abs(mouse_y - scroll->highlight_start_y);

  // mouse is higher than starting point + 1 texture height
  // then switch starting point with current mouse position
  if (mouse_y <= scroll->highlight_start_y &&
      abs_mouse_height_diff >= textures[0]->h) {
    highlight_end_y = scroll->highlight_start_y;
    highlight_start_y = mouse_y;
    highlight_end_x = scroll->highlight_start_x;
    highlight_start_x = mouse_x;
  }

  // mouse is left of starting point and
  // vertical diff is less than 1 texture height,
  // then switch starting point with current mouse position
  if (mouse_x <= scroll->highlight_start_x &&
      abs_mouse_height_diff < textures[0]->h) {
    highlight_end_y = scroll->highlight_start_y;
    highlight_start_y = mouse_y;
    highlight_end_x = scroll->highlight_start_x;
    highlight_start_x = mouse_x;
  }

  // mouse-highlighting
  for (int i = 0; i < textures_count; i += 1) {

    int texture_start_width = HORIZONTAL_PADDING + local_horizontal_offset +
                              scroll->horizontal_offset;
    int texture_start_height =
        VERTICAL_PADDING + local_vertical_offset + scroll->vertical_offset;

    if (texture_start_height <= highlight_start_y &&
        highlight_start_y < texture_start_height + textures[i]->h &&
        texture_start_width < highlight_start_x &&
        highlight_start_x < texture_start_width + textures[i]->w) {
      start_idx = i;
      start_idx_offset_w = local_horizontal_offset;
      start_idx_offset_h = local_vertical_offset;
    }
    // when highlighting upwards handle newlines:
    // find highlighted area that's past the start point
    // and take previous texture
    // TODO: intermittently jittery
    if (start_idx < 0 &&
        (texture_start_height <= highlight_start_y &&
             highlight_start_y < texture_start_height + textures[i]->h &&
             highlight_start_x < texture_start_width ||
         highlight_start_y < texture_start_height)) {
      if (i > 0) {
        start_idx = i - 1;
      }
      if (local_horizontal_offset > 0) {
        start_idx_offset_w = local_horizontal_offset - textures[i - 1]->w;
      }
      if (local_vertical_offset > 0) {
        start_idx_offset_h = local_vertical_offset - textures[i - 1]->h;
      }
    }

    if (texture_start_height <= highlight_end_y &&
            highlight_end_y < texture_start_height + textures[i]->h &&
            highlight_end_x < texture_start_width ||
        highlight_end_y < texture_start_height) {

      end_idx = i;
      break;
    }

    if (textures[i]->token->t == TOKEN_NEWLINE) {
      local_horizontal_offset = 0;
      local_vertical_offset += textures[i]->h;
      continue;
    }

    // NOTE: only render what fits on screen
    if (!(texture_start_height <= window_height &&
          texture_start_width <= window_width)) {
      local_horizontal_offset += textures[i]->w;
      continue;
    }

    local_horizontal_offset += textures[i]->w;
  }

  // printf("HERE: %d %d\n", start_idx, end_idx);
  if (start_idx < 0) {
    start_idx = 0;
  }
  for (int i = start_idx; i < end_idx; i += 1) {

    int texture_start_width =
        HORIZONTAL_PADDING + start_idx_offset_w + scroll->horizontal_offset;
    int texture_start_height =
        VERTICAL_PADDING + start_idx_offset_h + scroll->vertical_offset;
    int texture_char_size = textures[i]->w / textures[i]->token->vlen;
    int highlight_start_offset = 0;
    int hightlight_end_offset = 0;

    if (i == start_idx) {
      highlight_start_offset =
          ((highlight_start_x - texture_start_width) -
           (highlight_start_x - texture_start_width) % texture_char_size) %
          textures[i]->w;
    }
    if (i + 1 == end_idx) {
      hightlight_end_offset =
          ((texture_start_width + textures[i]->w - highlight_end_x) -
           (texture_start_width + textures[i]->w - highlight_end_x) %
               texture_char_size) %
          textures[i]->w;
    }

    SDL_Rect highlight_rect = {
        texture_start_width + highlight_start_offset, texture_start_height,
        textures[i]->w - (highlight_start_offset + hightlight_end_offset),
        textures[i]->h};
    SDL_Color prev = {0};
    SDL_GetRenderDrawColor(renderer, (Uint8 *)&prev.r, (Uint8 *)&prev.g,
                           (Uint8 *)&prev.b, (Uint8 *)&prev.a);
    SDL_SetRenderDrawColor(renderer, MOUSE_HIGHLIGHT.r, MOUSE_HIGHLIGHT.g,
                           MOUSE_HIGHLIGHT.b, MOUSE_HIGHLIGHT.a);
    SDL_RenderFillRect(renderer, &highlight_rect);
    SDL_SetRenderDrawColor(renderer, prev.r, prev.g, prev.b, prev.a);

    if (textures[i]->token->t == TOKEN_NEWLINE) {
      start_idx_offset_w = 0;
      start_idx_offset_h += textures[i]->h;
      continue;
    }

    start_idx_offset_w += textures[i]->w;
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

int cpy_to_renderer(SDL_Window *window, SDL_Renderer *renderer,
                    Texture **textures, int textures_count, Scroll *scroll,
                    State *state) {

  if (state->left_mouse_button_pressed) {
    handle_mouse_highlight(window, renderer, textures, textures_count, scroll,
                           state);
  }

  int local_horizontal_offset = 0;
  int local_vertical_offset = 0;

  int max_horizontal_offset = 0;

  int window_height = 0;
  int window_width = 0;
  (void)SDL_GetWindowSize(window, &window_width, &window_height);

  for (int i = 0; i < textures_count; i += 1) {

    if (textures[i]->token->t == TOKEN_NEWLINE) {
      max_horizontal_offset =
          gt(max_horizontal_offset, local_horizontal_offset);
      local_horizontal_offset = 0;
      local_vertical_offset += textures[i]->h;
      continue;
    }
    int texture_start_width = HORIZONTAL_PADDING + local_horizontal_offset +
                              scroll->horizontal_offset;
    int texture_start_height =
        VERTICAL_PADDING + local_vertical_offset + scroll->vertical_offset;

    // NOTE: only render what fits on screen
    if (!(texture_start_height <= window_height &&
          texture_start_width <= window_width)) {
      local_horizontal_offset += textures[i]->w;
      continue;
    }

    SDL_Rect text_rect = {texture_start_width, texture_start_height,
                          textures[i]->w, textures[i]->h};
    SDL_RenderCopy(renderer, textures[i]->texture, NULL, &text_rect);

    local_horizontal_offset += textures[i]->w;
  }

  scroll->horizontal_text = max_horizontal_offset;
  scroll->vertical_text = local_vertical_offset;

  return EXIT_SUCCESS;
}

int handle_sdl_events(SDL_Window *window, SDL_Event sdl_event,
                      SDL_Renderer *renderer, TTF_Font *font,
                      Texture **text_textures, int textures_count,
                      Scroll *scroll, State *state) {

  int event_count = 0;
  int err = 0;

  while (SDL_PollEvent(&sdl_event) > 0) {
    event_count += 1;

    SDL_RenderClear(renderer);

    // Q(UIT) START
    if (sdl_event.type == SDL_QUIT || sdl_event.type == SDL_KEYDOWN &&
                                          sdl_event.key.state == SDL_PRESSED &&
                                          sdl_event.key.keysym.sym == SDLK_q) {
      state->keep_window_open = false;
      return event_count;
      // Q(UIT) END

      // CTRL START
    } else if (sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_LCTRL ||
                sdl_event.key.keysym.sym == SDLK_RCTRL)) {
      state->ctrl_pressed = true;
    } else if (sdl_event.type == SDL_KEYUP &&
               sdl_event.key.state == SDL_RELEASED &&
               (sdl_event.key.keysym.sym == SDLK_LCTRL ||
                sdl_event.key.keysym.sym == SDLK_RCTRL)) {
      state->ctrl_pressed = false;
      // CTRL END

      // SHIFT START
    } else if (sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_LSHIFT ||
                sdl_event.key.keysym.sym == SDLK_RSHIFT)) {
      state->shift_pressed = true;
    } else if (sdl_event.type == SDL_KEYUP &&
               sdl_event.key.state == SDL_RELEASED &&
               (sdl_event.key.keysym.sym == SDLK_LSHIFT ||
                sdl_event.key.keysym.sym == SDLK_RSHIFT)) {
      state->shift_pressed = false;
      // SHIFT END

      // MOUSE START
    } else if (sdl_event.type == SDL_MOUSEBUTTONDOWN &&
               sdl_event.button.button == SDL_BUTTON_LEFT &&
               sdl_event.button.state == SDL_PRESSED) {
      state->left_mouse_button_pressed = true;
      (void)SDL_GetMouseState(&scroll->highlight_start_x,
                              &scroll->highlight_start_y);
    } else if (sdl_event.type == SDL_MOUSEBUTTONUP &&
               sdl_event.button.button == SDL_BUTTON_LEFT &&
               sdl_event.button.state == SDL_RELEASED) {
      state->left_mouse_button_pressed = false;
      // MOUSE END

      // SCROLL VERTICAL START
    } else if (!state->ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.y != 0) {
      scroll->vertical_offset += VERTICAL_SCROLL_MULT * sdl_event.wheel.y;

      scroll->vertical_lower_bound = -scroll->vertical_text;
      scroll->vertical_upper_bound = 0;

      scroll->vertical_offset =
          clamp(scroll->vertical_offset, scroll->vertical_lower_bound,
                scroll->vertical_upper_bound);
      // SCROLL VERTICAL END

      // SCROLL HORIZONTAL START
    } else if (!state->ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.x != 0) {
      scroll->horizontal_offset += HORIZONTAL_SCROLL_MULT * sdl_event.wheel.x;

      int w_width = 0;
      SDL_GetWindowSizeInPixels(window, &w_width, NULL);
      // NOTE: if lower_bound is 0, then no horizontal scrolling
      // otherwise half max line_len amount scrolling
      scroll->horizontal_lower_bound =
          scroll->horizontal_text >= w_width ? -scroll->horizontal_text : 0;
      scroll->horizontal_upper_bound = 0;

      scroll->horizontal_offset =
          clamp(scroll->horizontal_offset, scroll->horizontal_lower_bound,
                scroll->horizontal_upper_bound);
      // SCROLL HORIZONTAL END

      // FONT RESIZE WITH MOUSEWHEEL START
    } else if (state->ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.y != 0) {
      FONT_SIZE += FONT_INCREMENT * sign(sdl_event.wheel.y);
      FONT_SIZE = clamp(FONT_SIZE, FONT_LOWER_BOUND, FONT_UPPER_BOUND);
      TTF_SetFontSize(font, FONT_SIZE);
      state->refresh_tokens = true;
      // FONT RESIZE WITH MOUSEWHEEL END

      // FONT RESIZE +/- START
    } else if (state->ctrl_pressed && !state->shift_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_EQUALS ||
                sdl_event.key.keysym.sym == SDLK_MINUS)) {
      int old_font_size = FONT_SIZE;
      FONT_SIZE += FONT_INCREMENT * (sdl_event.key.keysym.sym == SDLK_EQUALS) -
                   FONT_INCREMENT * (sdl_event.key.keysym.sym == SDLK_MINUS);
      FONT_SIZE = clamp(FONT_SIZE, FONT_LOWER_BOUND, FONT_UPPER_BOUND);
      TTF_SetFontSize(font, FONT_SIZE);
      state->refresh_tokens = true;

      // FONT RESIZE +/- END

      // FONT RESIZE TO DEFAULT START
    } else if (state->ctrl_pressed && state->shift_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_EQUALS) {
      FONT_SIZE = DEFAULT_FONT_SIZE;
      TTF_SetFontSize(font, FONT_SIZE);
      state->refresh_tokens = true;
      // FONT RESIZE TO DEFAULT END
    }

    SDL_RenderClear(renderer);
    err = cpy_to_renderer(window, renderer, text_textures, textures_count,
                          scroll, state);
    if (err != EXIT_SUCCESS) {
      state->keep_window_open = false;
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
  State *state = calloc(1, sizeof(State));

  int err = EXIT_SUCCESS;
  err = cpy_to_renderer(window, renderer, text_textures, textures_count, scroll,
                        state);
  if (err != EXIT_SUCCESS) {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return err;
  }

  SDL_RenderPresent(renderer);

  Uint32 start = SDL_GetTicks();
  Uint32 end = SDL_GetTicks();
  float elapsed = 0;

  int handled_event_count = 0; // MAYBE: REMOVEME:

  state->keep_window_open = true;
  while (state->keep_window_open) {

    start = SDL_GetTicks();

    SDL_Event sdl_event;
    handled_event_count =
        handle_sdl_events(window, sdl_event, renderer, font, text_textures,
                          textures_count, scroll, state);
    if (!state->keep_window_open) {
      break;
    }

    contents = check_contents(filename, contents, &contents_len, &last_modified,
                              &state->file_modified);
    if (state->file_modified) {
      state->file_modified = false;

      tokens = update_tokens(tokens, contents, contents_len, tokenizer_config,
                             &tokens_count);

      text_textures = update_textures(text_textures, renderer, font, FONT_SIZE,
                                      tokens, tokens_count, &textures_count);

      SDL_RenderClear(renderer);
      err = cpy_to_renderer(window, renderer, text_textures, textures_count,
                            scroll, state);
      if (err != EXIT_SUCCESS) {
        break;
      }
    } else if (state->refresh_tokens) {
      state->refresh_tokens = false;

      text_textures = update_textures(text_textures, renderer, font, FONT_SIZE,
                                      tokens, tokens_count, &textures_count);

      SDL_RenderClear(renderer);
      err = cpy_to_renderer(window, renderer, text_textures, textures_count,
                            scroll, state);
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
  if (state != NULL) {
    free(state);
  }

  return err;
}