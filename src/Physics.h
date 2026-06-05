#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <cmath>

struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(float s)        const { return {x*s, y*s}; }
    Vec2& operator+=(const Vec2& o) { x+=o.x; y+=o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x-=o.x; y-=o.y; return *this; }
    Vec2& operator*=(float s)       { x*=s;   y*=s;   return *this; }
    float length() const { return std::sqrt(x*x + y*y); }
    Vec2 normalized() const {
        float l = length();
        return l > 0.f ? Vec2{x/l, y/l} : Vec2{};
    }
};

struct AABB {
    float x, y, w, h;
    bool intersects(const AABB& o) const {
        return x < o.x+o.w && x+w > o.x &&
               y < o.y+o.h && y+h > o.y;
    }
    bool contains(float px, float py) const {
        return px >= x && px <= x+w && py >= y && py <= y+h;
    }
    SDL_FRect toFRect() const { return {x, y, w, h}; }
};

enum class PlanetGimmick {
    None,        // Mars: no special mechanic
    AutoRock,    // Mercury: rocks self-move (heat convection)
    HazeVision,  // Venus: visual noise overlay
    Emotional,   // Earth: doubled popup messages
    HeavyGrav,   // Jupiter: rock mass x3, player speed x0.7
    Slippery,    // Saturn: floor playerFriction = 0.02
    SideDrift,   // Uranus: constant horizontal drift force
    WindStorm,   // Neptune: periodic wind burst
};

struct PlanetPhysics {
    const char*   name           = "Earth";
    float         gravity        = 9.8f;
    float         friction       = 0.85f;   // rock deceleration coefficient
    float         impulseScale   = 1.0f;
    float         playerFriction = 1.0f;    // 1.0 = instant stop, 0.02 = icy
    float         playerSpeed    = 1.0f;    // multiplier on PLAYER_SPEED
    PlanetGimmick gimmick        = PlanetGimmick::None;
    SDL_Color     skyColor       = {15, 12, 25, 255};
    SDL_Color     ambientColor   = {255, 255, 255, 255};
};

namespace Planets {
    // Index 0-7: Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune
    static const PlanetPhysics ALL[8] = {
        // name      grav   fric   imp    pFric  pSpd   gimmick
        { "Mercury",  3.70f, 0.90f, 0.80f, 1.0f, 1.0f, PlanetGimmick::AutoRock,
          {30, 25, 20, 255}, {160, 140, 120, 255} },
        { "Venus",    8.87f, 0.70f, 1.00f, 1.0f, 1.0f, PlanetGimmick::HazeVision,
          {50, 35, 10, 255}, {200, 160,  60, 255} },
        { "Earth",    9.80f, 0.85f, 1.00f, 1.0f, 1.0f, PlanetGimmick::Emotional,
          {10, 20, 40, 255}, { 80, 140,  80, 255} },
        { "Mars",     3.72f, 0.80f, 0.85f, 1.0f, 1.0f, PlanetGimmick::None,
          {35, 15, 10, 255}, {180,  80,  50, 255} },
        { "Jupiter", 24.80f, 0.50f, 2.50f, 1.0f, 0.7f, PlanetGimmick::HeavyGrav,
          {40, 25, 15, 255}, {200, 130,  70, 255} },
        { "Saturn",  10.40f, 0.10f, 1.10f, 0.02f,1.0f, PlanetGimmick::Slippery,
          {0,  0,  0,  255}, {210, 190, 120, 255} },
        { "Uranus",   8.87f, 0.30f, 0.95f, 1.0f, 1.0f, PlanetGimmick::SideDrift,
          {10, 30, 40, 255}, { 80, 200, 200, 255} },
        { "Neptune", 11.20f, 0.25f, 1.20f, 1.0f, 1.0f, PlanetGimmick::WindStorm,
          {10, 15, 50, 255}, { 50,  80, 220, 255} },
    };

    // Visit order: Mars first (story), then inner → outer
    static const int VISIT_ORDER[8] = {3, 0, 1, 2, 4, 5, 6, 7};
    static const int COUNT           = 8;
    static const int PARTS_PER_PLANET = 2;
    static const int TOTAL_PARTS     = COUNT * PARTS_PER_PLANET; // 16

    // Defined in UI.cpp (avoids multiple-definition warnings)
    extern const char* STORY[8];
    extern const char* GIMMICK_DESC[8];
    extern const char* BASE_HINTS[8];
}

// Rigid body (pushable rock)
struct RigidBody {
    Vec2  pos;
    Vec2  vel;
    float mass   = 5.f;
    float radius = 12.f;
    bool  active = true;

    AABB getAABB() const {
        return {pos.x - radius, pos.y - radius, radius*2.f, radius*2.f};
    }
};

class Physics {
public:
    static void applyImpulse(RigidBody& rb, Vec2 dir, float magnitude,
                             const PlanetPhysics& planet);
    static void integrate(RigidBody& rb, float dt, const PlanetPhysics& planet);
    static Vec2 resolveOverlap(const AABB& moving, const AABB& obstacle);
};
