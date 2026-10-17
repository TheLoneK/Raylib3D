#ifndef CONFIG_H
#define CONFIG_H

//----------------------------------------------------------------------------------
// Global configuration shared by every module
//----------------------------------------------------------------------------------
#define SCREEN_WIDTH        1280
#define SCREEN_HEIGHT        720
#define TARGET_FPS           200
#define WINDOW_TITLE        "Raylib3D - FPS framework"

#define CAMERA_FOV          80.0f

// Largest frame step the simulation will take (prevents tunneling on hitches)
#define MAX_FRAME_TIME      (1.0f/30.0f)

#endif // CONFIG_H
