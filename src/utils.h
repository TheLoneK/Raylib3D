#ifndef UTILS_H
#define UTILS_H

#include "raylib.h"

//----------------------------------------------------------------------------------
// Small math / random helpers shared across modules
//----------------------------------------------------------------------------------
// Yaw convention: yaw 0 looks down -Z, positive yaw turns left (matches the camera)

float Util_RandomFloat(float min, float max);                          // Uniform random float in [min, max]
Vector3 Util_RandomConeDirection(Vector3 forward, float spreadDegrees); // Random direction inside a cone around forward
Vector3 Util_DirectionFromYaw(float yaw);                               // Flat (XZ) unit direction for a yaw angle
float Util_YawFromDirection(Vector3 direction);                         // Yaw angle for a direction (Y ignored)
Vector3 Util_FlatDirection(Vector3 from, Vector3 to);                   // Normalized XZ direction from -> to
float Util_FlatDistance(Vector3 a, Vector3 b);                          // Distance on the XZ plane
float Util_AngleDifference(float from, float to);                       // Shortest signed angle from -> to, in [-PI, PI]
float Util_TurnToward(float current, float target, float maxStep);      // Rotate an angle toward target by at most maxStep

#endif // UTILS_H
