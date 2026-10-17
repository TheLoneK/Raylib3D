#this is the compile command to keep the library independant

#bash
gcc main.c -o TestGame.exe -O1 -Wall -std=c99 -Wno-missing-braces -I include/ -L lib/ -lraylib -lopengl32 -lgdi32 -lwinmm