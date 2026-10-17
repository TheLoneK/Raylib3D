# Optional Makefile (mingw32-make on Windows, make on Linux/macOS)
# Expects raylib headers in include/ and the raylib library in lib/

SOURCES = main.c $(wildcard src/*.c)
CFLAGS  = -O1 -Wall -std=c99 -Wno-missing-braces -I include/ -I src/

ifeq ($(OS),Windows_NT)
    TARGET = TestGame.exe
    LIBS   = -L lib/ -lraylib -lopengl32 -lgdi32 -lwinmm
else ifeq ($(shell uname),Darwin)
    TARGET = TestGame
    LIBS   = -L lib/ -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else
    TARGET = TestGame
    LIBS   = -L lib/ -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

$(TARGET): $(SOURCES) $(wildcard src/*.h)
	gcc $(SOURCES) -o $(TARGET) $(CFLAGS) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f TestGame TestGame.exe

.PHONY: run clean
