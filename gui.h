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

#define HORIZONTAL_SCROLL_MULT (-200)
#define VERTICAL_SCROLL_MULT 50

#define GUI_FONT "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
#define DEFAULT_FONT_SIZE 20
#define FONT_INCREMENT 4
#define FONT_LOWER_BOUND 12
#define FONT_UPPER_BOUND 64

#define VERTICAL_SCROLLBAR_WIDTH 15
#define HORIZONTAL_SCROLLBAR_HEIGHT 15

#define HORIZONTAL_PADDING (VERTICAL_SCROLLBAR_WIDTH + 5)
#define VERTICAL_PADDING (5)

#define FRAME_DELAY 16 // in milliseconds; ~60FPS

#define MILLISECOND 1
#define SECOND (1000 * MILLISECOND)

int BASE_FONT_SIZE = DEFAULT_FONT_SIZE; // MAYBE: move to state
int FONT_SIZE = DEFAULT_FONT_SIZE;      // MAYBE: move to state

// TODO: improve colors
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {255, 0, 0, 255};
const SDL_Color GREEN = {0, 255, 0, 255};
const SDL_Color BLUE = {0, 0, 255, 255};
const SDL_Color YELLOW = {255, 200, 0, 255};
const SDL_Color MAGENTA = {255, 0, 255, 255};
const SDL_Color GREY = {128, 128, 128, 255};
const SDL_Color MOUSE_HIGHLIGHT = {190, 240, 255, 128};
const SDL_Color SCROLLBAR_BG = {192, 192, 192, 64};
const SDL_Color SCROLLBAR_FG = {128, 128, 128, 128};

typedef struct {
  struct SDL_Texture *texture;
  Token *token;
  int x;
  int y;
  int w;
  int h;
} Texture;

typedef struct {
  int x;
  int y;
} Coord;

typedef struct {
  int window_width;
  int window_height;
  //
  bool keep_window_open;
  bool file_modified;
  //
  bool is_font_resized;
  float font_scale_factor;
  Uint64 font_size_unchanged_since;
  //
  bool ctrl_pressed;
  bool shift_pressed;
  bool left_mouse_button_pressed;
  //
  int max_horizontal_offset;
  int max_vertical_offset;
  //
  int horizontal_scroll;
  int vertical_scroll;
  //
  int highlight_stationary_texture_idx; // inclusive
  int highlight_moving_texture_idx;     // inclusive
  Coord *highlight_stationary_coord;
  Coord *highlight_moving_coord;
  //
  Uint64 last_mouse_click_tick;
} State;

void scale_rect(SDL_Rect *rect, float scale_factor) {
  rect->x = (float)rect->x * scale_factor;
  rect->y = (float)rect->y * scale_factor;
  rect->w = (float)rect->w * scale_factor;
  rect->h = (float)rect->h * scale_factor;
}

int texture_idx_from_mouse_pos(Texture **textures, int textures_count,
                               int mouse_x, int mouse_y, State *state) {
  for (int i = 0; i < textures_count; i += 1) {
    int texture_start_width =
        (HORIZONTAL_PADDING + textures[i]->x + state->horizontal_scroll) *
        state->font_scale_factor;
    int texture_start_height =
        (VERTICAL_PADDING + textures[i]->y + state->vertical_scroll) *
        state->font_scale_factor;

    // NOTE: only render what fits on window
    // continue if before window
    // break if after window
    if (texture_start_height + textures[i]->h < 0) {
      continue;
    } else if (state->window_height <= texture_start_height) {
      // NOTE: mouse is out of window to the bottom
      // use the last texture index that was still inside the window
      if (mouse_y > state->window_height) {
        return i - 1;
      }
      break;
    }

    if (
        // mouse is out of window to the top
        mouse_y < 0 ||
        // check based on cursor being on the same line
        texture_start_height <= mouse_y &&
            mouse_y < texture_start_height + textures[i]->h &&
            (
                // direct hit on the token
                texture_start_width < mouse_x &&
                    mouse_x < texture_start_width + textures[i]->w
                //
                ||
                // mouse is out of window to the left
                mouse_x < HORIZONTAL_PADDING
                //
                ||
                // mouse is out of window to the right
                mouse_x > state->window_width
                //
                )) {
      return i;
    }
  }
  return -1;
}

void handle_highlight(SDL_Renderer *renderer, Texture **textures,
                      int textures_count, State *state) {
  if (
      // stationary token index was neg
      state->highlight_stationary_texture_idx < 0 ||
      // mouse click, no moving
      state->highlight_stationary_coord->x ==
              state->highlight_moving_coord->x &&
          state->highlight_stationary_coord->y ==
              state->highlight_moving_coord->y) {
    return;
  }

  int start_idx = state->highlight_stationary_texture_idx;
  int end_idx = state->highlight_moving_texture_idx;

  int highlight_start_x = state->highlight_stationary_coord->x;
  int highlight_end_x = state->highlight_moving_coord->x;

  if (end_idx < start_idx) {
    start_idx = state->highlight_moving_texture_idx;
    end_idx = state->highlight_stationary_texture_idx;

    highlight_start_x = state->highlight_moving_coord->x;
    highlight_end_x = state->highlight_stationary_coord->x;
  }
  for (int i = start_idx; i <= end_idx; i += 1) {

    // NOTE: texture_start_width doesn't account for scroll,
    // because we want to leave highlight in place when horizontal scrolling
    int texture_start_width = HORIZONTAL_PADDING + textures[i]->x;
    int texture_start_height =
        VERTICAL_PADDING + textures[i]->y + state->vertical_scroll;

    int texture_char_size = textures[i]->w / textures[i]->token->vlen;
    int highlight_start_offset = 0;
    int hightlight_end_offset = 0;

    int start_diff = highlight_start_x - texture_start_width;
    if (i == start_idx && start_diff > 0 &&
        // only trim highlight start if mouse is inside the token
        texture_start_width <= highlight_start_x &&
        highlight_start_x <= texture_start_width + textures[i]->w) {
      highlight_start_offset =
          clamp(start_diff - start_diff % texture_char_size, 0, textures[i]->w);
    }
    int end_diff = texture_start_width + textures[i]->w - highlight_end_x;
    if (i == end_idx && end_diff > 0 &&
        // only trim highlight end if mouse is inside the token
        texture_start_width <= highlight_end_x &&
        highlight_start_x <= texture_start_width + textures[i]->w) {
      hightlight_end_offset =
          clamp(end_diff - end_diff % texture_char_size, 0, textures[i]->w);
    }

    SDL_Rect highlight_rect = {
        state->horizontal_scroll + texture_start_width + highlight_start_offset,
        texture_start_height,
        textures[i]->w - (highlight_start_offset + hightlight_end_offset),
        textures[i]->h};
    SDL_Color prev = {0};
    SDL_GetRenderDrawColor(renderer, (Uint8 *)&prev.r, (Uint8 *)&prev.g,
                           (Uint8 *)&prev.b, (Uint8 *)&prev.a);
    SDL_SetRenderDrawColor(renderer, MOUSE_HIGHLIGHT.r, MOUSE_HIGHLIGHT.g,
                           MOUSE_HIGHLIGHT.b, MOUSE_HIGHLIGHT.a);
    scale_rect(&highlight_rect, state->font_scale_factor);
    SDL_RenderFillRect(renderer, &highlight_rect);
    SDL_SetRenderDrawColor(renderer, prev.r, prev.g, prev.b, prev.a);
  }
}

void handle_double_click(Texture **textures, int textures_count, int idx,
                         State *state) {
  if (idx < 0) {
    return;
  }
  int idx_local = idx;
  int direction = 1;
  if (textures[idx]->token->t == TOKEN_STRING &&
      textures[idx]->token->vlen == 1 &&
      (*textures[idx]->token->v == '"' || *textures[idx]->token->v == '\'')) {
    char c = *textures[idx]->token->v;

    enum TOKEN_TYPE token_type;
    int direction_check_offset = 1;
    while (idx + direction_check_offset < textures_count &&
           (token_type = textures[idx + direction_check_offset]->token->t) &&
           (token_type == TOKEN_SPACES || token_type == TOKEN_TABS ||
            token_type == TOKEN_NEWLINE)) {
      direction_check_offset += 1;
    }

    if (idx + direction_check_offset < textures_count &&
        textures[idx + direction_check_offset]->token->t != TOKEN_STRING) {
      direction = -1;
    }

    // NOTE: boundaries are +1/-1, because we do `+= direction`
    // right after loop check
    while (0 < idx_local && idx_local < textures_count - 1) {
      idx_local += direction;
      if (textures[idx_local]->token->vlen == 1 &&
          textures[idx_local]->token->t == TOKEN_STRING &&
          *(textures[idx_local]->token->v) == c) {
        break;
      }
    }

    // NOTE: empty region, don't highlight
    if (abs(idx_local - idx) < 2) {
      return;
    }

  } else if (textures[idx]->token->vlen == 1) {
    char c = *(textures[idx]->token->v);
    int open_count = 0;
    if (c != '(' && c != ')' && c != '[' && c != ']' && c != '{' && c != '}' &&
        c != '<' && c != '>') {
      // don't highlight anything
      return;
    }
    if (c == ')' || c == ']' || c == '}' || c == '>') {
      direction = -1;
    }
    char end_c;
    switch (c) {
    case ')':
      end_c = '(';
      direction = -1;
      break;
    case ']':
      end_c = '[';
      direction = -1;
      break;
    case '}':
      end_c = '{';
      direction = -1;
      break;
    case '>':
      end_c = '<';
      direction = -1;
      break;
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
      fprintf(stderr, "unreachable - double-click handling");
      return;
    }
    // NOTE: allow highlighting inside brackets as WORD, STRING, COMMENT, etc
    enum TOKEN_TYPE token_type = textures[idx]->token->t;
    while (0 <= idx_local && idx_local < textures_count) {
      if (textures[idx_local]->token->vlen == 1 &&
          *textures[idx_local]->token->v == end_c &&
          textures[idx_local]->token->t == token_type && open_count == 1) {
        break;
      }
      if (textures[idx_local]->token->vlen == 1 &&
          *textures[idx_local]->token->v == c &&
          textures[idx_local]->token->t == token_type) {
        open_count += 1;
      }
      if (textures[idx_local]->token->vlen == 1 &&
          *textures[idx_local]->token->v == end_c &&
          textures[idx_local]->token->t == token_type && open_count > 1) {
        open_count -= 1;
      }
      idx_local += direction;
    }

    // NOTE: empty region, don't highlight
    if (abs(idx_local - idx) < 2) {
      return;
    }
  } else {
    // didn't match any highlighting criteria
    // highlight current token
    // for that increment the idx and idx_local
    // so that below incr/decr works properly
    idx -= direction;
    idx_local += direction;
  }

  // exclude selection bounds and only select insides
  idx += direction;
  idx_local -= direction;

  if (idx > idx_local) {
    int tmp = idx;
    idx = idx_local;
    idx_local = tmp;
  }

  state->highlight_stationary_texture_idx = idx;
  state->highlight_moving_texture_idx = idx_local;
  state->highlight_stationary_coord->x = HORIZONTAL_PADDING + textures[idx]->x;
  state->highlight_stationary_coord->y = VERTICAL_PADDING + textures[idx]->y;
  state->highlight_moving_coord->x =
      HORIZONTAL_PADDING + textures[idx_local]->x + textures[idx_local]->w;
  state->highlight_moving_coord->y =
      VERTICAL_PADDING + textures[idx_local]->y + textures[idx_local]->h;
}

void handle_scrollbars(SDL_Renderer *renderer, State *state) {

  SDL_Color prev = {0};
  SDL_GetRenderDrawColor(renderer, (Uint8 *)&prev.r, (Uint8 *)&prev.g,
                         (Uint8 *)&prev.b, (Uint8 *)&prev.a);

  // vertical scrollbar background
  SDL_Rect vertical_scrollbar_bg_rect = {0, 0, VERTICAL_SCROLLBAR_WIDTH,
                                         state->window_height};
  SDL_SetRenderDrawColor(renderer, SCROLLBAR_BG.r, SCROLLBAR_BG.g,
                         SCROLLBAR_BG.b, SCROLLBAR_BG.a);
  SDL_RenderFillRect(renderer, &vertical_scrollbar_bg_rect);

  // vertical scrollbar foreground
  int height_hundred_percent = state->window_height;
  SDL_Rect vertical_scrollbar_fg_rect = {
      0,
      height_hundred_percent * (-state->vertical_scroll) /
          state->max_vertical_offset,
      VERTICAL_SCROLLBAR_WIDTH,
      height_hundred_percent * state->window_height /
          state->max_vertical_offset};
  SDL_SetRenderDrawColor(renderer, SCROLLBAR_FG.r, SCROLLBAR_FG.g,
                         SCROLLBAR_FG.b, SCROLLBAR_FG.a);
  SDL_RenderFillRect(renderer, &vertical_scrollbar_fg_rect);

  // horizontal scrollbar background
  if (state->horizontal_scroll != 0) {
    SDL_Rect horizontal_scrollbar_bg_rect = {
        VERTICAL_SCROLLBAR_WIDTH,
        state->window_height - HORIZONTAL_SCROLLBAR_HEIGHT, state->window_width,
        HORIZONTAL_SCROLLBAR_HEIGHT};
    SDL_SetRenderDrawColor(renderer, SCROLLBAR_BG.r, SCROLLBAR_BG.g,
                           SCROLLBAR_BG.b, SCROLLBAR_BG.a);
    SDL_RenderFillRect(renderer, &horizontal_scrollbar_bg_rect);

    // horizontal scrollbar foreground
    int width_hundred_percent = state->window_width;
    SDL_Rect horizontal_scrollbar_fg_rect = {
        width_hundred_percent * (-state->horizontal_scroll) /
            state->max_horizontal_offset,
        state->window_height - HORIZONTAL_SCROLLBAR_HEIGHT,
        width_hundred_percent * state->window_height /
            state->max_horizontal_offset,
        HORIZONTAL_SCROLLBAR_HEIGHT};
    SDL_SetRenderDrawColor(renderer, SCROLLBAR_FG.r, SCROLLBAR_FG.g,
                           SCROLLBAR_FG.b, SCROLLBAR_FG.a);
    SDL_RenderFillRect(renderer, &horizontal_scrollbar_fg_rect);
  }

  SDL_SetRenderDrawColor(renderer, prev.r, prev.g, prev.b, prev.a);
}

// allocs memory
Texture **tokens_to_textures(SDL_Renderer *renderer, TTF_Font *font,
                             int font_size, Token **tokens, int tokens_count,
                             int *textures_count, State *state) {
  Texture **textures = calloc(tokens_count, sizeof(Texture *));

  int local_horizontal_offset = 0;
  int local_vertical_offset = 0;

  int max_horizontal_offset = 0;

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
    tp->x = local_horizontal_offset;
    tp->y = local_vertical_offset;
    tp->w = text_surface->w;
    tp->h = text_surface->h;

    textures[*textures_count] = tp;
    *textures_count += 1;

    SDL_FreeSurface(text_surface);

    if (tokens[i]->t == TOKEN_NEWLINE) {
      max_horizontal_offset =
          gt(max_horizontal_offset, local_horizontal_offset);
      // NOTE: if newline, extend the texture width to end of screen
      tp->w += 4 * state->window_width - local_horizontal_offset -
               tp->w; // FIXME: HACK: vertical scrolling fix
      local_horizontal_offset = 0;
      local_vertical_offset += text_surface->h;
    } else {
      local_horizontal_offset += text_surface->w;
    }
  }

  state->max_horizontal_offset = max_horizontal_offset;
  state->max_vertical_offset = local_vertical_offset;

  // NOTE: snap back if text fits on screen, but horizontal scroll is non-zero
  if (max_horizontal_offset < state->window_width) {
    state->horizontal_scroll = 0;
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
                          int tokens_count, int *textures_count, State *state) {
  free_textures(textures, *textures_count);
  *textures_count = 0;
  return tokens_to_textures(renderer, font, FONT_SIZE, tokens, tokens_count,
                            textures_count, state);
}

int cpy_to_renderer(SDL_Renderer *renderer, Texture **textures,
                    int textures_count, State *state) {

  handle_highlight(renderer, textures, textures_count, state);
  handle_scrollbars(renderer, state);

  for (int i = 0; i < textures_count; i += 1) {

    int texture_start_width =
        HORIZONTAL_PADDING + textures[i]->x + state->horizontal_scroll;
    int texture_start_height =
        VERTICAL_PADDING + textures[i]->y + state->vertical_scroll;

    // NOTE: only render what fits on window
    // continue if before window
    // break if after window
    if (texture_start_height + textures[i]->h < 0) {
      continue;
    } else if (state->window_height < texture_start_height) {
      break;
    }

    // NOTE: don't render newline char
    if (textures[i]->token->t == TOKEN_NEWLINE) {
      continue;
    }

    SDL_Rect text_rect = {texture_start_width, texture_start_height,
                          textures[i]->w, textures[i]->h};
    scale_rect(&text_rect, state->font_scale_factor);
    SDL_RenderCopy(renderer, textures[i]->texture, NULL, &text_rect);
  }

  return EXIT_SUCCESS;
}

int handle_sdl_events(SDL_Window *window, SDL_Event sdl_event,
                      SDL_Renderer *renderer, TTF_Font *font,
                      Texture **text_textures, int textures_count,
                      State *state) {

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
    } else if (state->left_mouse_button_pressed &&
               sdl_event.type == SDL_MOUSEMOTION) {
      int mouse_x = 0;
      int mouse_y = 0;
      (void)SDL_GetMouseState(&mouse_x, &mouse_y);
      int idx = texture_idx_from_mouse_pos(text_textures, textures_count,
                                           mouse_x, mouse_y, state);
      if (idx >= 0) {
        state->highlight_moving_texture_idx = idx;
        state->highlight_moving_coord->x = mouse_x;
        state->highlight_moving_coord->y = mouse_y;
      }

    } else if (sdl_event.type == SDL_MOUSEBUTTONDOWN &&
               sdl_event.button.button == SDL_BUTTON_LEFT &&
               sdl_event.button.state == SDL_PRESSED) {
      Uint64 current_tick = SDL_GetTicks64();
      state->left_mouse_button_pressed = true;
      int mouse_x = 0;
      int mouse_y = 0;
      (void)SDL_GetMouseState(&mouse_x, &mouse_y);

      int idx = texture_idx_from_mouse_pos(text_textures, textures_count,
                                           mouse_x, mouse_y, state);
      if (idx >= 0) {
        state->highlight_stationary_texture_idx = idx;
        state->highlight_stationary_coord->x = mouse_x;
        state->highlight_stationary_coord->y = mouse_y;

        // NOTE: set moving values to stationary ones
        // to not highlight on click
        // if mouse is moved, then will highlight
        state->highlight_moving_texture_idx =
            state->highlight_stationary_texture_idx;
        state->highlight_moving_coord->x = mouse_x;
        state->highlight_moving_coord->y = mouse_y;
      }

      if (current_tick - state->last_mouse_click_tick <= 250 * MILLISECOND) {
        handle_double_click(text_textures, textures_count, idx, state);
      }
      state->last_mouse_click_tick = current_tick;

    } else if (sdl_event.type == SDL_MOUSEBUTTONUP &&
               sdl_event.button.button == SDL_BUTTON_LEFT &&
               sdl_event.button.state == SDL_RELEASED) {
      state->left_mouse_button_pressed = false;
      // MOUSE END

      // SCROLL VERTICAL START
    } else if (!state->ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.y != 0) {
      state->vertical_scroll = clamp(
          state->vertical_scroll + VERTICAL_SCROLL_MULT * sdl_event.wheel.y,
          -state->max_vertical_offset, 0);
      // SCROLL VERTICAL END

      // SCROLL HORIZONTAL START
    } else if (!state->ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.x != 0) {

      // NOTE: if lower_bound is 0, then no horizontal scrolling
      state->horizontal_scroll = clamp(
          state->horizontal_scroll + HORIZONTAL_SCROLL_MULT * sdl_event.wheel.x,
          state->max_horizontal_offset >= state->window_width
              ? -state->max_horizontal_offset
              : 0,
          0);
      // SCROLL HORIZONTAL END

      // FONT RESIZE WITH MOUSEWHEEL START
    } else if (state->ctrl_pressed && sdl_event.type == SDL_MOUSEWHEEL &&
               sdl_event.wheel.y != 0) {
      FONT_SIZE += FONT_INCREMENT * sign(sdl_event.wheel.y);
      FONT_SIZE = clamp(FONT_SIZE, FONT_LOWER_BOUND, FONT_UPPER_BOUND);
      TTF_SetFontSize(font, FONT_SIZE);
      state->is_font_resized = true;
      state->font_scale_factor = (float)FONT_SIZE / (float)BASE_FONT_SIZE;
      state->font_size_unchanged_since = SDL_GetTicks64();
      // FONT RESIZE WITH MOUSEWHEEL END

      // FONT RESIZE +/- START
    } else if (state->ctrl_pressed && !state->shift_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_EQUALS ||
                sdl_event.key.keysym.sym == SDLK_MINUS)) {

      // NOTE: revert font_scaling
      state->max_horizontal_offset =
          (float)state->max_horizontal_offset / state->font_scale_factor;
      state->max_vertical_offset =
          (float)state->max_vertical_offset / state->font_scale_factor;

      int old_font_size = FONT_SIZE;
      FONT_SIZE += FONT_INCREMENT * (sdl_event.key.keysym.sym == SDLK_EQUALS) -
                   FONT_INCREMENT * (sdl_event.key.keysym.sym == SDLK_MINUS);
      FONT_SIZE = clamp(FONT_SIZE, FONT_LOWER_BOUND, FONT_UPPER_BOUND);
      TTF_SetFontSize(font, FONT_SIZE);
      state->is_font_resized = true;
      state->font_scale_factor = (float)FONT_SIZE / (float)BASE_FONT_SIZE;
      state->font_size_unchanged_since = SDL_GetTicks64();

      // NOTE: apply new font scaling
      state->max_horizontal_offset =
          (float)state->max_horizontal_offset * state->font_scale_factor;
      state->max_vertical_offset =
          (float)state->max_vertical_offset * state->font_scale_factor;
      // FONT RESIZE +/- END

      // FONT RESIZE TO DEFAULT START
    } else if (state->ctrl_pressed && state->shift_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_EQUALS) {

      // NOTE: revert font_scaling
      state->max_horizontal_offset =
          (float)state->max_horizontal_offset / state->font_scale_factor;
      state->max_vertical_offset =
          (float)state->max_vertical_offset / state->font_scale_factor;

      FONT_SIZE = DEFAULT_FONT_SIZE;
      BASE_FONT_SIZE = DEFAULT_FONT_SIZE;
      TTF_SetFontSize(font, FONT_SIZE);
      state->is_font_resized = true;
      state->font_scale_factor = 1.0f;
      state->font_size_unchanged_since = SDL_GetTicks64();
      // FONT RESIZE TO DEFAULT END

      // WINDOW RESIZE START
    } else if (sdl_event.type == SDL_WINDOWEVENT &&
               sdl_event.window.event == SDL_WINDOWEVENT_RESIZED) {
      (void)SDL_GetWindowSize(window, &state->window_width,
                              &state->window_height);
      // WINDOW RESIZE END
    }

    SDL_RenderClear(renderer);
    err = cpy_to_renderer(renderer, text_textures, textures_count, state);
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

  State *state = calloc(1, sizeof(State));
  state->highlight_stationary_coord = calloc(1, sizeof(Coord));
  state->highlight_moving_coord = calloc(1, sizeof(Coord));
  (void)SDL_GetWindowSize(window, &state->window_width, &state->window_height);
  state->font_scale_factor = 1.0f;
  state->font_size_unchanged_since = SDL_GetTicks64();

  int textures_count = 0;
  Texture **text_textures = tokens_to_textures(
      renderer, font, FONT_SIZE, tokens, tokens_count, &textures_count, state);

  SDL_RenderClear(renderer);

  int err = EXIT_SUCCESS;
  err = cpy_to_renderer(renderer, text_textures, textures_count, state);
  if (err != EXIT_SUCCESS) {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return err;
  }

  SDL_RenderPresent(renderer);

  Uint32 start = SDL_GetTicks64();
  Uint32 end = SDL_GetTicks64();
  float elapsed = 0;

  int handled_event_count = 0;

  state->keep_window_open = true;
  while (state->keep_window_open) {

    start = SDL_GetTicks64();

    SDL_Event sdl_event;
    handled_event_count =
        handle_sdl_events(window, sdl_event, renderer, font, text_textures,
                          textures_count, state);
    if (!state->keep_window_open) {
      break;
    }

    contents = check_contents(filename, contents, &contents_len, &last_modified,
                              &state->file_modified);
    if (state->file_modified) {
      state->file_modified = false;

      tokens = update_tokens(tokens, contents, contents_len, tokenizer_config,
                             &tokens_count);

      text_textures =
          update_textures(text_textures, renderer, font, FONT_SIZE, tokens,
                          tokens_count, &textures_count, state);

      SDL_RenderClear(renderer);
      err = cpy_to_renderer(renderer, text_textures, textures_count, state);
      if (err != EXIT_SUCCESS) {
        break;
      }
      // NOTE: if we need to refresh tokens (eg font resize)
      // but we manually went back to default size
      // before re-calculating the textures,
      // then don't recalculate the tokens
    } else if (state->is_font_resized && FONT_SIZE == DEFAULT_FONT_SIZE) {
      state->is_font_resized = false;
      BASE_FONT_SIZE = FONT_SIZE;
      state->font_scale_factor = 1.0f;

      // NOTE: if we need to refresh tokens (eg font resize)
      // and the font hasn't changed in x seconds
      // recalculate the textures for better quality text
    } else if (state->is_font_resized &&
               10 * SECOND <
                   SDL_GetTicks64() - state->font_size_unchanged_since) {
      state->is_font_resized = false;
      BASE_FONT_SIZE = FONT_SIZE;
      state->font_scale_factor = 1.0f;

      // MAYBE: TODO: make update_textures parallel safe
      text_textures =
          update_textures(text_textures, renderer, font, FONT_SIZE, tokens,
                          tokens_count, &textures_count, state);

      SDL_RenderClear(renderer);
      err = cpy_to_renderer(renderer, text_textures, textures_count, state);
      if (err != EXIT_SUCCESS) {
        break;
      }
      SDL_RenderPresent(renderer);
    }

    end = SDL_GetTicks64();
    elapsed = end - start;
    if (elapsed <= FRAME_DELAY) {
      SDL_Delay(FRAME_DELAY - elapsed);
    }
    // NOTE: render only when event happened, otherwise don't present.
    // Aim is to improve idle performance.
    // However, if this doesn't feel good, remove the if-statement
    // and just present render.
    if (handled_event_count > 0) {
      SDL_RenderPresent(
          renderer); // MAYBE: NOTE: always present current renderer
    }
  }

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();

  free_textures(text_textures, textures_count);
  free_tokens(tokens, tokens_count);
  free_contents(contents);
  if (state != NULL) {
    if (state->highlight_stationary_coord != NULL) {
      free(state->highlight_stationary_coord);
    }
    if (state->highlight_moving_coord != NULL) {
      free(state->highlight_moving_coord);
    }
    free(state);
  }

  return err;
}