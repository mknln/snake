
all:
	gcc high-score-entry.c snake.c -Wall --std=gnu99 -g -lSDL -lSDL_image -lSDL_ttf -lSDL_gfx -o snake

