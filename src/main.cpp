//------------------------------------------------------------------------------------
// Raylib3D - first person shooter framework
//
// main.cpp only owns the window and the main loop; see README.md for the layout of src/
//------------------------------------------------------------------------------------
#include "raylib.h"
#include "core/Config.h"
#include "game/Game.h"

#include <ctime>

int main()
{
    InitWindow(Config::ScreenWidth, Config::ScreenHeight, Config::WindowTitle);
    SetTargetFPS(Config::TargetFps);
    DisableCursor();        // Limit cursor to relative movement inside the window

    {
        Game game(static_cast<unsigned int>(std::time(nullptr)));

        while (!WindowShouldClose())    // Detect window close button or ESC key
        {
            game.Update(GetFrameTime());
            game.Draw();
        }
    }   // Game releases its GPU meshes here, before the window closes

    CloseWindow();
    return 0;
}
