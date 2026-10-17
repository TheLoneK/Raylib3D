#include "utils.h"
#include "raymath.h"

float Util_RandomFloat(float min, float max)
{
    float t = (float)GetRandomValue(0, 10000)/10000.0f;
    return min + (max - min)*t;
}

Vector3 Util_RandomConeDirection(Vector3 forward, float spreadDegrees)
{
    forward = Vector3Normalize(forward);
    if (spreadDegrees <= 0.0f) return forward;

    // Build a basis around the forward vector
    Vector3 worldUp = (fabsf(forward.y) > 0.99f) ? (Vector3){ 1.0f, 0.0f, 0.0f } : (Vector3){ 0.0f, 1.0f, 0.0f };
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, worldUp));
    Vector3 up = Vector3CrossProduct(right, forward);

    // sqrt() keeps the distribution uniform over the cone's disc
    float angle = spreadDegrees*DEG2RAD*sqrtf(Util_RandomFloat(0.0f, 1.0f));
    float theta = Util_RandomFloat(0.0f, 2.0f*PI);
    float offset = tanf(angle);

    Vector3 direction = Vector3Add(forward, Vector3Add(Vector3Scale(right, cosf(theta)*offset), Vector3Scale(up, sinf(theta)*offset)));
    return Vector3Normalize(direction);
}

Vector3 Util_DirectionFromYaw(float yaw)
{
    return (Vector3){ -sinf(yaw), 0.0f, -cosf(yaw) };
}

float Util_YawFromDirection(Vector3 direction)
{
    return atan2f(-direction.x, -direction.z);
}

Vector3 Util_FlatDirection(Vector3 from, Vector3 to)
{
    Vector3 delta = (Vector3){ to.x - from.x, 0.0f, to.z - from.z };
    float length = Vector3Length(delta);
    if (length < 0.0001f) return (Vector3){ 0 };
    return Vector3Scale(delta, 1.0f/length);
}

float Util_FlatDistance(Vector3 a, Vector3 b)
{
    return Vector2Distance((Vector2){ a.x, a.z }, (Vector2){ b.x, b.z });
}

float Util_AngleDifference(float from, float to)
{
    float diff = fmodf(to - from + PI, 2.0f*PI);
    if (diff < 0.0f) diff += 2.0f*PI;
    return diff - PI;
}

float Util_TurnToward(float current, float target, float maxStep)
{
    float diff = Util_AngleDifference(current, target);
    if (fabsf(diff) <= maxStep) return target;
    return current + ((diff > 0.0f) ? maxStep : -maxStep);
}
