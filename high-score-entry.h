
#ifndef HIGH_SCORE_ENTRY_H
#define HIGH_SCORE_ENTRY_H

#include <SDL/SDL.h>

typedef struct high_score_entry high_score_entry;

high_score_entry* high_score_entry_init();

void high_score_entry_draw(high_score_entry*, SDL_Surface*);
void high_score_entry_free(high_score_entry*);
void high_score_entry_handle_keyevent(high_score_entry*,SDL_KeyboardEvent);

#endif
