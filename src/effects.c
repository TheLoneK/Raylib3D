#include "effects.h"
#include "raymath.h"

void Effects_Clear(EffectPool *pool)
{
    *pool = (EffectPool){ 0 };
}

static Effect *AddEffect(EffectPool *pool)
{
    // Reuse the oldest (closest to expiring) effect when full
    Effect *best = &pool->items[0];
    for (int i = 0; i < MAX_EFFECTS; i++)
    {
        if (!pool->items[i].active) return &pool->items[i];
        if (pool->items[i].life < best->life) best = &pool->items[i];
    }
    return best;
}

void Effects_AddTracer(EffectPool *pool, Vector3 start, Vector3 end, Color color)
{
    *AddEffect(pool) = (Effect){
        .active = true, .type = EFFECT_TRACER,
        .start = start, .end = end,
        .life = 0.06f, .maxLife = 0.06f, .color = color,
    };
}

void Effects_AddImpact(EffectPool *pool, Vector3 position, float size, Color color)
{
    *AddEffect(pool) = (Effect){
        .active = true, .type = EFFECT_IMPACT,
        .start = position, .end = position, .size = size,
        .life = 0.25f, .maxLife = 0.25f, .color = color,
    };
}

void Effects_Update(EffectPool *pool, float deltaTime)
{
    for (int i = 0; i < MAX_EFFECTS; i++)
    {
        Effect *effect = &pool->items[i];
        if (!effect->active) continue;

        effect->life -= deltaTime;
        if (effect->life <= 0.0f) effect->active = false;
    }
}

void Effects_Draw(const EffectPool *pool)
{
    for (int i = 0; i < MAX_EFFECTS; i++)
    {
        const Effect *effect = &pool->items[i];
        if (!effect->active) continue;

        float t = effect->life/effect->maxLife;     // 1 -> 0

        switch (effect->type)
        {
            case EFFECT_TRACER:
                DrawLine3D(effect->start, effect->end, Fade(effect->color, t));
                break;
            case EFFECT_IMPACT:
                DrawSphereEx(effect->start, effect->size*(1.0f + (1.0f - t)*1.5f), 6, 6, Fade(effect->color, t));
                break;
            default: break;
        }
    }
}
