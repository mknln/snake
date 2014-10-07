
#include "high-score-entry.h"
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#define FONT_PATH "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf"

struct high_score_entry {
  char name[3];
  int index;
  int char1, char2, char3;
};

high_score_entry* high_score_entry_init() {
  high_score_entry* entry = malloc(sizeof(struct high_score_entry));
  strcpy(entry->name, "AAA");
  entry->index = 0;
  entry->char1 = 0;
  entry->char2 = 0;
  entry->char3 = 0;
  return entry;
}

void high_score_entry_draw(high_score_entry* entry, SDL_Surface* screen) {
  TTF_Font* font = TTF_OpenFont(FONT_PATH, 50);
  SDL_Color fg = {255, 255, 255};
  SDL_Color bg = {0, 0, 0};
  // Just use this render to get the width for centering
  SDL_Surface* textSurface = TTF_RenderText_Shaded(font, entry->name, fg, bg);

  int offset = (screen->w - textSurface->w)/2;
  int v_center = (screen->h - textSurface->h)/2;
  for (int i = 0; i <= 2; i++) {
    if (i == entry->index) {
      fg = (SDL_Color){0, 255, 0};
    } else {
      fg = (SDL_Color){255, 255, 255};
    }
    char str[] = {entry->name[i], 0};
    SDL_Surface* text = TTF_RenderText_Shaded(font, str, fg, bg);
    SDL_Rect textLocation = {offset, v_center, 0, 0};
    SDL_BlitSurface(text, NULL, screen, &textLocation);
    offset += text->w;
    SDL_FreeSurface(text);
  }

  SDL_FreeSurface(textSurface);
  TTF_CloseFont(font);
}

void high_score_entry_free(high_score_entry* entry) {
  free(entry);
}

void _high_score_entry_next_index(high_score_entry* entry) {
  entry->index = (entry->index + 1) % 3;
}

void _high_score_entry_prev_index(high_score_entry* entry) {
  entry->index = (entry->index - 1) < 0 ? 2 : (entry->index - 1);
}

void _high_score_entry_prev_char(high_score_entry* entry) {
  char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-$@*\0";

  if (entry->index == 0) {
    entry->char1 = (entry->char1 - 1) < 0 ? strlen(alpha) - 1 : (entry->char1 - 1);
    entry->name[entry->index] = alpha[entry->char1];
  } else if (entry->index == 1) {
    entry->char2 = (entry->char2 - 1) < 0 ? strlen(alpha) - 1 : (entry->char2 - 1);
    entry->name[entry->index] = alpha[entry->char2];
  } else if (entry->index == 2) {
    entry->char3 = (entry->char3 - 1) < 0 ? strlen(alpha) - 1 : (entry->char3 - 1);
    entry->name[entry->index] = alpha[entry->char3];
  }
}

void _high_score_entry_next_char(high_score_entry* entry) {
  char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-$@*\0";

  if (entry->index == 0) {
    entry->char1 = (entry->char1 + 1) % strlen(alpha);
    entry->name[entry->index] = alpha[entry->char1];
  } else if (entry->index == 1) {
    entry->char2 = (entry->char2 + 1) % strlen(alpha);
    entry->name[entry->index] = alpha[entry->char2];
  } else if (entry->index == 2) {
    entry->char3 = (entry->char3 + 1) % strlen(alpha);
    entry->name[entry->index] = alpha[entry->char3];
  }
}

void high_score_entry_handle_keyevent(high_score_entry* entry, SDL_KeyboardEvent event) {
  switch (event.keysym.sym) {
    case SDLK_LEFT:
      _high_score_entry_prev_index(entry);
      break;
    case SDLK_RIGHT:
      _high_score_entry_next_index(entry);
      break;
    case SDLK_DOWN:
      _high_score_entry_prev_char(entry);
      break;
    case SDLK_UP:
      _high_score_entry_next_char(entry);
      break;
  }
}
