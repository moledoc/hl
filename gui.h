#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "color_scheme.h"
#include "consts.h"
#include "tokens.h"
#include "utils.h"

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 540

#define HORIZONTAL_SCROLL_MULT (-200)
#define VERTICAL_SCROLL_MULT 50

#define GUI_FONT "/usr/share/fonts/truetype/freefont/FreeMono.ttf"
#define DEFAULT_FONT_SIZE 20
#define FONT_INCREMENT 1
#define FONT_LOWER_BOUND 12
#define FONT_UPPER_BOUND 64

#define VERTICAL_SCROLLBAR_WIDTH (15)
#define HORIZONTAL_SCROLLBAR_HEIGHT 15

#define HORIZONTAL_PADDING_BASE (VERTICAL_SCROLLBAR_WIDTH + 5)
#define VERTICAL_PADDING (5)
#define ROW_NUMBER_PADDING (10)

#define FRAME_DELAY 16 // in milliseconds; ~60FPS

#define MILLISECOND 1
#define SECOND (1000 * MILLISECOND)

#define FONT_RENDERING_DELAY 2

int BASE_FONT_SIZE = DEFAULT_FONT_SIZE; // MAYBE: move to state
int FONT_SIZE = DEFAULT_FONT_SIZE;      // MAYBE: move to state
int HORIZONTAL_PADDING = (HORIZONTAL_PADDING_BASE);
int ROW_NUMBER_WIDTH = 0; // NOTE: will be updated once it's calculated

// TODO: add SEARCH_BUF_SIZE overflow checks NOTE: might already be done
// TODO: handle case when search text doesn't fit in the window
#define SEARCH_BUF_SIZE (4096 + 1)
char SEARCH_BUF[SEARCH_BUF_SIZE] = {0};
int SEARCH_BUF_OFFSET = 0;

// TODO: handle case when goto_line text doesn't fit in the window
#define GOTO_LINE_BUF_SIZE (100 + 1)
char GOTO_LINE_BUF[GOTO_LINE_BUF_SIZE] = {0};
int GOTO_LINE_BUF_OFFSET = 0;

typedef struct {
  struct SDL_Texture *texture;
  Token *token;
  int x;
  int y;
  int w;
  int h;
  int r;
  int c;
} Texture;

typedef struct {
  int x;
  int y;
} Coord;

typedef struct {
  int window_width;
  int window_height;
  //
  TTF_Font *font;
  //
  SDL_Texture *clearing;
  //
  bool keep_window_open;
  bool file_modified;
  bool color_scheme_modified;
  //
  bool is_font_resized;
  float font_scale_factor;
  Uint64 font_size_unchanged_since;
  //
  bool ctrl_pressed;  // TODO: use sdl_event.key.keysym.mod == KMOD_CTRL
  bool shift_pressed; // TODO: use sdl_event.key.keysym.mod == KMOD_SHIFT
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
  //
  Texture **row_nr_textures;
  int rows_count;
  //
  bool search_mode;
  //
  bool goto_line_mode;
} State;

typedef struct SearchResult {
  struct SearchResult *next;
  struct SearchResult *prev;
  Coord *start;
  Coord *end;
  int start_texture_idx;
  int end_texture_idx;
  char *val;
} SearchResult;

SearchResult *search_results = NULL;

void add_search_result(Coord *start, Coord *end, int start_texture_idx,
                       int end_texture_idx, State *state) {

  SearchResult *new = calloc(1, sizeof(SearchResult));
  new->start = calloc(1, sizeof(Coord));
  new->end = calloc(1, sizeof(Coord));
  new->start->x = start->x;
  new->start->y = start->y;
  new->end->x = end->x;
  new->end->y = end->y;
  new->start_texture_idx = start_texture_idx;
  new->end_texture_idx = end_texture_idx;

  if (search_results == NULL) {
    new->val = calloc(SEARCH_BUF_OFFSET, sizeof(char));
    memcpy(new->val, SEARCH_BUF + 1, SEARCH_BUF_OFFSET - 1);
    new->next = new;
    new->prev = new;
    search_results = new;
    return;
  }
  new->val = search_results->val; // NOTE: reuse str pointer; is slightly
                                  // dangerous, but lets see how badly it goes

  SearchResult *cur = search_results;
  for (; cur->next != search_results; cur = cur->next) {
    ;
  }
  cur->next = new;
  new->prev = cur;
  new->next = search_results;
  search_results->prev = new;
}

void print_search_results() {
  SearchResult *cur = search_results;
  for (; cur->next != search_results; cur = cur->next) {
    printf("(%d, %d), (%d, %d): %s\n", cur->start->x, cur->start->y,
           cur->end->x, cur->end->y, SEARCH_BUF + 1);
  }
}

void *free_search_results() {
  if (search_results == NULL) {
    return NULL;
  }
  if (search_results->val != NULL) {
    free(search_results->val); // NOTE: reused pointer for all search results
  }
  SearchResult *cur = search_results;
  while (cur != search_results) {
    SearchResult *me = search_results;
    ((SearchResult *)search_results->prev)->next = NULL;
    search_results = (SearchResult *)search_results->next;
    if (me->start != NULL) {
      free(me->start);
    }
    if (me->end != NULL) {
      free(me->start);
    }
    free(me);
  }
  return NULL;
}

typedef struct SearchHistory {
  struct SearchHistory *next;
  struct SearchHistory *prev;
  char *val;
} SearchHistory;

SearchHistory *search_history = NULL;

void add_to_search_history() {
  // NOTE: if the current search is the most resent in history, don't add it
  if (search_history != NULL &&
      strcmp(search_history->val, SEARCH_BUF + 1) == 0) {
    return;
  }

  SearchHistory *new = calloc(1, sizeof(SearchHistory));
  new->val = calloc(SEARCH_BUF_OFFSET, sizeof(char));

  if (SEARCH_BUF_OFFSET <= 1) {
    return;
  }

  memcpy(new->val, SEARCH_BUF + 1,
         SEARCH_BUF_OFFSET - 1); // NOTE: +1/-1 to account for '/'
  new->next = search_history;
  if (search_history != NULL) {
    search_history->prev = new;
  }
  search_history = new;
}

void *free_search_history() {
  // NOTE: roll history to the top
  for (; search_history != NULL && search_history->prev != NULL;
       search_history = search_history->prev) {
    ;
  }

  for (; search_history != NULL;) {
    if (search_history->val != NULL) {
      free(search_history->val);
    }
    SearchHistory *me = search_history;
    search_history = search_history->next;
    free(me);
  }
  return NULL;
}

void scale_texture_font(Texture **textures, int textures_count, State *state) {
  for (int i = 0; i < textures_count; i += 1) {
    textures[i]->x = rint((float)textures[i]->x * state->font_scale_factor);
    textures[i]->y = rint((float)textures[i]->y * state->font_scale_factor);
    textures[i]->w = rint((float)textures[i]->w * state->font_scale_factor);
    textures[i]->h = rint((float)textures[i]->h * state->font_scale_factor);
  }
}

void scale_font(Texture **textures, int textures_count, State *state) {

  state->max_horizontal_offset =
      rint((float)state->max_horizontal_offset * state->font_scale_factor);
  state->max_vertical_offset =
      rint((float)state->max_vertical_offset * state->font_scale_factor);

  state->horizontal_scroll =
      rint((float)state->horizontal_scroll * state->font_scale_factor);
  state->vertical_scroll =
      rint((float)state->vertical_scroll * state->font_scale_factor);

  state->highlight_stationary_coord->x = rint(
      (float)state->highlight_stationary_coord->x * state->font_scale_factor);
  state->highlight_stationary_coord->y = rint(
      (float)state->highlight_stationary_coord->y * state->font_scale_factor);

  state->highlight_moving_coord->x =
      rint((float)state->highlight_moving_coord->x * state->font_scale_factor);
  state->highlight_moving_coord->y =
      rint((float)state->highlight_moving_coord->y * state->font_scale_factor);

  scale_texture_font(textures, textures_count, state);
}

void reset_scale_texture_font(Texture **textures, int textures_count,
                              State *state) {
  for (int i = 0; i < textures_count; i += 1) {
    textures[i]->x = rint((float)textures[i]->x / state->font_scale_factor);
    textures[i]->y = rint((float)textures[i]->y / state->font_scale_factor);
    textures[i]->w = rint((float)textures[i]->w / state->font_scale_factor);
    textures[i]->h = rint((float)textures[i]->h / state->font_scale_factor);
  }
}

void reset_font_scale(Texture **textures, int textures_count, State *state) {

  state->highlight_stationary_coord->x = rint(
      (float)state->highlight_stationary_coord->x / state->font_scale_factor);
  state->highlight_moving_coord->x =
      rint((float)state->highlight_moving_coord->x / state->font_scale_factor);

  state->max_horizontal_offset =
      rint((float)state->max_horizontal_offset / state->font_scale_factor);
  state->max_vertical_offset =
      rint((float)state->max_vertical_offset / state->font_scale_factor);

  state->horizontal_scroll =
      rint((float)state->horizontal_scroll / state->font_scale_factor);
  state->vertical_scroll =
      rint((float)state->vertical_scroll / state->font_scale_factor);

  state->highlight_stationary_coord->x = rint(
      (float)state->highlight_stationary_coord->x / state->font_scale_factor);
  state->highlight_stationary_coord->y = rint(
      (float)state->highlight_stationary_coord->y / state->font_scale_factor);

  state->highlight_moving_coord->x =
      rint((float)state->highlight_moving_coord->x / state->font_scale_factor);
  state->highlight_moving_coord->y =
      rint((float)state->highlight_moving_coord->y / state->font_scale_factor);

  reset_scale_texture_font(textures, textures_count, state);
}

int texture_idx_from_mouse_pos(Texture **textures, int textures_count,
                               int mouse_x, int mouse_y, State *state) {

  for (int i = 0; i < textures_count; i += 1) {
    int texture_start_width =
        HORIZONTAL_PADDING + textures[i]->x + state->horizontal_scroll;
    int texture_start_height =
        VERTICAL_PADDING + textures[i]->y + state->vertical_scroll;

    // NOTE: only render what fits on window
    // continue if before window
    // break if after window
    if (texture_start_height + textures[i]->h < 0 ||
        texture_start_width > state->window_width ||
        texture_start_width + textures[i]->w < 0) {
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

// TODO: FIXME: when font is scaled, existing highlight might move or get gaps
// NOTE: MAYBE: FIXME: when highlighting with mouse downwards and cursor is out
// of window to left/right, then currently last highlighted line's first token
// is also highlighted
// NOTE: code shared with handle_copy_to_clipboard,
// but too early to abstract anything.
// Just a note that any changes here might also be needed to be done in
// handle_copy_to_clipboard.
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
        texture_start_width + highlight_start_offset + state->horizontal_scroll,
        texture_start_height,
        textures[i]->w - (highlight_start_offset + hightlight_end_offset),
        textures[i]->h};
    SDL_Color prev = {0};
    SDL_GetRenderDrawColor(renderer, (Uint8 *)&prev.r, (Uint8 *)&prev.g,
                           (Uint8 *)&prev.b, (Uint8 *)&prev.a);
    SDL_SetRenderDrawColor(renderer, color_scheme->mouse_highlight.r,
                           color_scheme->mouse_highlight.g,
                           color_scheme->mouse_highlight.b,
                           color_scheme->mouse_highlight.a);

    SDL_RenderFillRect(renderer, &highlight_rect);
    SDL_SetRenderDrawColor(renderer, prev.r, prev.g, prev.b, prev.a);
  }
}

void handle_double_click(Texture **textures, int textures_count, int idx,
                         State *state) {
  if (idx < 0) {
    return;
  }

  int idx_local = textures[idx]->token->s_until;

  if (idx > idx_local) {
    int tmp = idx;
    idx = idx_local;
    idx_local = tmp;
  }

  // exclude selection bounds and only select insides
  if (idx != idx_local && idx < textures_count - 1 && idx_local > 0) {
    idx += 1;
    idx_local -= 1;
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
  SDL_Rect vertical_scrollbar_bg_rect = {
      ROW_NUMBER_WIDTH, 0, VERTICAL_SCROLLBAR_WIDTH, state->window_height};
  SDL_SetRenderDrawColor(
      renderer, color_scheme->scrollbar_bg.r, color_scheme->scrollbar_bg.g,
      color_scheme->scrollbar_bg.b, color_scheme->scrollbar_bg.a);
  SDL_RenderFillRect(renderer, &vertical_scrollbar_bg_rect);

  // vertical scrollbar foreground
  int height_hundred_percent = state->window_height;
  SDL_Rect vertical_scrollbar_fg_rect = {
      ROW_NUMBER_WIDTH,
      height_hundred_percent * (-state->vertical_scroll) /
          state->max_vertical_offset,
      VERTICAL_SCROLLBAR_WIDTH,
      height_hundred_percent * state->window_height /
          state->max_vertical_offset};
  SDL_SetRenderDrawColor(
      renderer, color_scheme->scrollbar_fg.r, color_scheme->scrollbar_fg.g,
      color_scheme->scrollbar_fg.b, color_scheme->scrollbar_fg.a);
  SDL_RenderFillRect(renderer, &vertical_scrollbar_fg_rect);

  // horizontal scrollbar background
  if (state->horizontal_scroll != 0) {
    SDL_Rect horizontal_scrollbar_bg_rect = {
        HORIZONTAL_PADDING - ROW_NUMBER_PADDING,
        state->window_height - HORIZONTAL_SCROLLBAR_HEIGHT, state->window_width,
        HORIZONTAL_SCROLLBAR_HEIGHT};
    SDL_SetRenderDrawColor(
        renderer, color_scheme->scrollbar_bg.r, color_scheme->scrollbar_bg.g,
        color_scheme->scrollbar_bg.b, color_scheme->scrollbar_bg.a);
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
    SDL_SetRenderDrawColor(
        renderer, color_scheme->scrollbar_fg.r, color_scheme->scrollbar_fg.g,
        color_scheme->scrollbar_fg.b, color_scheme->scrollbar_fg.a);
    SDL_RenderFillRect(renderer, &horizontal_scrollbar_fg_rect);
  }

  SDL_SetRenderDrawColor(renderer, prev.r, prev.g, prev.b, prev.a);
}

void handle_actionbox(SDL_Renderer *renderer, State *state, bool handle,
                      char *buf) {

  if (!handle) {
    return;
  }

  SDL_Surface *actionbox_surface =
      TTF_RenderUTF8_Solid(state->font, buf, color_scheme->actionbox_text_fg);
  if (actionbox_surface == NULL) {
    fprintf(stderr, "[WARNING]: failed to create actionbox text surface: %s\n",
            TTF_GetError());
    return;
  }

  SDL_Texture *actionbox_texture =
      SDL_CreateTextureFromSurface(renderer, actionbox_surface);

  if (actionbox_texture == NULL) {
    fprintf(stderr, "failed to create actionbox text texture: %s\n",
            SDL_GetError());
    SDL_FreeSurface(actionbox_surface);
    return;
  }

  int start_x = 0;
  int start_y = state->window_height - actionbox_surface->h;

  SDL_Color prev = {0};
  SDL_GetRenderDrawColor(renderer, (Uint8 *)&prev.r, (Uint8 *)&prev.g,
                         (Uint8 *)&prev.b, (Uint8 *)&prev.a);
  // NOTE: actionbox bar
  SDL_SetRenderDrawColor(
      renderer, color_scheme->actionbox_box.r, color_scheme->actionbox_box.g,
      color_scheme->actionbox_box.b, color_scheme->actionbox_box.a);
  SDL_Rect clearing_rect = {start_x, start_y, state->window_width,
                            actionbox_surface->h};
  SDL_RenderFillRect(renderer, &clearing_rect);

  // NOTE: actionbox text bg
  SDL_SetRenderDrawColor(renderer, color_scheme->actionbox_text_bg.r,
                         color_scheme->actionbox_text_bg.g,
                         color_scheme->actionbox_text_bg.b,
                         color_scheme->actionbox_text_bg.a);
  SDL_Rect rect_text_bg = {start_x, start_y, actionbox_surface->w,
                           actionbox_surface->h};
  SDL_RenderFillRect(renderer, &rect_text_bg);

  SDL_SetRenderDrawColor(renderer, prev.r, prev.g, prev.b, prev.a);

  // NOTE: actionbox text fg
  SDL_Rect rect_text_fg = {start_x, start_y, actionbox_surface->w,
                           actionbox_surface->h};
  SDL_RenderCopy(renderer, actionbox_texture, NULL, &rect_text_fg);

  SDL_FreeSurface(actionbox_surface);
  SDL_DestroyTexture(actionbox_texture);
}

void handle_search_results(Texture **textures, int textures_count,
                           State *state) {

  char *sliding_window = calloc(SEARCH_BUF_OFFSET, sizeof(char));
  int sliding_window_filled = 0;

  int textures_offset_start = 0;
  int char_offset_start = 0;

  int textures_offset_end = 0;
  int char_offset_end = 0;

  Coord start_coord = {0};
  Coord end_coord = {0};

  while (textures_offset_start < textures_count) {

    int texture_char_size = textures[textures_offset_start]->w /
                            textures[textures_offset_start]->token->vlen;

    start_coord.x = HORIZONTAL_PADDING + textures[textures_offset_start]->x +
                    texture_char_size * char_offset_start;
    start_coord.y = VERTICAL_PADDING + textures[textures_offset_start]->y;

    // NOTE: fill sliding window
    while (sliding_window_filled <
           SEARCH_BUF_OFFSET - 1) { // -1 to account for '/'
      if (char_offset_end >= textures[textures_offset_end]->token->vlen) {
        textures_offset_end += 1;
        if (textures_offset_end >= textures_count) {
          free(sliding_window);
          return;
        }
        char_offset_end = 0;
      }
      memcpy(sliding_window + sliding_window_filled,
             (textures[textures_offset_end]->token->v + char_offset_end), 1);
      sliding_window_filled += 1;
      char_offset_end += 1;
    }

    if (strcmp(SEARCH_BUF + 1, sliding_window) == 0) {
      end_coord.x = HORIZONTAL_PADDING + textures[textures_offset_end]->x +
                    texture_char_size * char_offset_end;
      end_coord.y = VERTICAL_PADDING + textures[textures_offset_end]->y;
      add_search_result(&start_coord, &end_coord, textures_offset_start,
                        textures_offset_end, state);
    }

    sliding_window_filled -= 1;
    memmove(sliding_window, sliding_window + 1, SEARCH_BUF_OFFSET - 2);

    char_offset_start += 1;
    if (char_offset_start >= textures[textures_offset_start]->token->vlen) {
      textures_offset_start += 1;
      if (textures_offset_start >= textures_count) {
        free(sliding_window);
        return;
      }
      char_offset_start = 0;
    }
  }

  free(sliding_window);
}

// NOTE: code shared with handle_highlight, but too early to abstract
// anything. Just a note that any changes here might also be needed to be done
// in handle_highlight. NOTE: there is small delay when pasting after copying
// from application, might need to investigate in the future
void handle_copy_to_clipboard(Texture **textures, int textures_count,
                              State *state) {
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

  int copy_text_len = 0;
  for (int i = start_idx; i <= end_idx; i += 1) {
    copy_text_len += textures[i]->token->vlen;
  }

  char *copy_to_clipboard = calloc(copy_text_len + 1, sizeof(char));

  int offset = 0;
  for (int i = start_idx; i <= end_idx; i += 1) {

    int texture_start_width =
        HORIZONTAL_PADDING + textures[i]->x + state->horizontal_scroll;
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

    int start_char_offset = highlight_start_offset / texture_char_size;
    int end_char_offset = hightlight_end_offset / texture_char_size;

    memcpy(copy_to_clipboard + offset,
           textures[i]->token->v + start_char_offset,
           textures[i]->token->vlen - end_char_offset);
    offset += textures[i]->token->vlen - start_char_offset - end_char_offset;
  }
  SDL_SetClipboardText((const char *)copy_to_clipboard);
  free(copy_to_clipboard);
}

// allocs memory
void row_nrs_to_textures(SDL_Renderer *renderer, int font_size, int rows,
                         State *state) {
  Texture **textures = calloc(rows, sizeof(Texture *));

  SDL_Color color = color_scheme->numbers;

  int local_vertical_offset = 0;

  char buf[10] = {0};

  for (int i = 0; i < rows; i += 1) {

    snprintf(buf, sizeof(buf), "%d", i + 1);
    SDL_Surface *row_nr_surface = TTF_RenderUTF8_Solid(state->font, buf, color);
    if (row_nr_surface == NULL) {
      fprintf(stderr, "failed to create row nr surface: %s\n", TTF_GetError());
      return;
    }
    memset(buf, '\0', sizeof(buf));

    SDL_Texture *row_nr_texture =
        SDL_CreateTextureFromSurface(renderer, row_nr_surface);

    if (row_nr_texture == NULL) {
      fprintf(stderr, "failed to create row nr texture: %s\n", SDL_GetError());
      return;
    }

    /*
      worst to best scaled font
      SDL_ScaleModeNearest < nearest pixel sampling>
      SDL_ScaleModeLinear < linear filtering>
      SDL_ScaleModeBest < anisotropic filtering>
    */
    SDL_SetTextureScaleMode(row_nr_texture, SDL_ScaleModeBest);

    Texture *tp = calloc(1, sizeof(Texture));
    tp->texture = row_nr_texture;
    tp->token = NULL;
    tp->x = 0;
    tp->y = local_vertical_offset;
    tp->w = row_nr_surface->w;
    tp->h = row_nr_surface->h;
    tp->r = i + 1;
    tp->c = 0;

    local_vertical_offset += row_nr_surface->h;
    textures[i] = tp;

    SDL_FreeSurface(row_nr_surface);
  }

  state->row_nr_textures = textures;
  state->rows_count = rows;
}

// allocs memory
Texture **tokens_to_textures(SDL_Renderer *renderer, int font_size,
                             Token **tokens, int tokens_count,
                             int *textures_count, State *state) {
  Texture **textures = calloc(tokens_count, sizeof(Texture *));

  int local_horizontal_offset = 0;
  int local_vertical_offset = 0;

  int max_horizontal_offset = 0;

  int row = 0;
  int col = 0;

  SDL_Color text_color = color_scheme->fg;

  for (int i = 0; i < tokens_count; i += 1) {

    if (tokens[i]->t == TOKEN_STRING) {
      text_color = color_scheme->strings;
    } else if (tokens[i]->t == TOKEN_NUMBER) {
      text_color = color_scheme->numbers;
    } else if (tokens[i]->t == TOKEN_CODE_KEYWORD) {
      text_color = color_scheme->code_keywords;
    } else if (tokens[i]->t == TOKEN_COMMENT_KEYWORD) {
      text_color = color_scheme->comment_keywords;
    } else if (tokens[i]->t == TOKEN_COMMENT) {
      text_color = color_scheme->comments;
    } else {
      text_color = color_scheme->fg;
    }

    SDL_Surface *text_surface =
        TTF_RenderUTF8_Solid(state->font, tokens[i]->v, text_color);
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

    /*
      worst to best scaled font
      SDL_ScaleModeNearest < nearest pixel sampling>
      SDL_ScaleModeLinear < linear filtering>
      SDL_ScaleModeBest < anisotropic filtering>
    */
    SDL_SetTextureScaleMode(text_texture, SDL_ScaleModeBest);

    Texture *tp = calloc(1, sizeof(Texture));
    tp->texture = text_texture;
    tp->token = tokens[i];
    tp->x = local_horizontal_offset;
    tp->y = local_vertical_offset;
    tp->w = text_surface->w;
    tp->h = text_surface->h;
    tp->r = row;
    tp->c = col;

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
      col = 0;
      row += 1;
    } else {
      local_horizontal_offset += text_surface->w;
      col += tokens[i]->vlen;
    }
  }

  state->max_horizontal_offset = max(max_horizontal_offset, 1);
  state->max_vertical_offset = max(local_vertical_offset, 1);

  // NOTE: snap back if text fits on screen, but horizontal scroll is non-zero
  if (max_horizontal_offset < state->window_width) {
    state->horizontal_scroll = 0;
  }

  row_nrs_to_textures(renderer, font_size, row, state);
  if (row - 1 >= 0) {
    ROW_NUMBER_WIDTH = state->row_nr_textures[row - 1]->w + ROW_NUMBER_PADDING;
    HORIZONTAL_PADDING = HORIZONTAL_PADDING_BASE + ROW_NUMBER_WIDTH;
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

void update_clearing_texture(SDL_Renderer *renderer, State *state) {
  if (state->clearing != NULL) {
    SDL_DestroyTexture(state->clearing);
  }
  state->clearing = SDL_CreateTexture(renderer, 0, 0, state->window_width,
                                      state->window_height);
  SDL_SetTextureColorMod(state->clearing, color_scheme->bg.r,
                         color_scheme->bg.g, color_scheme->bg.b);
}

// update_textures frees existing textures
// and creates new textures from tokens
// frees and allocs memory
Texture **update_textures(Texture **textures, SDL_Renderer *renderer,
                          int font_size, Token **tokens, int tokens_count,
                          int *textures_count, State *state) {
  free_textures(textures, *textures_count);
  free_textures(state->row_nr_textures, state->rows_count);
  *textures_count = 0;
  return tokens_to_textures(renderer, FONT_SIZE, tokens, tokens_count,
                            textures_count, state);
}

int cpy_to_renderer(SDL_Renderer *renderer, Texture **textures,
                    int textures_count, State *state) {

  handle_highlight(renderer, textures, textures_count, state);

  for (int i = 0; i < textures_count; i += 1) {

    int texture_start_width =
        HORIZONTAL_PADDING + textures[i]->x + state->horizontal_scroll;
    int texture_start_height =
        VERTICAL_PADDING + textures[i]->y + state->vertical_scroll;

    // NOTE: only render what fits on window
    // continue if before window
    // break if after window
    if (texture_start_height + textures[i]->h < VERTICAL_PADDING ||
        texture_start_width > state->window_width ||
        texture_start_width + textures[i]->w < HORIZONTAL_PADDING) {
      continue;
    } else if (state->window_height <= texture_start_height) {
      break;
    }

    // NOTE: don't render newline char
    if (textures[i]->token->t == TOKEN_NEWLINE) {
      continue;
    }

    SDL_Rect text_rect = {texture_start_width, texture_start_height,
                          textures[i]->w, textures[i]->h};
    SDL_RenderCopy(renderer, textures[i]->texture, NULL, &text_rect);
  }

  // NOTE: we clear the area where we need to draw scrollbar and row numbers
  if (state->clearing != NULL) {
    SDL_Color prev = {0};
    SDL_GetRenderDrawColor(renderer, (Uint8 *)&prev.r, (Uint8 *)&prev.g,
                           (Uint8 *)&prev.b, (Uint8 *)&prev.a);
    SDL_SetRenderDrawColor(renderer, color_scheme->bg.r, color_scheme->bg.g,
                           color_scheme->bg.b, color_scheme->bg.a);
    SDL_Rect clearing_rect = {0, 0, HORIZONTAL_PADDING, state->window_height};
    SDL_RenderFillRect(renderer, &clearing_rect);
    SDL_SetRenderDrawColor(renderer, prev.r, prev.g, prev.b, prev.a);
  }

  handle_scrollbars(renderer, state);

  for (int i = 0; i < state->rows_count; i += 1) {

    int texture_start_width = state->row_nr_textures[i]->x + ROW_NUMBER_WIDTH -
                              ROW_NUMBER_PADDING / 2 -
                              state->row_nr_textures[i]->w;

    int texture_start_height = VERTICAL_PADDING + state->row_nr_textures[i]->y +
                               state->vertical_scroll;

    // NOTE: only render what fits on window
    // continue if before window
    // break if after window
    if (texture_start_height + state->row_nr_textures[i]->h < 0) {
      continue;
    } else if (state->window_height <= texture_start_height) {
      break;
    }

    SDL_Rect row_nr_rect = {texture_start_width, texture_start_height,
                            state->row_nr_textures[i]->w,
                            state->row_nr_textures[i]->h};
    SDL_RenderCopy(renderer, state->row_nr_textures[i]->texture, NULL,
                   &row_nr_rect);
  }

  // handle search
  handle_actionbox(renderer, state, state->search_mode, SEARCH_BUF);
  handle_actionbox(renderer, state, state->goto_line_mode, GOTO_LINE_BUF);

  return EXIT_SUCCESS;
}

int handle_sdl_events(SDL_Window *window, SDL_Event sdl_event,
                      SDL_Renderer *renderer, Texture **text_textures,
                      int textures_count, State *state) {

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

      // COPY HIGHLIGHTED TEXT START
    } else if (state->ctrl_pressed && !state->shift_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_c) {
      if (state->highlight_moving_coord->x !=
              state->highlight_stationary_coord->x &&
          state->highlight_moving_coord->y !=
              state->highlight_stationary_coord->y) {
        handle_copy_to_clipboard(text_textures, textures_count, state);
      }
      // COPY HIGHLIGHTED TEXT END

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

      reset_font_scale(text_textures, textures_count, state);
      reset_scale_texture_font(state->row_nr_textures, state->rows_count,
                               state);

      FONT_SIZE += FONT_INCREMENT * sign(sdl_event.wheel.y);
      FONT_SIZE = clamp(FONT_SIZE, FONT_LOWER_BOUND, FONT_UPPER_BOUND);
      TTF_SetFontSize(state->font, FONT_SIZE);

      state->is_font_resized = true;
      state->font_scale_factor = (float)FONT_SIZE / (float)BASE_FONT_SIZE;
      state->font_size_unchanged_since = SDL_GetTicks64();
      scale_font(text_textures, textures_count, state);

      scale_texture_font(state->row_nr_textures, state->rows_count, state);
      if (state->rows_count - 1 >= 0) {
        ROW_NUMBER_WIDTH = state->row_nr_textures[state->rows_count - 1]->w +
                           ROW_NUMBER_PADDING;
        HORIZONTAL_PADDING = HORIZONTAL_PADDING_BASE + ROW_NUMBER_WIDTH;
      }

      // FONT RESIZE WITH MOUSEWHEEL END

      // FONT RESIZE +/- START
    } else if (state->ctrl_pressed && !state->shift_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_EQUALS ||
                sdl_event.key.keysym.sym == SDLK_MINUS)) {

      reset_font_scale(text_textures, textures_count, state);
      reset_scale_texture_font(state->row_nr_textures, state->rows_count,
                               state);

      FONT_SIZE += FONT_INCREMENT * (sdl_event.key.keysym.sym == SDLK_EQUALS) -
                   FONT_INCREMENT * (sdl_event.key.keysym.sym == SDLK_MINUS);
      FONT_SIZE = clamp(FONT_SIZE, FONT_LOWER_BOUND, FONT_UPPER_BOUND);
      TTF_SetFontSize(state->font, FONT_SIZE);

      state->is_font_resized = true;
      state->font_scale_factor = (float)FONT_SIZE / (float)BASE_FONT_SIZE;
      state->font_size_unchanged_since = SDL_GetTicks64();
      scale_font(text_textures, textures_count, state);

      scale_texture_font(state->row_nr_textures, state->rows_count, state);
      if (state->rows_count - 1 >= 0) {
        ROW_NUMBER_WIDTH = state->row_nr_textures[state->rows_count - 1]->w +
                           ROW_NUMBER_PADDING;
        HORIZONTAL_PADDING = HORIZONTAL_PADDING_BASE + ROW_NUMBER_WIDTH;
      }
      // FONT RESIZE +/- END

      // FONT RESIZE TO DEFAULT START
    } else if (state->ctrl_pressed && state->shift_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_EQUALS) {

      if (FONT_SIZE != DEFAULT_FONT_SIZE) {
        FONT_SIZE = DEFAULT_FONT_SIZE;
        TTF_SetFontSize(state->font, DEFAULT_FONT_SIZE);
        state->is_font_resized = true;
        // NOTE: + FONT_RENDERING_DELAY because we want to render right away
        state->font_size_unchanged_since =
            SDL_GetTicks64() + FONT_RENDERING_DELAY * SECOND;
        // NOTE: factor to 0 to pass the condition
        state->font_scale_factor = 0;
      }
      // FONT RESIZE TO DEFAULT END

      // WINDOW RESIZE START
    } else if (sdl_event.type == SDL_WINDOWEVENT &&
               sdl_event.window.event == SDL_WINDOWEVENT_RESIZED) {
      (void)SDL_GetWindowSize(window, &state->window_width,
                              &state->window_height);
      update_clearing_texture(renderer, state);
      // WINDOW RESIZE END

      // NEXT COLORSCHEME START
    } else if (state->ctrl_pressed && state->shift_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_COMMA ||
                sdl_event.key.keysym.sym == SDLK_PERIOD)) {
      color_scheme_idx += -1 * (sdl_event.key.keysym.sym == SDLK_COMMA) +
                          (sdl_event.key.keysym.sym == SDLK_PERIOD);
      if (color_scheme_idx == -1) {
        color_scheme_idx = COLOR_SCHEME_COUNT - 1;
      } else if (color_scheme_idx == COLOR_SCHEME_COUNT) {
        color_scheme_idx = 0;
      }
      color_scheme = &color_schemes[color_scheme_idx];
      SDL_SetRenderDrawColor(renderer, color_scheme->bg.r, color_scheme->bg.g,
                             color_scheme->bg.b, color_scheme->bg.a);
      state->color_scheme_modified = true;
      // NEXT COLORSCHEME END

      // ENABLE SEARCH START
    } else if (!state->search_mode && state->ctrl_pressed &&
               sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_f) {
      search_results = free_search_results();
      memset(SEARCH_BUF + 1, 0, SEARCH_BUF_OFFSET - 1);
      SEARCH_BUF_OFFSET = 1;
      state->search_mode = true;
      // ENABLE SEARCH END

      // DISABLE SEARCH START
    } else if (state->search_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_ESCAPE) {
      state->search_mode = false;

      // NOTE: roll search history back to top
      for (; search_history != NULL && search_history->prev != NULL;
           search_history = search_history->prev) {
        ;
      }
      // DISABLE SEARCH END

      // NAVIGATE SEARCH HISTORY START
    } else if (state->search_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               (sdl_event.key.keysym.sym == SDLK_UP ||
                sdl_event.key.keysym.sym == SDLK_DOWN)) {

      SearchHistory *cur = search_history;
      switch (sdl_event.key.keysym.sym) {
      case SDLK_UP:
        if (SEARCH_BUF_OFFSET <= 1) {
          ; // NOTE: search buf empty, insert current search_history
        } else if (search_history != NULL && search_history->next != NULL) {
          search_history = search_history->next;
          cur = search_history;
        }
        break;
      case SDLK_DOWN:
        if (SEARCH_BUF_OFFSET <= 1) {
          cur = NULL; // NOTE: search buf empty, keep it empty
        } else if (search_history != NULL && search_history->prev != NULL) {
          search_history = search_history->prev;
          cur = search_history;
        } else if (search_history != NULL && search_history->prev == NULL) {
          cur = NULL;
        }
        break;
      }
      if (cur == NULL) {
        memset(SEARCH_BUF + 1, 0,
               SEARCH_BUF_OFFSET - 1); // NOTE: -1 to account for '/'
        SEARCH_BUF_OFFSET = 1;         // NOTE: +1 to account for '/'
      } else {
        memcpy(SEARCH_BUF + 1, cur->val, strlen(cur->val));
        SEARCH_BUF_OFFSET = strlen(cur->val) + 1; // NOTE: +1 to account for '/'
        if (SEARCH_BUF_OFFSET < SEARCH_BUF_SIZE) {
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '\0';
        }
      }
      // NAVIGATE SEARCH HISTORY START

      // PASTE TO SEARCH START
    } else if (state->search_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_v &&
               sdl_event.key.keysym.mod & KMOD_CTRL) {
      char *clipboard = SDL_GetClipboardText();
      int clipboard_len = strlen(clipboard);
      if (clipboard_len + SEARCH_BUF_OFFSET < SEARCH_BUF_SIZE) {
        memcpy(SEARCH_BUF + SEARCH_BUF_OFFSET, clipboard, clipboard_len);
        SEARCH_BUF_OFFSET += clipboard_len;
      } else {
        fprintf(stdout,
                "[WARNING]: search text length (%d) exceeding search buffer "
                "size (%d)\n",
                clipboard_len + SEARCH_BUF_OFFSET, SEARCH_BUF_SIZE);
      }
      // PASTE TO SEARCH START

      // SEARCH TYPING START
    } else if (state->search_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym != SDLK_RETURN &&
               SEARCH_BUF_OFFSET + 1 < SEARCH_BUF_SIZE) {
      if (sdl_event.key.keysym.sym == SDLK_BACKSPACE) {
        if (SEARCH_BUF_OFFSET > 1) {
          SEARCH_BUF_OFFSET -= 1;
        }
        SEARCH_BUF[SEARCH_BUF_OFFSET] = '\0';
      }

      if ('a' <= sdl_event.key.keysym.sym && sdl_event.key.keysym.sym <= 'z') {
        char c = sdl_event.key.keysym.sym;
        if (sdl_event.key.keysym.mod & KMOD_SHIFT) {
          c = c - 'a' + 'A';
        }
        SEARCH_BUF[SEARCH_BUF_OFFSET] = c;
        SEARCH_BUF_OFFSET += 1;
      }

      if (('0' <= sdl_event.key.keysym.sym && sdl_event.key.keysym.sym <= '9' ||
           sdl_event.key.keysym.sym == '`' || sdl_event.key.keysym.sym == '-' ||
           sdl_event.key.keysym.sym == '=' || sdl_event.key.keysym.sym == '[' ||
           sdl_event.key.keysym.sym == ']' || sdl_event.key.keysym.sym == ';' ||
           sdl_event.key.keysym.sym == '\'' ||
           sdl_event.key.keysym.sym == '\\' ||
           sdl_event.key.keysym.sym == ',' || sdl_event.key.keysym.sym == '.' ||
           sdl_event.key.keysym.sym == '/' ||
           sdl_event.key.keysym.sym == ' ') &&
          !(sdl_event.key.keysym.mod &
            KMOD_SHIFT)) { // TODO: handle tab and newline properly
        SEARCH_BUF[SEARCH_BUF_OFFSET] = sdl_event.key.keysym.sym;
        SEARCH_BUF_OFFSET += 1;
      }

      if (('0' <= sdl_event.key.keysym.sym && sdl_event.key.keysym.sym <= '9' ||
           sdl_event.key.keysym.sym == '`' || sdl_event.key.keysym.sym == '-' ||
           sdl_event.key.keysym.sym == '=' || sdl_event.key.keysym.sym == '[' ||
           sdl_event.key.keysym.sym == ']' || sdl_event.key.keysym.sym == ';' ||
           sdl_event.key.keysym.sym == '\'' ||
           sdl_event.key.keysym.sym == '\\' ||
           sdl_event.key.keysym.sym == ',' || sdl_event.key.keysym.sym == '.' ||
           sdl_event.key.keysym.sym == '/' ||
           sdl_event.key.keysym.sym == ' ') &&
          sdl_event.key.keysym.mod &
              KMOD_SHIFT) { // TODO: handle tab and newline properly
        switch (sdl_event.key.keysym.sym) {
        case '1':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '!';
          break;
        case '2':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '@';
          break;
        case '3':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '#';
          break;
        case '4':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '$';
          break;
        case '5':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '%';
          break;
        case '6':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '^';
          break;
        case '7':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '&';
          break;
        case '8':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '*';
          break;
        case '9':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '(';
          break;
        case '0':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = ')';
          break;
        case '`':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '~';
          break;
        case '-':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '_';
          break;
        case '=':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '+';
          break;
        case '[':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '{';
          break;
        case ']':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '}';
          break;
        case ';':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = ':';
          break;
        case '\'':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '"';
          break;
        case '\\':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '|';
          break;
        case ',':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '<';
          break;
        case '.':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '>';
          break;
        case '/':
          SEARCH_BUF[SEARCH_BUF_OFFSET] = '?';
          break;
        default:
          SEARCH_BUF[SEARCH_BUF_OFFSET] = sdl_event.key.keysym.sym;
          break;
        }
        SEARCH_BUF_OFFSET += 1;
      }
      // SEARCH TYPING END

      // SEARCH START
    } else if (state->search_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_RETURN &&
               SEARCH_BUF_OFFSET > 1) {

      // NOTE: no search yet
      if (search_results == NULL) {
        handle_search_results(text_textures, textures_count, state);

        // NOTE: find first search result >= to current vertical scroll
        {
          SearchResult *cur = search_results;
          for (; cur != NULL && cur->next != search_results &&
                 cur->start->y < abs(state->vertical_scroll);
               cur = cur->next) {
            ;
          }
          search_results = cur;
        }

        // NOTE: new search
      } else if (search_results != NULL &&
                 strcmp(SEARCH_BUF + 1, search_results->val) != 0) {
        search_results = free_search_results();
        handle_search_results(text_textures, textures_count, state);

        // NOTE: find first search result >= to current vertical scroll
        {
          SearchResult *cur = search_results;
          for (; cur != NULL && cur->next != search_results &&
                 cur->start->y < abs(state->vertical_scroll);
               cur = cur->next) {
            ;
          }
          search_results = cur;
        }

        // NOTE: search didn't change, go to next/prev occurence
      } else if (search_results != NULL &&
                 strcmp(SEARCH_BUF + 1, search_results->val) == 0) {
        if (sdl_event.key.keysym.mod & KMOD_SHIFT) {
          search_results = search_results->prev;
        } else {
          search_results = search_results->next;
        }
      }

      if (search_results != NULL) {
        // NOTE: jump scroll, if search result not in view
        if (search_results->start->y < abs(state->vertical_scroll) ||
            abs(state->vertical_scroll) + state->window_height <
                search_results->start->y) {
          state->vertical_scroll = -search_results->start->y;
        }

        // NOTE: set highlight
        state->highlight_stationary_coord = search_results->start;
        state->highlight_moving_coord = search_results->end;
        state->highlight_stationary_texture_idx =
            search_results->start_texture_idx;
        state->highlight_moving_texture_idx = search_results->end_texture_idx;
      }

      add_to_search_history();

      // SEARCH END

      // JUMP HALF PAGE DOWN START
    } else if (sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_d &&
               sdl_event.key.keysym.mod & KMOD_CTRL) {
      state->vertical_scroll =
          clamp(state->vertical_scroll - state->window_height / 2,
                -state->max_vertical_offset, 0);
      // JUMP HALF PAGE DOWN END

      // JUMP HALF PAGE UP START
    } else if (sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_u &&
               sdl_event.key.keysym.mod & KMOD_CTRL) {

      state->vertical_scroll =
          clamp(state->vertical_scroll + state->window_height / 2,
                -state->max_vertical_offset, 0);
      // JUMP HALF PAGE UP END

      // JUMP TO BEGINNING START
    } else if (sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_a &&
               sdl_event.key.keysym.mod & KMOD_CTRL) {

      state->vertical_scroll = 0;
      // JUMP TO BEGINNING END

      // JUMP TO END START
    } else if (sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_e &&
               sdl_event.key.keysym.mod & KMOD_CTRL) {

      state->vertical_scroll = -state->max_vertical_offset;
      // JUMP TO END END

      // ENABLE GOTO_LINE START
    } else if (!state->goto_line_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_g &&
               (sdl_event.key.keysym.mod & KMOD_CTRL)) {
      memset(GOTO_LINE_BUF + 1, 0, GOTO_LINE_BUF_OFFSET - 1);
      GOTO_LINE_BUF_OFFSET = 1;
      state->goto_line_mode = true;
      // ENABLE GOTO_LINE END

      // DISABLE GOTO_LINE START
    } else if (state->goto_line_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_ESCAPE) {
      memset(GOTO_LINE_BUF + 1, 0, GOTO_LINE_BUF_OFFSET - 1);
      GOTO_LINE_BUF_OFFSET = 1;
      state->goto_line_mode = false;
      // DISABLE GOTO_LINE END

      // GOTO_LINE TYPING START
    } else if (state->goto_line_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym != SDLK_RETURN) {
      if (sdl_event.key.keysym.sym == SDLK_BACKSPACE) {
        if (GOTO_LINE_BUF_OFFSET > 1) {
          GOTO_LINE_BUF_OFFSET -= 1;
        }
        GOTO_LINE_BUF[GOTO_LINE_BUF_OFFSET] = '\0';
      }
      if ('0' <= sdl_event.key.keysym.sym && sdl_event.key.keysym.sym <= '9' &&
          GOTO_LINE_BUF_OFFSET < GOTO_LINE_BUF_SIZE - 1) {
        GOTO_LINE_BUF[GOTO_LINE_BUF_OFFSET] = sdl_event.key.keysym.sym;
        GOTO_LINE_BUF_OFFSET += 1;
      }
      // GOTO_LINE TYPING END

      // GOTO_LINE START
    } else if (state->goto_line_mode && sdl_event.type == SDL_KEYDOWN &&
               sdl_event.key.state == SDL_PRESSED &&
               sdl_event.key.keysym.sym == SDLK_RETURN &&
               GOTO_LINE_BUF_OFFSET > 1) {
      int idx = atoi(GOTO_LINE_BUF + 1) - 1;
      memset(GOTO_LINE_BUF + 1, 0, GOTO_LINE_BUF_OFFSET - 1);
      GOTO_LINE_BUF_OFFSET = 1;
      if (0 <= idx && idx < state->rows_count) {
        state->vertical_scroll = -state->row_nr_textures[idx]->y;
      }
      state->goto_line_mode = false;
      // GOTO_LINE END

      //
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
  SDL_SetRenderDrawColor(renderer, color_scheme->bg.r, color_scheme->bg.g,
                         color_scheme->bg.b, color_scheme->bg.a);

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
  update_clearing_texture(renderer, state);
  state->font = font;

  SEARCH_BUF[SEARCH_BUF_OFFSET] = '/';
  SEARCH_BUF_OFFSET += 1;

  GOTO_LINE_BUF[GOTO_LINE_BUF_OFFSET] = ':';
  GOTO_LINE_BUF_OFFSET += 1;

  int textures_count = 0;
  Texture **text_textures = tokens_to_textures(
      renderer, FONT_SIZE, tokens, tokens_count, &textures_count, state);

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
    handled_event_count = handle_sdl_events(
        window, sdl_event, renderer, text_textures, textures_count, state);
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
          update_textures(text_textures, renderer, FONT_SIZE, tokens,
                          tokens_count, &textures_count, state);

      SDL_RenderClear(renderer);
      err = cpy_to_renderer(renderer, text_textures, textures_count, state);
      if (err != EXIT_SUCCESS) {
        break;
      }

      // NOTE: if we need to refresh tokens (eg font resize)
      // and the font hasn't changed in x seconds
      // recalculate the textures for better quality text
      // NOTE: if we go back to default font size, update textures right away
    } else if (state->color_scheme_modified) {
      state->color_scheme_modified = false;
      text_textures =
          update_textures(text_textures, renderer, FONT_SIZE, tokens,
                          tokens_count, &textures_count, state);

      SDL_RenderClear(renderer);
      err = cpy_to_renderer(renderer, text_textures, textures_count, state);
      if (err != EXIT_SUCCESS) {
        break;
      }
    } else if (state->is_font_resized &&
               (FONT_RENDERING_DELAY * SECOND <
                SDL_GetTicks64() - state->font_size_unchanged_since)) {
      state->is_font_resized = false;

      // NOTE: only recalc textures if font is scaled
      if (state->font_scale_factor != 1.0f) {
        BASE_FONT_SIZE = FONT_SIZE;
        state->font_scale_factor = 1.0f;
        // MAYBE: TODO: make update_textures parallel safe
        text_textures =
            update_textures(text_textures, renderer, FONT_SIZE, tokens,
                            tokens_count, &textures_count, state);

        SDL_RenderClear(renderer);
        err = cpy_to_renderer(renderer, text_textures, textures_count, state);
        if (err != EXIT_SUCCESS) {
          break;
        }
        SDL_RenderPresent(renderer);
      }
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

  TTF_CloseFont(state->font);
  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();

  if (state->clearing != NULL) {
    SDL_DestroyTexture(state->clearing);
  }
  free_textures(state->row_nr_textures, state->rows_count);
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
  search_results = free_search_results();
  search_history = free_search_history();
  return err;
}