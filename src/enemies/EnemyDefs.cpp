#include "enemies/EnemyDefs.h"
#include <algorithm>
#include <array>

// Fields must stay in declaration order
static const std::array<EnemyDef, static_cast<size_t>(EnemyType::Count)> s_enemyDefs = {{
    // EnemyType::Brute
    EnemyDef{
        .name = "Brute", .attackKind = AttackKind::Melee,
        .maxHealth = 120.0f, .radius = 0.55f, .height = 2.0f,
        .wanderSpeed = 2.5f, .chaseSpeed = 8.0f, .turnSpeed = 8.0f,
        .sightRange = 45.0f, .sightFovDegrees = 140.0f, .memoryTime = 5.0f,
        .attackRange = 2.0f, .preferredRange = 0.0f,
        .attackDamage = 20.0f, .attackWindup = 0.35f, .attackCooldown = 0.9f,
        .headHeightFraction = 0.72f,
        .color = Color{ 185, 65, 55, 255 },
    },
    // EnemyType::Gunner
    EnemyDef{
        .name = "Gunner", .attackKind = AttackKind::Projectile,
        .maxHealth = 80.0f, .radius = 0.45f, .height = 1.8f,
        .wanderSpeed = 2.0f, .chaseSpeed = 4.5f, .turnSpeed = 6.0f,
        .sightRange = 60.0f, .sightFovDegrees = 120.0f, .memoryTime = 6.0f,
        .attackRange = 35.0f, .preferredRange = 14.0f,
        .attackDamage = 10.0f, .attackWindup = 0.5f, .attackCooldown = 1.3f,
        .projectileSpeed = 28.0f, .projectileRadius = 0.2f, .aimSpreadDegrees = 2.5f,
        .headHeightFraction = 0.72f,
        .color = Color{ 60, 120, 200, 255 },
    },
    // EnemyType::SkullLord (attackCooldown = time between bolt volleys; it is all skull, so no headshots)
    EnemyDef{
        .name = "Skull Lord", .attackKind = AttackKind::Projectile,
        .maxHealth = 3000.0f, .radius = 1.3f, .height = 2.4f,
        .wanderSpeed = 2.0f, .chaseSpeed = 4.0f, .turnSpeed = 3.0f,
        .sightRange = 50.0f, .sightFovDegrees = 360.0f, .memoryTime = 999.0f,
        .attackRange = 45.0f, .preferredRange = 14.0f,
        .attackDamage = 12.0f, .attackWindup = 0.5f, .attackCooldown = 2.0f,
        .projectileSpeed = 22.0f, .projectileRadius = 0.35f, .aimSpreadDegrees = 2.0f,
        .color = Color{ 225, 215, 190, 255 },
    },
    // EnemyType::Lich (abilities tuned in bosses/Lich.cpp)
    EnemyDef{
        .name = "Lich", .attackKind = AttackKind::Projectile,
        .maxHealth = 4000.0f, .radius = 0.9f, .height = 3.2f,
        .wanderSpeed = 2.0f, .chaseSpeed = 3.0f, .turnSpeed = 2.5f,
        .sightRange = 50.0f, .sightFovDegrees = 360.0f, .memoryTime = 999.0f,
        .attackRange = 45.0f, .preferredRange = 16.0f,
        .headHeightFraction = 0.78f,
        .color = Color{ 70, 40, 95, 255 },
    },
    // EnemyType::LichCrystal
    EnemyDef{
        .name = "Soul Crystal", .attackKind = AttackKind::Melee,
        .maxHealth = 250.0f, .radius = 0.6f, .height = 2.2f,
        .wanderSpeed = 0.0f, .chaseSpeed = 0.0f, .turnSpeed = 0.0f,
        .dropsLoot = false,
        .color = Color{ 90, 230, 255, 255 },
    },
}};

const EnemyDef &GetEnemyDef(EnemyType type)
{
    size_t index = static_cast<size_t>(type);
    if (index >= s_enemyDefs.size()) index = 0;
    return s_enemyDefs[index];
}

float GetLargestEnemyRadius()
{
    float largest = 0.0f;
    for (const EnemyDef &def : s_enemyDefs) largest = std::max(largest, def.radius);
    return largest;
}
