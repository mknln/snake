
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <SDL/SDL.h>

#define DEBUG 1

#define debug(args) \
  if (DEBUG) { \
    fprintf(stderr, args); \
  }

/* Quick & Dirty hash implementation */
struct hashnode {
  char* key;
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
  
  while (c = *s++) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}

struct hashnode* hash_keys(struct hash* hash) {
  return hash->keys;
}

bool hash_exists(struct hash* hash, const char* key) {
  struct hashnode* match = hash->hash_array[_hash_func(key) % hash->hash_size];
  while (match != NULL) {
    if (strcmp(match->key, key) == 0) {
      return true;
    }
    match = match->next;
  }
  return false;
}

void hash_add(struct hash* hash, char* key) {
  int hash_index =_hash_func(key) % hash->hash_size;
  struct hashnode* new_hashnode = hashnode_init();
  new_hashnode->key = key;
  if (hash->hash_array[hash_index] == NULL) {
    new_hashnode->next = NULL;
    hash->hash_array[hash_index] = new_hashnode;
  } else {
    // Add to front of existing array
    new_hashnode->next = hash->hash_array[hash_index];
    hash->hash_array[hash_index] = new_hashnode;
  }

  // Add key to front of list of keys
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
  }
  hashnode_free(hash->keys);
}

struct missile;

/* Game - Declarations */
#define DEFAULT_DELAY 80
#define WARPED_DELAY 30
typedef struct game {
  bool running;
  bool gameOver;
  int width;
  int height;
  struct snake* snake;
  struct hash* berries;
  bool timeWarp;
  SDL_TimerID warpTimerId;
  int frameDelay;
  // Missiles - Only one currently supported
  struct missile* missile;
  struct hash* missile_exists;
} Game;
const Game GAME_DEFAULT = {true, false, 50, 50, NULL, NULL, false, NULL, DEFAULT_DELAY, NULL, 0};
Game game;

struct point {
  int x;
  int y;
};
typedef struct missile {
  struct point location;
  bool active;
  struct game* game;
  bool dead;
} Missile;

bool game_has_berry_at(struct game*, int, int);
void game_remove_berry(struct game*, int, int);
void game_add_random_berry(struct game*);
void game_handle_keyevent(struct game*, SDL_KeyboardEvent);
void game_next_state(struct game*);
void game_set_timewarp(struct game*);
Uint32 game_disable_timewarp_callback(Uint32, void*);
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

  mySnake->num_points = 7;
  mySnake->has_moved = true;

  return mySnake;
}

void snake_print_points(Snake* snake) {
  printf("Direction: %d, %d\n", snake->direction.dx, snake->direction.dy);
  int i;
  Node* node = snake->back;
  while (node != NULL) {
    printf("Point: %d, %d\n", node->point.x, node->point.y);
    node = node->next;
  }
}

bool snake_change_direction(Snake* snake, int dx, int dy) {
  // Prevent multiple keypresses before snake has actually moved
  if (!snake->has_moved) {
     return;
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
  if (game_has_berry_at(&game, x, y)) {
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
  // Does snake collide with itself?
  if (snake_has_point_at_ignore_front(snake, snake->front->point.x, snake->front->point.y)) {
    return true;
  }

  // Does snake collide with a missile?
  struct node* node = snake->back;
  while (node != NULL) {
    if (game.missile->location.x == node->point.x && game.missile->location.y == node->point.y) {
      return true;
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

/* Game structure */
/* Game methods */
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
  }
}

bool game_has_berry_at(Game* game, int x, int y) {
  char str[7];
  sprintf(str, "%d,%d", x, y);
  return hash_exists(game->berries, str);
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
    hash_add(game->berries, str);
    printf("Added berry at %d, %d\n", x, y);
  }
}

void game_remove_berry(Game* game, int x, int y) {
  char str[7];
  sprintf(str, "%d,%d", x, y);
  hash_delete(game->berries, str);
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

void game_update_missile_stuff(Game* game) {
  // Randomly add new missiles
  // Update all missile locations (and missile_exists hash)
  // TODO support >1 missile
  missile_go(game->missile);
  if (game->missile->dead) {
    missile_free(game->missile);
    game->missile = missile_init(game);
  }
}

void game_next_state(Game* game) {
  if (game->running && !game->gameOver) {
    snake_go(game->snake);
    game_update_missile_stuff(game);
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
      game->frameDelay = DEFAULT_DELAY - (game->snake->berriesEaten / 4);
    }
  } else {
    printf("Game Over\n");
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
  return game.timeWarp ? WARPED_DELAY : game.frameDelay;
}

int main () {
  debug("Hello\n");
  srand(time(NULL));

  SDL_Surface* screen;
  SDL_Surface* square;
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
  square = SDL_CreateRGBSurface(SDL_ALPHA_OPAQUE,
      10, /* width */
      10, /* height */
      32, /* color depth */
      0x000000ff, /* rmask */
      0x0000ff00, /* gmask */
      0x00ff0000, /* bmask */
      0xff000000 /* amask */
    );

  if (square == NULL) {
    printf("Issue creating square RGB surface: %s\n", SDL_GetError());
    return 1;
  }
  //SDL_FillRect(square, NULL, 0x4000FF00);
  Uint32 color = SDL_MapRGBA(square->format, 0, 255, 0, 200);
  SDL_FillRect(square, NULL, color);

  Snake* mySnake = snake_init();
  snake_print_points(mySnake);

  game = GAME_DEFAULT;
  game.snake = mySnake;
  game.berries = hash_init();
  game.missile = missile_init(&game);
  game.missile_exists = hash_init();

  game_add_random_berry(&game);

  SDL_TimerID timerId = SDL_AddTimer(DEFAULT_DELAY, timer_event, &game);

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
          printf("Key event: %d %d\n", event.key.keysym.sym);
          game_handle_keyevent(&game, event.key);
          break;
        case SDL_USEREVENT:
          game_next_state(&game);
          break;
      }
    }
    // repaint
    SDL_FillRect(screen, NULL, 0x00000000);
    // Paint snake
    SDL_Rect dest;
    dest.w = 10;
    dest.h = 10;
    Node* node = mySnake->back;
    while (node != NULL) {
      dest.x = node->point.x * 10;
      dest.y = node->point.y * 10;
      //printf("Blitting %d,%d\n", dest.x, dest.y);
      SDL_BlitSurface(square, NULL, screen, &dest);
      node = node->next;
    }
    // Paint berries
    dest.w = 10;
    dest.h = 10;
    struct hashnode* berries = game.berries->keys;
    while (berries != NULL) {
      const char* key = berries->key;
      int x, y;
      sscanf(key, "%d,%d", &x, &y);
      dest.x = x * 10;
      dest.y = y * 10;
      SDL_FillRect(screen, &dest, 0xff0000ff);
      berries = berries->next;
    }

    // Paint missile(s)
    SDL_Rect missileDest;
    missileDest.w = 10;
    missileDest.h = 10;
    missileDest.x = game.missile->location.x * 10;
    missileDest.y = game.missile->location.y * 10;
    SDL_FillRect(screen, &missileDest, 0xffffffff);
    //printf("Done.\n");

    SDL_UpdateRect(screen, 0,0,0,0);

  }

  SDL_FreeSurface(screen);
  SDL_FreeSurface(square);
  snake_free(mySnake);
  missile_free(game.missile);
  hash_free(game.berries);
  hash_free(game.missile_exists);
  SDL_Quit();

  return 0;
}


