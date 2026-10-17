#ifndef EFFECTS_H
#define EFFECTS_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Short lived visual effects (tracers, impacts)
//----------------------------------------------------------------------------------
#define MAX_EFFECTS     256

typedef enum EffectType {
    EFFECT_TRACER = 0,      // Line from start to end
    EFFECT_IMPACT           // Expanding puff at start
} EffectType;

typedef struct Effect {
    bool active;
    EffectType type;
    Vector3 start;
    Vector3 end;
    float size;
    float life;             // Seconds left
    float maxLife;
    Color color;
} Effect;

typedef struct EffectPool {
    Effect items[MAX_EFFECTS];
} EffectPool;

void Effects_Clear(EffectPool *pool);
void Effects_AddTracer(EffectPool *pool, Vector3 start, Vector3 end, Color color);
void Effects_AddImpact(EffectPool *pool, Vector3 position, float size, Color color);
void Effects_Update(EffectPool *pool, float deltaTime);
void Effects_Draw(const EffectPool *pool);

#endif // EFFECTS_H
