
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>
#include <wordexp.h>

#include "high-score-entry.h"

#define DEBUG 1

#define debug(fmt, ...) \
  if (DEBUG) { \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
  }

// Suppress -Wunused-parameter warning from gcc
#define UNUSED(expr) do { (void)(expr); } while (0)

/* Timer object */
/* Added because the SDL timer doesn't support pausing */
typedef struct mytimer {
  SDL_TimerID timerId;
  int startTime;
  int lastPauseTime;
  int duration;
  int timeLeft;
  // As needed by SDL_AddTimer
  SDL_NewTimerCallback callback;
  void* param;
} MyTimer;

MyTimer* mytimer_init(int duration, SDL_NewTimerCallback callback,
                      void* param) {
  MyTimer* mytimer = malloc(sizeof(MyTimer));
  mytimer->duration = duration;
  mytimer->callback = callback;
  mytimer->param = param;
  mytimer->timeLeft = duration;
  mytimer->timerId = SDL_AddTimer(duration, callback, param);
  mytimer->startTime = SDL_GetTicks();
  mytimer->lastPauseTime = SDL_GetTicks();
  return mytimer;
}

void _mytimer_cancel_timer(MyTimer* mytimer) {
  if (mytimer->timerId != NULL) {
    SDL_RemoveTimer(mytimer->timerId);
    mytimer->timerId = NULL;
  }
}

void mytimer_pause(MyTimer* mytimer) {
  int time_elapsed = SDL_GetTicks() - mytimer->lastPauseTime;
  mytimer->timeLeft = mytimer->timeLeft - time_elapsed;
  debug("Halting timer with %d left\n", mytimer->timeLeft);
  _mytimer_cancel_timer(mytimer);
  mytimer->lastPauseTime = SDL_GetTicks();
}

void mytimer_unpause(MyTimer* mytimer) {
  if (mytimer->timerId == NULL) {
    debug("Resuming timer with %d left\n", mytimer->timeLeft);
    mytimer->timerId = SDL_AddTimer(mytimer->timeLeft, mytimer->callback, mytimer->param);
  } else {
    debug("!! Warning: Attempt to unpause timer that is not paused\n");
  }
}

void mytimer_cancel(MyTimer* mytimer) {
  _mytimer_cancel_timer(mytimer);
}

void mytimer_free(MyTimer* mytimer) {
  free(mytimer);
}

/* High Scores */
#define HIGH_SCORE_FILE "~/.snake/scores.txt"
typedef struct score {
  char* name;
  int points;
} score;

typedef struct high_scores {
  score* scores[10];
} high_scores;

char* expand_path(char* path) {
  wordexp_t wordexp_data;
  // Does shell expansion
  wordexp(path, &wordexp_data, 0);
  char* filepath = strdup(wordexp_data.we_wordv[0]);
  printf("Path: %s\n", filepath);
  wordfree(&wordexp_data);
  return filepath;
}

high_scores* high_scores_load() {
  high_scores* scores = malloc(sizeof(high_scores));
  for (int x = 0; x < 10; x++) {
    scores->scores[x] = NULL;
  }

  char* filepath = expand_path(HIGH_SCORE_FILE);
  FILE* fp = fopen(filepath, "r");
  free(filepath);
  if (fp == NULL) {
    printf("Failed to read high scores file\n");
    return scores;
  }
  size_t len = 0;
  char* line = NULL;
  int i = 0;
  while (getline(&line, &len, fp) >= 0) {
    printf("Line: %s", line);
    if (strlen(line) <= 1) {
      break;
    }
    char* name = strdup(strtok(line, ","));
    char* points = strtok(NULL, ",");
    // Replace trailing newline
    points[strlen(points) - 1] = '\0';
    int pts = atoi(points);
    scores->scores[i] = malloc(sizeof(struct score));
    scores->scores[i]->name = name;
    scores->scores[i]->points = pts;
    printf("%s %d\n", name, pts);
    
    i++;
  }

  fclose(fp);
  return scores;
}

/**
 * Returns -1 if score is not in top 10, otherwise returns 0-based index of score
 */
int high_scores_get_score_index(high_scores* scores, int value) {
  for (int i = 0; i < 10; i++) {
    if (scores->scores[i] == NULL) {
      return i;
    }
    if (value >= scores->scores[i]->points) {
      return i;
    }
  }

  return -1;
}

void high_scores_add_score(high_scores* scores, int value, char* name, int index) {
  // Move all entries (>= index) forward one position to make room for new entry
  if (scores->scores[9] != NULL) {
    printf("freeing entry 9\n");
    free(scores->scores[9]->name);
    free(scores->scores[9]);
    scores->scores[9] = NULL;
  }
  for (int i = 8; i >= index; i--) {
    printf("moving score %d\n", i);
    scores->scores[i+1] = scores->scores[i];
  }

  score* new_entry = malloc(sizeof(score));
  new_entry->name = name;
  new_entry->points = value;
  scores->scores[index] = new_entry;
}

void high_scores_save(high_scores* scores) {
  char* path = expand_path(HIGH_SCORE_FILE);
  FILE* fp = fopen(path, "w");
  free(path);
  if (fp == NULL) {
    printf("Error opening high scores file for writing; exiting");
    exit(1);
    return;
  }

  for (int i = 0; i < 10 && scores->scores[i] != NULL; i++) {
    // format is name,score
    fprintf(fp, "%s,%d\n", scores->scores[i]->name, scores->scores[i]->points);
  }

  fclose(fp);
}

void high_scores_free(high_scores* scores) {
  for (int i = 0; i < 10 && scores->scores[i] != NULL; i++) {
    free(scores->scores[i]->name);
    free(scores->scores[i]);
  }
}

/* Quick & Dirty hash implementation */
struct hashnode {
  char* key;
  void* data;
  struct hashnode* next;
};

struct hashnode* hashnode_init() {
  struct hashnode* hashnode = malloc(sizeof(struct hashnode));
  return hashnode;
}

void hashnode_free(struct hashnode* hashnode) {
  struct hashnode* cur = hashnode;
  while (cur != NULL) {
    struct hashnode* next = cur->next;
    free(cur->key);
    free(cur);
    cur = next;
  }
}

bool hashnode_removekey(struct hashnode** hashnode, const char* key) {
  struct hashnode* prev = NULL;
  struct hashnode* cur = *hashnode;
  while (cur != NULL) {
    if (strcmp(key, cur->key) == 0) {
      if (prev == NULL) {
        // This was the head, so set head to the next value
        *hashnode = cur->next;
      } else {
        prev->next = cur->next;
      }
      free(cur->key);
      free(cur);
      return true;
    }
    prev = cur;
    cur = cur->next;
  }

  return false; // Not Found
}

struct hash {
  int hash_size;
  struct hashnode* hash_array[100];
  struct hashnode* keys;
};

struct hash* hash_init() {
  struct hash* hash = malloc(sizeof(struct hash));
  hash->hash_size = 100;
  int i;
  for (i = 0; i < hash->hash_size; i++) {
    hash->hash_array[i] = NULL;
  }
  hash->keys = NULL;
  return hash;
};

unsigned long _hash_func(const char* s) {
  /* djb2 hash function */
  unsigned long hash = 5381;
  int c;
  
  while ((c = *s++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}

struct hashnode* hash_keys(struct hash* hash) {
  return hash->keys;
}

void* hash_at(struct hash* hash, const char* key) {
  struct hashnode* match = hash->hash_array[_hash_func(key) % hash->hash_size];
  while (match != NULL) {
    if (strcmp(match->key, key) == 0) {
      return match->data;
    }
    match = match->next;
  }
  return NULL;
}

void hash_add(struct hash* hash, char* key, void* data) {
  int hash_index =_hash_func(key) % hash->hash_size;
  struct hashnode* new_hashnode = hashnode_init();
  new_hashnode->key = key;
  new_hashnode->data = data;
  if (hash->hash_array[hash_index] == NULL) {
    new_hashnode->next = NULL;
    hash->hash_array[hash_index] = new_hashnode;
  } else {
    // Add to front of existing array
    new_hashnode->next = hash->hash_array[hash_index];
    hash->hash_array[hash_index] = new_hashnode;
  }

  // Add key to front of list of keys
  // We are abusing hashnode for this purpose ...
  struct hashnode* keyhash = hashnode_init();
  keyhash->key = strdup(key);
  keyhash->next = hash->keys;
  hash->keys = keyhash;
  printf("Added %s to keys. %s %s %s\n", keyhash->key, keyhash->key, key, hash->keys->key);
}

bool hash_delete(struct hash* hash, const char* key) {
  int hash_index = _hash_func(key) % hash->hash_size;
  // Remove key from list
  struct hashnode* prev = NULL;
  struct hashnode* cur = hash->hash_array[hash_index];
  while (cur != NULL) {
    if (strcmp(key, cur->key) == 0) {
      if (prev == NULL) {
        // This was the head, so set head to the next value
        hash->hash_array[hash_index] = cur->next;
      } else {
        prev->next = cur->next;
      }
      free(cur->key);
      free(cur);
      // Now, remove key from key list
      hashnode_removekey(&hash->keys, key);
      return true;
    }
    prev = cur;
    cur = cur->next;
  }

  return false; // Not Found
}

void hash_free(struct hash* hash) {
  int i;
  for (i = 0; i < hash->hash_size; i++) {
    hashnode_free(hash->hash_array[i]);
    hash->hash_array[i] = NULL;
  }
  hashnode_free(hash->keys);
  hash->keys = NULL;
}

void hash_reset(struct hash* hash) {
  hash_free(hash);
}


struct missile;
struct berry;

/* Game - Declarations */
#define SNAKE_DEFAULT_DELAY 80
#define SNAKE_WARPED_DELAY 30
#define SNAKE_HYPER_DELAY 30
#define MISSILE_DELAY 80
#define GAME_HYPER_MODE_DURATION_MS 10000
typedef struct game {
  bool running;
  bool gameOver;
  int width;
  int height;
  struct snake* snake;
  struct hash* berries;
  bool timeWarp;
  SDL_TimerID warpTimerId;
  bool hyperMode;
  struct mytimer* hyperTimer;
  int frameDelay;
  // Missiles
  struct missile_list* missiles;
  struct hash* missile_exists;
  struct high_scores* scores;
} Game;
const Game GAME_DEFAULT = {true, false, 50, 50, NULL, NULL, false, NULL, false, NULL, SNAKE_DEFAULT_DELAY, NULL, NULL, NULL};
Game game;

void game_reset(Game* game) {
  game->running = false;
  game->gameOver = false;
  game->timeWarp = false;
  game->warpTimerId = NULL;
  game->hyperMode = false;
  if (game->hyperTimer != NULL) {
    mytimer_cancel(game->hyperTimer);
    mytimer_free(game->hyperTimer);
    game->hyperTimer = NULL;
  }
  game->frameDelay = SNAKE_DEFAULT_DELAY;
  game->hyperTimer = NULL;
  // berries
  // missiles
}

struct point {
  int x;
  int y;
};

typedef struct berry {
  bool hyper;
  bool added_during_hyper;
} Berry;

Berry* berry_init() {
  Berry* berry = malloc(sizeof(Berry));
  berry->hyper = false;
  berry->added_during_hyper = false;
  return berry;
}

void berry_free(Berry* berry) {
  free(berry);
}

typedef struct missile {
  struct point location;
  bool active;
  struct game* game;
  bool dead;
} Missile;
void missile_free(struct missile*);

Berry* game_berry_at(struct game*, int, int);
void game_remove_berry(struct game*, int, int);
void game_add_random_berry(struct game*);
void game_handle_keyevent(struct game*, SDL_KeyboardEvent);
void game_next_state(struct game*);
void game_set_time_warp(struct game*);
void game_enter_hyper_mode(Game* game);
Uint32 game_disable_timewarp_callback(Uint32, void*);
Uint32 game_disable_hypermode_callback(Uint32, void*);
void game_update_missile_stuff(struct game*);

/* Snake structures */
struct direction {
  int dx;
  int dy;
};

/* Snake */
/* Represent the snake as a linked list of points */
typedef struct node {
  struct point point;
  struct node* next;
} Node;

Node* node_create(int x, int y, Node* next) {
  Node* node = malloc(sizeof(struct node));
  node->point.x = x;
  node->point.y = y;
  node->next = next;
  return node;
}

void node_free(Node* node) {
  free(node);
}

/* Missile list */
typedef struct missile_item {
  struct missile_item* prev;
  Missile* item;
  struct missile_item* next;
} MissileItem;

typedef struct missile_list {
  MissileItem* head;
} MissileList;

MissileList* missile_list_init() {
  MissileList* list = malloc(sizeof(struct missile_list));
  list->head = NULL;
  return list;
}

void missile_list_reset(MissileList* list) {
  // free items, reset pointer. Do not free list itself.
  MissileItem* node = list->head;
  while (node != NULL) {
    MissileItem* next = node->next;
    missile_free(node->item);
    free(node);
    node = next;
  }
  list->head = NULL;
}

void missile_list_add(MissileList* list, Missile* missile) {
  // Add to front of list
  MissileItem* item = malloc(sizeof(struct missile_item));
  if (list->head != NULL) {
    list->head->prev = item;
  }
  item->prev = NULL;
  item->item = missile;
  item->next = list->head;
  list->head = item;
}

void missile_list_remove(MissileList* list, MissileItem* itemToRemove) {
  if (itemToRemove->prev == NULL) {
    // This is the head of the list, so reassign the head to next item
    list->head = itemToRemove->next;
    if (list->head != NULL) {
      list->head->prev = NULL;
    }
  } else {
    MissileItem* prev = itemToRemove->prev;
    MissileItem* next = itemToRemove->next;
    if (prev != NULL) {
      prev->next = next;
    }
    if (next != NULL) {
      next->prev = prev;
    }
  }
  missile_free(itemToRemove->item);
  free(itemToRemove);
}

void missile_list_free(MissileList* list) {
  MissileItem* node = list->head;
  while (node != NULL) {
    MissileItem* next = node->next;
    missile_free(node->item);
    free(node);
    node = next;
  }
}

/* Missile */
Missile* missile_init(struct game* game) {
  Missile* missile = malloc(sizeof(struct missile));
  missile->location.x = rand() % game->width;
  missile->location.y = game->height - 1;
  missile->active = true; //false;
  missile->game = game;
  missile->dead = false;

  return missile;
}

void missile_go(Missile* missile) {
  if (missile->location.y > 0) {
    missile->location.y -= 1;
  } else {
    // Dead means the memory can be freed
    missile->dead = true;
  }
}

void missile_free(Missile* missile) {
  free(missile);
}

/* Snake */
typedef struct snake {
  Node *back, *front;
  struct direction direction;
  int num_points;
  bool has_moved;
  unsigned berriesEaten;
} Snake;

Snake* snake_init() {
  Snake* mySnake = malloc(sizeof(struct snake));

  // Set initial direction
  mySnake->direction.dx = 0;
  mySnake->direction.dy = 1;

  // Add initial points
  mySnake->front = node_create(8,6, NULL);
  Node* n = node_create(8,5, mySnake->front);
  Node* n2 = node_create(8,4, n);
  Node* n3 = node_create(8,3, n2);
  Node* n4 = node_create(8,2, n3);
  Node* n5 = node_create(8,1, n4);
  mySnake->back = node_create(8, 0, n5);

  mySnake->berriesEaten = 0;
  mySnake->num_points = 7;
  mySnake->has_moved = true;

  return mySnake;
}

void snake_reset(Snake* snake) {

  // Set initial direction
  snake->direction.dx = 0;
  snake->direction.dy = 1;

  if (snake->back != NULL) {
    Node* node = snake->back;
    while (node != NULL) {
      Node* to_remove = node;
      node = node->next;
      node_free(to_remove);
    }
  }
  // Add initial points
  snake->front = node_create(8,6, NULL);
  Node* n = node_create(8,5, snake->front);
  Node* n2 = node_create(8,4, n);
  Node* n3 = node_create(8,3, n2);
  Node* n4 = node_create(8,2, n3);
  Node* n5 = node_create(8,1, n4);
  snake->back = node_create(8, 0, n5);
  snake->berriesEaten = 0;
  snake->num_points = 7;
  snake->has_moved = true;
}

void snake_print_points(Snake* snake) {
  printf("Direction: %d, %d\n", snake->direction.dx, snake->direction.dy);
  Node* node = snake->back;
  while (node != NULL) {
    printf("Point: %d, %d\n", node->point.x, node->point.y);
    node = node->next;
  }
}

bool snake_change_direction(Snake* snake, int dx, int dy) {
  // Prevent multiple keypresses before snake has actually moved
  if (!snake->has_moved) {
     return false;
  }

  // No direction change. This check also prevents snake from turning back on itself.
  if (snake->direction.dx == dx || snake->direction.dy == dy)
    return false;

  snake->direction.dx = dx;
  snake->direction.dy = dy;
  snake->has_moved = false;
  printf("Snake will change direction: %d, %d\n", dx, dy);
  return true;
}

void snake_grow(Snake* snake) {
  // Figure out direction of tail based on last two points
  int dx = snake->back->point.x - snake->back->next->point.x;
  int dy = snake->back->point.y - snake->back->next->point.y;
  Node* old_tail = snake->back;
  snake->back = node_create(old_tail->point.x + dx, old_tail->point.y + dy, old_tail);
}

bool snake_try_eat_berry(Snake* snake) {
  int y = snake->front->point.y;
  int x = snake->front->point.x;
  Berry* berry = game_berry_at(&game, x, y);
  if (berry != NULL) {
    if (berry->hyper) {
      game_enter_hyper_mode(&game);
    }
    game_remove_berry(&game, x, y);
    snake_grow(snake);
    snake->berriesEaten++;
    return true;
  }

  return false;
}

bool _snake_has_point_at(Snake* snake, int x, int y, bool ignore_front) {
  // ... a hash table would be better. Do search for now.
  Node* node = snake->back;
  while (node != NULL && !(node == snake->front && ignore_front)) {
    if (x == node->point.x && y == node->point.y) {
      return true;
    }
    node = node->next;
  }
  return false;
}

bool snake_has_point_at_ignore_front(Snake* snake, int x, int y) {
  return _snake_has_point_at(snake, x, y, true);
}

bool snake_has_point_at(Snake* snake, int x, int y) {
  return _snake_has_point_at(snake, x, y, false);
}

void snake_go(Snake* snake) {
  // Remove tail, add a new head
  Node* tail = snake->back;
  snake->back = snake->back->next;
  node_free(tail);
  // Add new head in direction snake is moving
  int x = snake->front->point.x + snake->direction.dx;
  int y = snake->front->point.y + snake->direction.dy;
  snake->front->next = node_create(x, y, NULL);
  snake->front = snake->front->next;
  snake->has_moved = true;
}

bool snake_check_dead(Snake* snake) {
  // Does snake go out of bounds?
  if (snake->front->point.x < 0 || snake->front->point.x >= game.width || snake->front->point.y < 0 || snake->front->point.y >= game.height) {
    return true;
  }

  // In hyper-mode, snake can go out of bounds but otherwise cannot die
  if (game.hyperMode) {
    return false;
  }

  // Does snake collide with itself?
  if (snake_has_point_at_ignore_front(snake, snake->front->point.x, snake->front->point.y)) {
    return true;
  }

  // Does snake collide with a missile?
  struct node* node = snake->back;
  while (node != NULL) {
    struct missile_item* missile = game.missiles->head;
    while (missile != NULL) {
      if (missile->item->location.x == node->point.x && missile->item->location.y == node->point.y) {
        return true;
      }

      missile = missile->next;
    }

    node = node->next;
  }

  return false;
}

void snake_free(Snake* snake) {
  Node* node = snake->back;
  while (node != NULL) {
    Node* to_remove = node;
    node = node->next;
    node_free(to_remove);
  }
  free(snake);
}

/* Game structure */
/* Game methods */
void game_pause(Game* game) {
  game->running = !game->running;

  // Manage hypermode timer
  if (game->hyperMode && game->hyperTimer != NULL) {
    if (game->running) {
      mytimer_unpause(game->hyperTimer);
    } else {
      mytimer_pause(game->hyperTimer);
    }
  }
}

void game_handle_keyevent(Game* game, SDL_KeyboardEvent keyevent) {
  switch (keyevent.keysym.sym) {
    case SDLK_DOWN:
      snake_change_direction(game->snake, 0, 1);
      break;
    case SDLK_UP:
      snake_change_direction(game->snake, 0, -1);
      break;
    case SDLK_LEFT:
      snake_change_direction(game->snake, -1, 0);
      break;
    case SDLK_RIGHT:
      snake_change_direction(game->snake, 1, 0);
      break;
    case SDLK_p:
    case SDLK_SPACE:
      game_pause(game);
      break;
    default:
      break;
  }
}

Berry* game_berry_at(Game* game, int x, int y) {
  char str[7];
  sprintf(str, "%d,%d", x, y);
  return (Berry*)hash_at(game->berries, str);
}

void game_add_random_berry(Game* game) {
  int y = rand() % game->height;
  int x = rand() % game->width;
  if (snake_has_point_at(game->snake, x, y)) {
    // Don't add berry on top of snake
    game_add_random_berry(game);
  } else {
    char* str = malloc(8); // 999,999\0
    sprintf(str, "%d,%d", x, y);
    Berry* berry = berry_init();
    // Don't add more hyper berries when already in hyper mode
    if (game->hyperMode == false) {
      berry->hyper = (rand() % 10 == 1);
    }
    // Mark for cleanup if added during hyper
    berry->added_during_hyper = game->hyperMode;
    hash_add(game->berries, str, berry);
    printf("Added berry at %d, %d\n", x, y);
    printf("Berry hyper? %d\n", berry->hyper);
  }
}

void game_cleanup_berries(Game* game) {
  // Remove berries added during hyper mode
  struct hashnode* keys = game->berries->keys;
  while (keys != NULL) {
    const char* key = keys->key;
    int x, y;
    sscanf(key, "%d,%d", &x, &y);
    Berry* berry = (Berry*)hash_at(game->berries, key);
    if (berry->added_during_hyper) {
      game_remove_berry(game, x, y);
    }
    keys = keys->next;
  }
  // If that is all the berries, add one more.
  if (game->berries->keys == NULL) {
    game_add_random_berry(game);
  }
}

void game_remove_berry(Game* game, int x, int y) {
  char str[7];
  sprintf(str, "%d,%d", x, y);
  Berry* berry = (Berry*)hash_at(game->berries, str);
  if (berry != NULL) {
    hash_delete(game->berries, str);
    berry_free(berry);
  }
}

Uint32 game_disable_timewarp_callback(Uint32 interval, void *param) {

  printf("Timer fired\n");
  struct game* game = (struct game*) param;
  game->timeWarp = false;
  game->warpTimerId = NULL;
  return 0; // One-shot timer
}

void game_set_time_warp(Game* game) {
  game->timeWarp = true;
  // Temporary speed-up
  if (game->warpTimerId != NULL) {
    SDL_RemoveTimer(game->warpTimerId);
    game->warpTimerId = NULL;
  }
  game->warpTimerId = SDL_AddTimer(600, game_disable_timewarp_callback, game);
}

void game_enter_hyper_mode(Game* game) {
  printf("Entering hyper mode\n");
  game->hyperMode = true;
  int berries_to_add = 10;
  while (berries_to_add > 0) {
    game_add_random_berry(game);
    berries_to_add--;
  }

  // Temporary speed-up
  if (game->hyperTimer != NULL) {
    mytimer_cancel(game->hyperTimer);
    mytimer_free(game->hyperTimer);
    game->hyperTimer = NULL;
  }
  game->hyperTimer = mytimer_init(GAME_HYPER_MODE_DURATION_MS, game_disable_hypermode_callback, game);
}

Uint32 game_disable_hypermode_callback(Uint32 interval, void *param) {

  printf("Disabling hypermode\n");
  struct game* game = (struct game*) param;
  game->hyperMode = false;
  mytimer_cancel(game->hyperTimer);
  mytimer_free(game->hyperTimer);
  game->hyperTimer = NULL;
  game_cleanup_berries(game);
  return 0; // One-shot timer
}
void game_update_missile_stuff(Game* game) {
  // Randomly add new missiles
  // Update all missile locations (and missile_exists hash)
  struct missile_item* missile = game->missiles->head;

  while (missile != NULL) {
    missile_go(missile->item);

    // Hold reference in case current missile gets freed
    struct missile_item* next_item = missile->next;

    if (missile->item->dead) {
      missile_list_remove(game->missiles, missile);
      //missile_list_add(game->missiles, missile_init(game));
    }

    missile = next_item;
  }
}

bool game_snake_time_ready(Game* game) {
    Uint32 time_now = SDL_GetTicks();
    static Uint32 last_snake_time = 0;
    bool normalReady = time_now - last_snake_time >= game->frameDelay;
    bool warpReady = game->timeWarp && (time_now - last_snake_time >= SNAKE_WARPED_DELAY);
    bool hyperReady = game->hyperMode && (time_now - last_snake_time >= SNAKE_HYPER_DELAY);
    if (normalReady || warpReady || hyperReady) {
      last_snake_time = time_now;
      return true;
    }
    return false;
}

bool game_missile_time_ready(Game* game) {
    Uint32 time_now = SDL_GetTicks();
    static Uint32 last_missile_time = 0;
    bool res = time_now - last_missile_time >= MISSILE_DELAY;
    if (res) {
      last_missile_time = time_now;
    }
    return res;
}

void game_next_state(Game* game) {
  if (game->running && !game->gameOver) {

    if (game_snake_time_ready(game)) {
      snake_go(game->snake);
    }

    if (game_missile_time_ready(game)) {
      game_update_missile_stuff(game);

      // Random chance of adding a new missile
      if (rand() % 200 < 10) {
        missile_list_add(game->missiles, missile_init(game));
      }
    }

    if (snake_check_dead(game->snake)) {
      game->gameOver = true;
      return;
    }
    if (snake_try_eat_berry(game->snake)) {
      printf("Ate berry\n");
      
      game_add_random_berry(game);
      // Set time warp for next 1 second
      game_set_time_warp(game);
      // Decrease delay by 1ms for every 4 berries eaten
      game->frameDelay = SNAKE_DEFAULT_DELAY - (game->snake->berriesEaten / 4);
    }
  }
}

Uint32 timer_event(Uint32 interval, void *param) {
  /* This timer just pushes an event to the event queue, so
   * that we can do our processing in the main event loop.
   */
  SDL_Event event;
  SDL_UserEvent userevent;

  userevent.type = SDL_USEREVENT;
  userevent.code = 0;
  userevent.data1 = NULL;
  userevent.data2 = NULL;

  event.type = SDL_USEREVENT;
  event.user = userevent;

  SDL_PushEvent(&event);
  return 10;
}

typedef enum {
  GAME_RUNNING,
  GAME_PAUSED,
  GAME_GAMEOVER,
  GAME_SCORES,
  GAME_SCORES_DISPLAY
} GameState;

typedef enum {
  GAME_SCORE_ENTRY,
  GAME_SCORE_DISPLAY
} GameScoreState;

GameState game_state;

void high_score_entered_callback(high_score_entry* entry, void* data) {
  Game* game = (Game*)data;
  printf("entered!!! %s\n", entry->name);
  int score = game->snake->berriesEaten * 10;
  int score_index = high_scores_get_score_index(game->scores, score);
  high_scores_add_score(game->scores, score, strdup(entry->name), score_index);
  high_scores_save(game->scores);

  game_state = GAME_SCORES_DISPLAY;
}

#define FONT_PATH "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf"
void high_scores_paint(high_scores* scores, SDL_Surface* screen) {
  TTF_Font* font = TTF_OpenFont(FONT_PATH, 25);

  SDL_Color fg = {255, 0, 0};
  SDL_Color bg = {0, 0, 0};

  // Header
  SDL_Rect loc = {50, 20, 0, 0};
  SDL_Surface* header = TTF_RenderText_Shaded(font, "HIGH SCORES TABLE", fg, bg);
  SDL_BlitSurface(header, NULL, screen, &loc);
  fg = (SDL_Color){255,255,255};

  int offsetY = 40 + header->h;
  for (int i = 0; i < 10 && scores->scores[i] != NULL; i++) {
    char* str = malloc(sizeof(char) * 100);
    sprintf(str, "%4d %3s %d", (i+1), scores->scores[i]->name, scores->scores[i]->points);
    SDL_Surface* text = TTF_RenderText_Shaded(font, str, fg, bg);
    SDL_Rect textLocation = {30, offsetY, 0, 0};
    SDL_BlitSurface(text, NULL, screen, &textLocation);
    offsetY += text->h;
    SDL_FreeSurface(text);
    free(str);
  }

  TTF_CloseFont(font);
}

void screen_draw_score(SDL_Surface* screen, Game game) {
  TTF_Font* font = TTF_OpenFont(FONT_PATH, 16);
  SDL_Color fg = {255, 255, 255};
  SDL_Color bg = {0, 0, 0};
  char* scoreText = malloc(sizeof(char) * 20);
  sprintf(scoreText, "Score: %d", 10 * game.snake->berriesEaten);
  SDL_Surface* text = TTF_RenderText_Shaded(font, scoreText, fg, bg);
  SDL_Rect loc = {game.width * 10 - text->w - 10, game.height * 10 - text->h - 10, 0, 0};
  SDL_BlitSurface(text, NULL, screen, &loc);
  free(scoreText);
  SDL_FreeSurface(text);
  TTF_CloseFont(font);
}

void screen_draw_snake(SDL_Surface* screen, Game game, SDL_Surface* square,
                       SDL_Surface* hyper_square) {
  SDL_Rect dest;
  dest.w = 10;
  dest.h = 10;
  Node* node = game.snake->back;
  while (node != NULL) {
    dest.x = node->point.x * 10;
    dest.y = node->point.y * 10;
    //printf("Blitting %d,%d\n", dest.x, dest.y);
    if (game.hyperMode) {
      SDL_BlitSurface(hyper_square, NULL, screen, &dest);
    } else {
      SDL_BlitSurface(square, NULL, screen, &dest);
    }
    node = node->next;
  }
}

void screen_draw_berries(SDL_Surface* screen, Game game,
                         SDL_Surface* berry_image,
                         SDL_Surface* hyper_image) {
  SDL_Rect dest;
  dest.w = 10;
  dest.h = 10;
  struct hashnode* keys = game.berries->keys;
  while (keys != NULL) {
    const char* key = keys->key;
    int x, y;
    sscanf(key, "%d,%d", &x, &y);
    dest.x = x * 10;
    dest.y = y * 10;
    Berry* berry = (Berry*)hash_at(game.berries, key);
    if (berry->hyper) {
      SDL_BlitSurface(hyper_image, NULL, screen, &dest);
    } else {
      SDL_BlitSurface(berry_image, NULL, screen, &dest);
    }
    keys = keys->next;
  }
}

void screen_draw_missiles(SDL_Surface* screen, Game game) {
  SDL_Rect missileDest;
  missileDest.w = 10;
  missileDest.h = 10;
  struct missile_item* missile = game.missiles->head;
  while (missile != NULL) {
    missileDest.x = missile->item->location.x * 10;
    missileDest.y = missile->item->location.y * 10;
    SDL_FillRect(screen, &missileDest, 0xffffffff);

    missile = missile -> next;
  }
}

int main () {
  debug("Hello\n");
  srand(time(NULL));

  high_scores* scores = high_scores_load();
  high_score_entry* score_entry = high_score_entry_init();

  TTF_Init();
  SDL_Surface* screen;
  SDL_Surface* green_square, *yellow_square;
  SDL_Surface* berry_image = IMG_Load("./berry.png");
  SDL_Surface* star_image = IMG_Load("./star.png");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    printf("Unable to init SDL: %s\n", SDL_GetError());
    return 1;
  }
  atexit(SDL_Quit);
  // -50x50 grid of 10px squares
  screen = SDL_SetVideoMode(10 * 50, 10 * 50, 32, 0);
  if (screen == NULL) {
    printf("Unable to set video mode: %s\n" , SDL_GetError());
    return 1;
  }

  assert(SDL_BYTEORDER == SDL_LIL_ENDIAN);
  green_square = SDL_CreateRGBSurface(SDL_ALPHA_OPAQUE,
      10, /* width */
      10, /* height */
      32, /* color depth */
      0x000000ff, /* rmask */
      0x0000ff00, /* gmask */
      0x00ff0000, /* bmask */
      0xff000000 /* amask */
    );
  yellow_square = SDL_CreateRGBSurface(SDL_ALPHA_OPAQUE,
      10, /* width */
      10, /* height */
      32, /* color depth */
      0x000000ff, /* rmask */
      0x0000ff00, /* gmask */
      0x00ff0000, /* bmask */
      0xff000000 /* amask */
    );

  assert(green_square != NULL);
  assert(yellow_square != NULL);
  //SDL_FillRect(square, NULL, 0x4000FF00);
  //Note: We are doing all these steps to get transparency for the squares
  Uint32 green = SDL_MapRGBA(green_square->format, 0, 255, 0, 200);
  SDL_FillRect(green_square, NULL, green);
  Uint32 yellow = SDL_MapRGBA(yellow_square->format, 255, 255, 0, 200);
  SDL_FillRect(yellow_square, NULL, yellow);

  Snake* mySnake = snake_init();
  snake_print_points(mySnake);

  game_state = GAME_RUNNING;
  game = GAME_DEFAULT;
  game.snake = mySnake;
  game.berries = hash_init();
  game.missiles = missile_list_init();
  game.scores = scores;
  missile_list_add(game.missiles, missile_init(&game));
  missile_list_add(game.missiles, missile_init(&game));
  missile_list_add(game.missiles, missile_init(&game));
  game.missile_exists = hash_init();

  high_score_entry_register_callback(score_entry, &high_score_entered_callback, &game);

  game_add_random_berry(&game);

  SDL_TimerID timerId = SDL_AddTimer(SNAKE_DEFAULT_DELAY, timer_event, &game);
  UNUSED(timerId);

  // Wait for the user to close the window
  bool run = true;
  while (run) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      switch (event.type) {
        case SDL_QUIT:
          run = false;
          break;
        case SDL_KEYDOWN:
          printf("Key event: %d\n", event.key.keysym.sym);
          if (game_state == GAME_RUNNING) {
            game_handle_keyevent(&game, event.key);
          } else if (game_state == GAME_SCORES) {
            high_score_entry_handle_keyevent(score_entry, event.key);
          } else if (game_state == GAME_SCORES_DISPLAY) {
            /* Reset everything */
            game_reset(&game);
            snake_reset(mySnake);
            game_state = GAME_RUNNING;
            hash_reset(game.berries);
            missile_list_reset(game.missiles);
            game_add_random_berry(&game);
            high_score_entry_reset(score_entry);

            game.running = true;
            /* End reset */
          }
          break;
        case SDL_USEREVENT: // Timer event
          game_next_state(&game);
          break;
      }
    }
    
    // Transition to game over state
    if (game_state == GAME_RUNNING && game.gameOver) {
      int score_index;
      int score = 10 * game.snake->berriesEaten;
      if ((score_index = high_scores_get_score_index(game.scores, score)) >= 0) {
        printf("New high score! %d\n", score);
        game_state = GAME_SCORES;
      } else {
        game_state = GAME_SCORES_DISPLAY;
      }
    }
    // repaint
    SDL_FillRect(screen, NULL, 0x00000000);

    if (game_state == GAME_RUNNING) {
      // Draw score text
      screen_draw_score(screen, game);

      // Paint snake
      screen_draw_snake(screen, game, green_square, yellow_square);

      // Paint berries
      screen_draw_berries(screen, game, berry_image, star_image);

      // Paint missile(s)
      screen_draw_missiles(screen, game);

      //printf("Done.\n");
    } else if (game_state == GAME_SCORES) {
      high_score_entry_draw(score_entry, screen);
    } else if (game_state == GAME_SCORES_DISPLAY) {
      high_scores_paint(scores, screen);
    }

    SDL_UpdateRect(screen, 0,0,0,0);

  }

  SDL_FreeSurface(screen);
  SDL_FreeSurface(green_square);
  SDL_FreeSurface(yellow_square);
  SDL_FreeSurface(berry_image);
  snake_free(mySnake);
  missile_list_free(game.missiles);
  hash_free(game.berries);
  hash_free(game.missile_exists);
  high_scores_free(scores);
  high_score_entry_free(score_entry);
  TTF_Quit();
  SDL_Quit();

  return 0;
}


