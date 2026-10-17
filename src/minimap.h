#ifndef MINIMAP_H
#define MINIMAP_H

#include "raylib.h"
#include "level.h"
#include "player.h"
#include "enemy.h"

//----------------------------------------------------------------------------------
// Minimap: compact player-centered view in the corner, or an expanded full map
//----------------------------------------------------------------------------------
typedef struct MinimapSettings {
    float compactSize;          // Pixels (square)
    float compactWorldRadius;   // World units visible from the center to the edge
    float margin;               // Pixels from the screen edge
    bool showEnemies;           // Later this can depend on line of sight / radar items
    Color backgroundColor;
    Color floorColor;
    Color wallColor;
    Color borderColor;
    Color playerColor;
} MinimapSettings;

typedef struct Minimap {
    MinimapSettings settings;
    bool expanded;
} Minimap;

MinimapSettings Minimap_DefaultSettings(void);
void Minimap_Init(Minimap *minimap, MinimapSettings settings);
void Minimap_ToggleExpanded(Minimap *minimap);
void Minimap_Draw(const Minimap *minimap, const Level *level, const Player *player, const EnemyList *enemies);

#endif // MINIMAP_H
