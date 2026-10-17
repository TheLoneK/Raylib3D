#pragma once

#include "raylib.h"

//----------------------------------------------------------------------------------
// Small math / random helpers shared across modules
//----------------------------------------------------------------------------------
// Yaw convention: yaw 0 looks down -Z, positive yaw turns left (matches the camera)
namespace MathUtils
{
    float RandomFloat(float min, float max);                            // Uniform random float in [min, max]
    Vector3 RandomConeDirection(Vector3 forward, float spreadDegrees);  // Random direction inside a cone around forward
    Vector3 DirectionFromYaw(float yaw);                                // Flat (XZ) unit direction for a yaw angle
    Vector3 RightFromYaw(float yaw);                                    // Flat unit vector to the right of that direction
    float YawFromDirection(Vector3 direction);                          // Yaw angle for a direction (Y ignored)
    Vector3 FlatDirection(Vector3 from, Vector3 to);                    // Normalized XZ direction from -> to
    float FlatDistance(Vector3 a, Vector3 b);                           // Distance on the XZ plane
    float AngleDifference(float from, float to);                        // Shortest signed angle from -> to, in [-PI, PI]
    float TurnToward(float current, float target, float maxStep);       // Rotate an angle toward target by at most maxStep
}
