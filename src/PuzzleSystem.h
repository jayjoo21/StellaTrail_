#pragma once
#include "Physics.h"
#include <vector>

struct PressurePlate {
    AABB area;
    int  linkedDoorId = -1;
    bool pressed      = false;
};

struct Door {
    AABB area;
    bool  open     = false;
    float openAnim = 0.f;   // 0=fully closed, 1=fully open (visual only)
};

struct Part {
    Vec2 pos;
    bool collected = false;
    float bobTimer = 0.f;
    AABB getAABB() const { return {pos.x-8.f, pos.y-8.f, 16.f, 16.f}; }
};

struct WarpGate {
    Vec2  pos;
    bool  active    = false;
    bool  triggered = false;
    float glowTimer = 0.f;
    float particleTimer = 0.f;

    AABB getAABB() const {
        return {pos.x-20.f, pos.y-20.f, 40.f, 40.f};
    }
    AABB getTriggerAABB() const {
        return {pos.x-32.f, pos.y-32.f, 64.f, 64.f};
    }
};

struct BaseEntrance {
    Vec2 pos;
    float w = 48.f, h = 64.f;
    AABB getAABB() const {
        return {pos.x - w*0.5f, pos.y - h*0.5f, w, h};
    }
};

class PuzzleSystem {
public:
    std::vector<PressurePlate> plates;
    std::vector<Door>          doors;
    std::vector<RigidBody>     rocks;
    std::vector<Part>          parts;
    WarpGate                   warpGate;
    BaseEntrance               baseEntrance;

    void update(float dt);
    bool isBlocked(const AABB& mover) const;
    void resolveRockVsWalls(const std::vector<AABB>& walls);
    void resolveRockVsRock();
    int  tryCollect(const AABB& player);

    Vec2 rockExternalForce = {};
    Vec2 playerPos         = {};

    void addPressurePlate(float x, float y, float w, float h, int doorId);
    void addDoor(float x, float y, float w, float h);
    void addRock(float x, float y, float mass = 5.f);
    void addPart(float x, float y);
    void setWarpGate(float x, float y);
    void setBaseEntrance(float x, float y);
    void activateWarpGate() { warpGate.active = true; }

    const PlanetPhysics& getPhysics() const { return m_physics; }
    void setPlanetPhysics(const PlanetPhysics& p) { m_physics = p; }

private:
    PlanetPhysics m_physics;
    float m_mercuryTimer = 0.f;

    void checkPlates();
    void tickMercuryGimmick(float dt);
};
