#include "Physics.h"
#include <algorithm>

void Physics::applyImpulse(RigidBody& rb, Vec2 dir, float magnitude,
                            const PlanetPhysics& planet) {
    float j = magnitude * planet.impulseScale;
    Vec2 dv = dir.normalized() * (j / rb.mass);
    rb.vel += dv;
}

void Physics::integrate(RigidBody& rb, float dt, const PlanetPhysics& planet) {
    constexpr float STOP_THRESHOLD = 2.f;
    rb.vel *= (1.f - planet.friction * dt);
    rb.pos += rb.vel * dt;
    if (rb.vel.length() < STOP_THRESHOLD) {
        rb.vel = {0.f, 0.f};
    }
}

Vec2 Physics::resolveOverlap(const AABB& moving, const AABB& obstacle) {
    float overlapL = (moving.x + moving.w) - obstacle.x;
    float overlapR = (obstacle.x + obstacle.w) - moving.x;
    float overlapT = (moving.y + moving.h) - obstacle.y;
    float overlapB = (obstacle.y + obstacle.h) - moving.y;

    float minX = std::min(overlapL, overlapR);
    float minY = std::min(overlapT, overlapB);

    Vec2 push{};
    if (minX < minY) {
        push.x = (overlapL < overlapR) ? -overlapL : overlapR;
    } else {
        push.y = (overlapT < overlapB) ? -overlapT : overlapB;
    }
    return push;
}
