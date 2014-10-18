
#ifndef HIGH_SCORE_ENTRY_H
#define HIGH_SCORE_ENTRY_H

#include <SDL/SDL.h>

typedef struct high_score_entry {
  char name[4];
  int index;
  int char1, char2, char3;
  void (*finished_callback)(struct high_score_entry*,void*);
  void* callback_data;
} high_score_entry;

high_score_entry* high_score_entry_init();

void high_score_entry_draw(high_score_entry*, SDL_Surface*);
void high_score_entry_free(high_score_entry*);
void high_score_entry_reset(high_score_entry*);
void high_score_entry_handle_keyevent(high_score_entry*,SDL_KeyboardEvent);
void high_score_entry_register_callback(high_score_entry*, void(*func)(high_score_entry*,void*), void* data);

#endif
