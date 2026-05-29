#include "Player.h"
#include <cmath>
#include <algorithm>

void Player::startJump(Vec2 dir) {
    jumpTimer    = JUMP_DURATION;
    jumpCooldown = 1.0f;
    // 2 tiles (64px) in jump direction over JUMP_DURATION
    float spd = (dir.length() > 0.f) ? 64.f / JUMP_DURATION : 0.f;
    jumpVel = (dir.length() > 0.f) ? dir.normalized() * spd : Vec2{};
}

void Player::update(float dt, const Uint8* keys, const std::vector<AABB>& walls) {
    if (jumpTimer    > 0.f) jumpTimer    = std::max(0.f, jumpTimer    - dt);
    if (jumpCooldown > 0.f) jumpCooldown = std::max(0.f, jumpCooldown - dt);

    Vec2 dir{};
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    dir.y -= 1.f;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  dir.y += 1.f;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  dir.x -= 1.f;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) dir.x += 1.f;

    float len = dir.length();
    if (len > 0.f) {
        dir = dir.normalized();
        facing = std::atan2(dir.y, dir.x) * 180.f / 3.14159f;
        m_animTimer += dt;
        if (m_animTimer > 0.15f) { m_animTimer = 0.f; m_frame = (m_frame+1) % 4; }
        // On key press: instant velocity (feels responsive on all planets)
        vel = dir * (PLAYER_SPEED * speedMult);
    } else {
        m_animTimer = 0.f; m_frame = 0;
        // On key release: apply floor friction
        // playerFriction=1.0 → near-instant stop; 0.02 → slippery slide
        float f = std::max(0.f, 1.f - playerFriction * 60.f * dt);
        vel *= f;
        if (vel.length() < 1.f) vel = {};
    }

    // External velocity (wind/drift — bypasses friction)
    // During a jump, jumpVel carries horizontal momentum over hazards
    Vec2 totalVel = vel + externalVel + (jumpTimer > 0.f ? jumpVel : Vec2{});

    Vec2 candidate = pos + totalVel * dt;
    resolveWalls(candidate, walls);
    pos = candidate;

    pushDir = (len > 0.f) ? dir : vel.normalized();
    tryPush = (len > 0.f);
}

void Player::resolveWalls(Vec2& candidate, const std::vector<AABB>& walls) {
    AABB box = {candidate.x - PLAYER_HALF, candidate.y - PLAYER_HALF,
                PLAYER_HALF*2.f, PLAYER_HALF*2.f};
    for (const auto& wall : walls) {
        if (!box.intersects(wall)) continue;
        Vec2 push = Physics::resolveOverlap(box, wall);
        candidate += push;
        // Cancel velocity component into wall
        if (push.x != 0.f) vel.x = 0.f;
        if (push.y != 0.f) vel.y = 0.f;
        box = {candidate.x - PLAYER_HALF, candidate.y - PLAYER_HALF,
               PLAYER_HALF*2.f, PLAYER_HALF*2.f};
    }
}

void Player::render(SDL_Renderer* r, float camX, float camY) {
    float groundSY = pos.y - camY;
    float jumpPhase = (jumpTimer > 0.f) ? (1.f - jumpTimer / JUMP_DURATION) : 0.f;
    float jumpY     = (jumpTimer > 0.f) ? -13.f * std::sin(jumpPhase * 3.14159f) : 0.f;

    float sx = pos.x - camX;
    float sy = groundSY + jumpY;

    // Shadow when airborne
    if (jumpTimer > 0.f) {
        float shadowFade = 0.6f + 0.4f * std::sin(jumpPhase * 3.14159f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(70 * shadowFade));
        SDL_FRect shad = {sx - 7.f, groundSY + 5.f, 14.f, 6.f};
        SDL_RenderFillRectF(r, &shad);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    // Body
    SDL_SetRenderDrawColor(r, 255, 220, 160, 255);
    SDL_FRect body = {sx-8.f, sy-10.f, 16.f, 18.f};
    SDL_RenderFillRectF(r, &body);

    // Helmet
    SDL_SetRenderDrawColor(r, 180, 220, 255, 200);
    SDL_FRect helm = {sx-7.f, sy-16.f, 14.f, 12.f};
    SDL_RenderFillRectF(r, &helm);

    SDL_SetRenderDrawColor(r, 100, 160, 240, 255);
    SDL_FRect visor = {sx-4.f, sy-14.f, 8.f, 6.f};
    SDL_RenderFillRectF(r, &visor);

    // Legs
    SDL_SetRenderDrawColor(r, 200, 160, 100, 255);
    float legOff = std::sin(m_animTimer * 20.f) * 3.f;
    SDL_FRect legL = {sx-6.f, sy+7.f, 5.f, 7.f + legOff};
    SDL_FRect legR = {sx+1.f, sy+7.f, 5.f, 7.f - legOff};
    SDL_RenderFillRectF(r, &legL);
    SDL_RenderFillRectF(r, &legR);
}
