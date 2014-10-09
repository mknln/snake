
all:
	gcc high-score-entry.c snake.c --std=gnu99 -g -lSDL -lSDL_ttf -lSDL_gfx -o snake

