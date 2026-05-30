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
    for (auto& lf : logFiles) {
        if (!lf.collected) lf.bobTimer += dt;
    }
    for (auto& ec : energyCells) {
        if (!ec.collected) ec.bobTimer += dt;
    }
    for (auto& ed : energyDrinks) {
        if (!ed.collected) ed.bobTimer += dt;
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
    // 6s cycle: 0-3s move left, 3-6s move right
    // 1s warning before each switch (at t=2s warning→left stops, t=5s warning→right stops)
    float prev = m_mercuryDirTimer;
    m_mercuryDirTimer += dt;
    if (m_mercuryDirTimer >= 6.f) m_mercuryDirTimer -= 6.f;

    float t = m_mercuryDirTimer;
    bool wasLeft  = (prev < 3.f);
    bool nowLeft  = (t    < 3.f);
    mercuryGoingLeft = nowLeft;

    // Warning: 1s before phase switch
    mercuryWarning = (t > 2.f && t < 3.f) || (t > 5.f);

    // Apply impulse at phase transition
    bool switched = (wasLeft != nowLeft) || (prev > t); // prev > t means wrapped
    if (switched) {
        Vec2 dir = nowLeft ? Vec2{-1.f, 0.f} : Vec2{1.f, 0.f};
        for (auto& rock : rocks) {
            if (!rock.active) continue;
            Physics::applyImpulse(rock, dir, 140.f, m_physics);
        }
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

int PuzzleSystem::tryCollectLog(const AABB& player) {
    for (int i = 0; i < (int)logFiles.size(); i++) {
        auto& lf = logFiles[i];
        if (!lf.collected && lf.getAABB().intersects(player)) {
            lf.collected = true;
            return lf.logId;
        }
    }
    return -1;
}

int PuzzleSystem::tryCollectCell(const AABB& player) {
    for (int i = 0; i < (int)energyCells.size(); i++) {
        auto& ec = energyCells[i];
        if (!ec.collected && ec.getAABB().intersects(player)) {
            ec.collected = true;
            return i;
        }
    }
    return -1;
}

int PuzzleSystem::tryCollectDrink(const AABB& player) {
    for (int i = 0; i < (int)energyDrinks.size(); i++) {
        auto& ed = energyDrinks[i];
        if (!ed.collected && ed.getAABB().intersects(player)) {
            ed.collected = true;
            return i;
        }
    }
    return -1;
}

bool PuzzleSystem::updateUnstablePlatforms(float dt, const AABB& playerAABB) {
    for (auto& up : unstablePlatforms) {
        // Use non-shaken AABB for contact detection (more reliable)
        AABB platAABB = {up.pos.x - up.w*0.5f, up.pos.y - up.h*0.5f, up.w, up.h};

        if (up.state == 0) {
            // Detect player standing on top of platform (foot contact)
            bool xOver = (playerAABB.x + playerAABB.w > platAABB.x) &&
                         (playerAABB.x < platAABB.x + platAABB.w);
            float playerBottom = playerAABB.y + playerAABB.h;
            bool onTop = xOver &&
                         (playerBottom >= platAABB.y - 2.f) &&
                         (playerBottom <= platAABB.y + 6.f);
            if (onTop) { up.state = 1; up.timer = 3.f; }
        } else if (up.state == 1) {
            up.timer -= dt;
            up.shakeAmt = 4.f * std::sin(up.timer * 22.f) * (up.timer / 3.f);
            if (up.timer <= 0.f) {
                // Platform collapses; heat crack below handles player death naturally
                up.state = 2; up.timer = 10.f; up.shakeAmt = 0.f;
            }
        } else if (up.state == 2) {
            up.timer -= dt;
            if (up.timer <= 0.f) { up.state = 0; up.timer = 0.f; }
        }
    }
    return false;
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
void PuzzleSystem::addLogFile(float x, float y, int logId) {
    logFiles.push_back({{x,y}, logId, false, 0.f});
}
void PuzzleSystem::addEnergyCell(float x, float y) {
    energyCells.push_back({{x,y}, false, 0.f});
}
void PuzzleSystem::addEnergyDrink(float x, float y) {
    energyDrinks.push_back({{x,y}, false, 0.f});
}
void PuzzleSystem::addUnstablePlatform(float x, float y, float w, float h) {
    UnstablePlatform up;
    up.pos = {x, y}; up.w = w; up.h = h;
    unstablePlatforms.push_back(up);
}
void PuzzleSystem::setWarpGate(float x, float y) {
    warpGate = WarpGate{};
    warpGate.pos = {x, y};
}
void PuzzleSystem::setBaseEntrance(float x, float y) {
    baseEntrance.pos = {x, y};
}
