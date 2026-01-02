#pragma once

#include <SDL3/SDL.h>
#include <cmath>
#include <string>
#include <algorithm>

namespace ScrapHeap {

// Math constants
constexpr float PI = 3.14159265358979f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

// Physics constants
constexpr float FRICTION = 0.92f;
constexpr float ANGULAR_FRICTION = 0.88f;
constexpr float COLLISION_ELASTICITY = 0.6f;
constexpr float BASE_HEALTH = 100.0f;

// Game constants
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr int MAX_PLAYERS = 4;
constexpr float MATCH_DURATION = 180.0f; // 3 minutes
constexpr int MAX_TAG_LENGTH = 12;

// Utility functions
inline float clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

inline float normalizeAngle(float angle) {
    while (angle > PI) angle -= 2 * PI;
    while (angle < -PI) angle += 2 * PI;
    return angle;
}

inline float angleDifference(float a, float b) {
    return normalizeAngle(b - a);
}

// Vector operations
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
    Vec2 operator/(float scalar) const { return {x / scalar, y / scalar}; }
    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }

    float length() const { return std::sqrt(x * x + y * y); }
    float lengthSquared() const { return x * x + y * y; }

    Vec2 normalized() const {
        float len = length();
        if (len > 0.0001f) return *this / len;
        return {0, 0};
    }

    float dot(const Vec2& other) const { return x * other.x + y * other.y; }

    static Vec2 fromAngle(float angle) {
        return {std::cos(angle), std::sin(angle)};
    }
};

// String utilities
std::string toUpperCase(const std::string& str);
std::string trim(const std::string& str);

} // namespace ScrapHeap
