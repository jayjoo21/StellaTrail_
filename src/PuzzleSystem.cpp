#include "PuzzleSystem.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

void PuzzleSystem::update(float dt) {
    // Apply external force to rocks (wind / drift)
    if (rockExternalForce.x != 0.f || rockExternalForce.y != 0.f) {
        for (auto& rock : rocks) {
            if (!rock.active) continue;
            rock.vel += rockExternalForce * dt;
        }
    }

    for (auto& rock : rocks) {
        if (!rock.active) continue;
        Physics::integrate(rock, dt, m_physics);
    }
    resolveRockVsRock();
    checkPlates();

    // Animate door open/close
    for (auto& door : doors) {
        float target = door.open ? 1.f : 0.f;
        door.openAnim += (target - door.openAnim) * 7.f * dt;
    }

    for (auto& p : parts) {
        if (!p.collected) p.bobTimer += dt;
    }

    // Warp gate glow pulse
    if (warpGate.active) {
        warpGate.glowTimer     += dt;
        warpGate.particleTimer += dt;
    }

    // Mercury gimmick: rocks randomly move every ~2s
    if (m_physics.gimmick == PlanetGimmick::AutoRock) {
        tickMercuryGimmick(dt);
    }
}

void PuzzleSystem::tickMercuryGimmick(float dt) {
    m_mercuryTimer += dt;
    if (m_mercuryTimer < 2.f) return;
    m_mercuryTimer = 0.f;
    if (rocks.empty()) return;
    int idx = rand() % (int)rocks.size();
    if (!rocks[idx].active) return;
    // Try up to 8 angles; skip directions that push rock into player
    for (int attempt = 0; attempt < 8; attempt++) {
        float angle = (float)(rand() % 628) / 100.f;
        Vec2 dir = {std::cos(angle), std::sin(angle)};
        Vec2 toPlayer = playerPos - rocks[idx].pos;
        float dist = toPlayer.length();
        if (dist < 96.f && dist > 0.f) {
            Vec2 norm = toPlayer * (1.f / dist);
            float dot = dir.x * norm.x + dir.y * norm.y;
            if (dot > 0.5f) continue;  // would push rock toward nearby player
        }
        Physics::applyImpulse(rocks[idx], dir, 120.f, m_physics);
        break;
    }
}

void PuzzleSystem::checkPlates() {
    for (auto& plate : plates) {
        plate.pressed = false;
        for (const auto& rock : rocks) {
            if (!rock.active) continue;
            if (rock.getAABB().intersects(plate.area)) {
                plate.pressed = true;
                break;
            }
        }
    }
    // Open a door only when ALL plates linked to it are pressed
    for (int did = 0; did < (int)doors.size(); did++) {
        bool anyLinked = false, allPressed = true;
        for (const auto& plate : plates) {
            if (plate.linkedDoorId == did) {
                anyLinked = true;
                if (!plate.pressed) allPressed = false;
            }
        }
        if (anyLinked) doors[did].open = allPressed;
    }
}

bool PuzzleSystem::isBlocked(const AABB& mover) const {
    for (const auto& door : doors)
        if (!door.open && door.area.intersects(mover)) return true;
    for (const auto& rock : rocks)
        if (rock.active && rock.getAABB().intersects(mover)) return true;
    return false;
}

void PuzzleSystem::resolveRockVsWalls(const std::vector<AABB>& walls) {
    for (auto& rock : rocks) {
        if (!rock.active) continue;
        AABB rb = rock.getAABB();
        for (const auto& wall : walls) {
            if (!rb.intersects(wall)) continue;
            Vec2 push = Physics::resolveOverlap(rb, wall);
            rock.pos += push;
            rb = rock.getAABB();
            if (push.x != 0.f) rock.vel.x *= -0.2f;
            if (push.y != 0.f) rock.vel.y *= -0.2f;
        }
    }
}

void PuzzleSystem::resolveRockVsRock() {
    for (int i = 0; i < (int)rocks.size(); i++) {
        for (int j = i+1; j < (int)rocks.size(); j++) {
            auto& a = rocks[i]; auto& b = rocks[j];
            if (!a.active || !b.active) continue;
            AABB ab = a.getAABB(), bb = b.getAABB();
            if (!ab.intersects(bb)) continue;
            Vec2 push = Physics::resolveOverlap(ab, bb);
            a.pos += push * 0.5f;
            b.pos -= push * 0.5f;
        }
    }
}

int PuzzleSystem::tryCollect(const AABB& player) {
    for (int i = 0; i < (int)parts.size(); i++) {
        auto& p = parts[i];
        if (!p.collected && p.getAABB().intersects(player)) {
            p.collected = true;
            return i;
        }
    }
    return -1;
}

void PuzzleSystem::addPressurePlate(float x, float y, float w, float h, int doorId) {
    plates.push_back({{x,y,w,h}, doorId, false});
}
void PuzzleSystem::addDoor(float x, float y, float w, float h) {
    doors.push_back({{x,y,w,h}, false});
}
void PuzzleSystem::addRock(float x, float y, float mass) {
    RigidBody rb; rb.pos = {x,y}; rb.mass = mass;
    rocks.push_back(rb);
}
void PuzzleSystem::addPart(float x, float y) {
    parts.push_back({{x,y}, false, 0.f});
}
void PuzzleSystem::setWarpGate(float x, float y) {
    warpGate = WarpGate{};
    warpGate.pos = {x, y};
}
void PuzzleSystem::setBaseEntrance(float x, float y) {
    baseEntrance.pos = {x, y};
}
