//------------------------------------------------------------------------------------
// Raylib3D asset editor
//
// Imports models and textures into Assets/ and chooses which ones the game's enemies
// and weapons use (saved to Assets/library.ini, read by the game). See Editor/README.md
//------------------------------------------------------------------------------------
#include "raylib.h"
#include "EditorApp.h"

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1600, 900, "Raylib3D - Asset Editor");
    SetWindowMinSize(1100, 650);
    SetExitKey(KEY_NULL);       // Escape closes dialogs, not the editor
    SetTargetFPS(60);

    {
        EditorApp editor;
        while (!WindowShouldClose()) editor.Frame();
        editor.SaveOnClose();
    }   // Unloads models/textures before the window closes

    CloseWindow();
    return 0;
}
