#pragma once
#include "Physics.h"
#include <SDL2/SDL.h>

constexpr float PLAYER_SPEED  = 120.f;
constexpr float PLAYER_HALF   = 10.f;
constexpr float JUMP_DURATION = 0.28f;

class Player {
public:
    Vec2  pos;
    Vec2  vel;
    float facing = 0.f;

    bool  tryPush = false;
    Vec2  pushDir;

    // Gimmick modifiers — set by Game per-planet
    float speedMult     = 1.0f;
    float playerFriction= 1.0f;
    Vec2  externalVel   = {};

    // Jump state
    float jumpTimer    = 0.f;   // counts down from JUMP_DURATION; >0 = airborne
    float jumpCooldown = 0.f;   // 1s cooldown between jumps
    bool  isAirborne() const { return jumpTimer > 0.f; }
    void  startJump();

    void update(float dt, const Uint8* keys, const std::vector<AABB>& walls);
    void render(SDL_Renderer* r, float camX, float camY);

    AABB getAABB() const {
        return {pos.x - PLAYER_HALF, pos.y - PLAYER_HALF,
                PLAYER_HALF*2.f, PLAYER_HALF*2.f};
    }

private:
    float m_animTimer = 0.f;
    int   m_frame     = 0;
    void  resolveWalls(Vec2& candidate, const std::vector<AABB>& walls);
};
