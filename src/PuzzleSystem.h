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

struct LogFile {
    Vec2  pos;
    int   logId    = 0;
    bool  collected = false;
    float bobTimer  = 0.f;
    AABB getAABB() const { return {pos.x-10.f, pos.y-10.f, 20.f, 20.f}; }
};

struct EnergyCell {
    Vec2  pos;
    bool  collected = false;
    float bobTimer  = 0.f;
    AABB getAABB() const { return {pos.x-10.f, pos.y-10.f, 20.f, 20.f}; }
};

struct EnergyDrink {
    Vec2  pos;
    bool  collected = false;
    float bobTimer  = 0.f;
    AABB getAABB() const { return {pos.x-8.f, pos.y-14.f, 16.f, 28.f}; }
};

struct UnstablePlatform {
    Vec2  pos;
    float w = 64.f, h = 14.f;
    int   state    = 0;   // 0=solid, 1=shaking, 2=falling, 3=gone
    float timer    = 0.f;
    float shakeAmt = 0.f;
    float velY     = 0.f;  // falling velocity
    bool  fallen   = false; // permanently off-screen, removed from walls
    bool  justFell = false; // true for exactly one frame when state 1→2
    AABB getAABB() const {
        return {pos.x - w*0.5f, pos.y - h*0.5f + shakeAmt, w, h};
    }
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
    std::vector<LogFile>          logFiles;
    std::vector<EnergyCell>       energyCells;
    std::vector<EnergyDrink>      energyDrinks;
    std::vector<UnstablePlatform> unstablePlatforms;
    WarpGate                      warpGate;
    BaseEntrance                  baseEntrance;

    bool mercuryGoingLeft = true;
    bool mercuryWarning   = false;

    void update(float dt);
    bool isBlocked(const AABB& mover) const;
    void resolveRockVsWalls(const std::vector<AABB>& walls);
    void resolveRockVsRock();
    int  tryCollect(const AABB& player);
    int  tryCollectLog(const AABB& player);
    int  tryCollectCell(const AABB& player);
    int  tryCollectDrink(const AABB& player);
    bool updateUnstablePlatforms(float dt, const AABB& playerAABB);  // returns true if player fell

    Vec2 rockExternalForce = {};
    Vec2 playerPos         = {};

    void addPressurePlate(float x, float y, float w, float h, int doorId);
    void addDoor(float x, float y, float w, float h);
    void addRock(float x, float y, float mass = 5.f);
    void addPart(float x, float y);
    void addLogFile(float x, float y, int logId);
    void addEnergyCell(float x, float y);
    void addEnergyDrink(float x, float y);
    void addUnstablePlatform(float x, float y, float w = 64.f, float h = 14.f);
    void setWarpGate(float x, float y);
    void setBaseEntrance(float x, float y);
    void activateWarpGate() { warpGate.active = true; }

    const PlanetPhysics& getPhysics() const { return m_physics; }
    void setPlanetPhysics(const PlanetPhysics& p) { m_physics = p; }

private:
    PlanetPhysics m_physics;
    float m_mercuryDirTimer = 0.f;

    void checkPlates();
    void tickMercuryGimmick(float dt);
};
