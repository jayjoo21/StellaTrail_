#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include "Player.h"
#include "TileMap.h"
#include "PuzzleSystem.h"
#include "UI.h"
#include "SolarMap.h"

// Saturn ice fragments
struct IceFragment {
    float x, y;
    float vx;           // horizontal velocity (positive=right)
    bool  active    = true;
    bool  hitPlayer = false;
    float radius    = 14.f;
    AABB getAABB() const { return {x-radius, y-radius, radius*2.f, radius*2.f}; }
};

// Venus toxic clouds
struct ToxicCloud {
    Vec2  startPos, endPos, pos;
    float t       = 0.f;
    float speed   = 0.22f;  // t-units per second
    bool  forward = true;
    float stunTimer = 0.f;
    float wobble  = 0.f;
    float radius  = 22.f;
    bool  stunShown = false;
    AABB getAABB() const {
        return {pos.x - radius, pos.y - radius, radius * 2.f, radius * 2.f};
    }
};

// Mars meteor shower
struct MeteorDust { float x, y, vx, vy, life, maxLife; };
struct Meteor {
    float x, y;
    float targetX, landY;
    float warnTimer;   // counts down from 1.5 → 0 = impact
    float fallSpeed;
    bool  active;
    bool  landed;
    float dustTimer;
    std::vector<MeteorDust> dust;
};

enum class Scene {
    Title,
    Prologue,
    SolarMap,
    PlanetIntro,
    Playing,
    BaseInterior,
    WarpActivation,
    GameOver,
    Ending,
};

class Game {
public:
    bool init(int screenW, int screenH, const std::string& title);
    void run();
    void shutdown();

private:
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_running = false;
    int  m_screenW = 1280, m_screenH = 720;

    Scene        m_scene   = Scene::Title;
    Player       m_player;
    TileMap      m_map;
    PuzzleSystem m_puzzle;
    UI           m_ui;
    SolarMap     m_solarMap;

    float m_camX = 0.f, m_camY = 0.f;
    float m_titleTimer    = 0.f;
    float m_prologueTimer = 0.f;
    int   m_prologueLine  = 0;
    float m_introTimer    = 0.f;
    float m_warpTimer     = 0.f;
    float m_endingTimer   = 0.f;
    float m_starAngle     = 0.f;
    float m_baseTimer     = 0.f;

    int  m_currentPlanet    = 3;
    int  m_planetPartsFound = 0;
    int  m_totalPartsFound  = 0;
    int  m_visitProgress    = 0;

    // Gimmick wind state (Neptune)
    float m_windCycle   = 0.f;   // 0-7s cycle
    bool  m_windActive  = false;
    bool  m_windWarning = false;

    // Stella speech bubble (first gimmick trigger)
    bool        m_gimmickSpoken = false;
    float       m_stellaTimer   = 0.f;
    std::string m_stellaText;

    // Mars 3-zone entry hints (zone 0 shown at load, 1-2 on crossing doors)
    bool  m_marsZoneShown[3]   = {};
    int   m_marsZoneTextIdx    = -1;  // 0/1/2 = zone being displayed
    float m_marsZoneTextTimer  = 0.f; // counts down from 4.0s

    // E-key rock grab/pull state
    int   m_grabbedRock = -1;
    Vec2  m_grabOffset  = {};
    float m_marsRockFlash[5] = {};

    // Base interior player position
    Vec2 m_basePlayerPos = {640.f, 480.f};
    bool m_baseReadingBoard = false;

    // Mercury reset system
    std::vector<Vec2> m_initialRockPos;
    float m_resetFade  = 0.f;
    int   m_resetState = 0;   // 0=idle, 1=fading out, 2=restoring+fading in

    // Mercury energy cells
    int m_energyCellsFound = 0;

    // Mars log files
    bool m_marsLogsCollected[3] = {};
    int  m_marsLogReading       = -1;
    bool m_marsArchiveOpen      = false;
    int  m_marsArchiveSel       = 0;

    // Lives system
    int   m_lives        = 3;
    float m_deathFade    = 0.f;
    int   m_deathState   = 0;   // 0=alive, 1=fade-out, 2=fade-in
    Vec2  m_respawnPos;
    float m_gameOverTimer = 0.f;

    // Mercury
    float m_mercuryDayCycle = 0.f;
    std::vector<AABB> m_heatCracks;  // death-zone AABBs (Mercury)

    // Mars meteor shower
    float m_marsNextMeteor     = 6.f;
    std::vector<Meteor> m_marsMeteorites;
    bool  m_marsMeteorDoorOpen = false;
    int   m_marsMeteorPlateIdx = -1;

    // Mercury solar flare
    float m_mercurySolarCycle       = 0.f;
    bool  m_mercurySolarWarning     = false;
    bool  m_mercurySolarFlareActive = false;
    float m_solarFlareTimer         = 0.f;
    float m_solarFlareTint          = 0.f;
    struct SolarBeam { float y; bool hitPlayer; };
    std::vector<SolarBeam> m_solarBeams;

    // Saturn
    std::vector<IceFragment> m_saturnFragments;
    float m_saturnNextFrag = 3.f;

    // Venus
    std::vector<ToxicCloud> m_venusClouds;

    // Jupiter wind + vortex
    float m_jupiterWindCycle   = 0.f;
    int   m_jupiterWindDir     = 0;    // 0=E 1=W 2=S 3=N
    bool  m_jupiterWindWarning = false;
    bool  m_jupiterWindActive  = true;
    bool  m_jupiterPrevWarning = false;
    float m_jupiterVortexTimer = 0.f;
    bool  m_jupiterVortexWarn  = false;
    bool  m_jupiterVortexDanger= false;
    int   m_jupiterHints       = 0;    // bitmask

    // Developer mode
    bool m_devMenuOpen = false;

    void handleEvents();
    void update(float dt);
    void render();

    void updateTitle(float dt);
    void updatePrologue(float dt);
    void updateSolarMap(float dt);
    void updateIntro(float dt);
    void updatePlaying(float dt);
    void updateBaseInterior(float dt);
    void updateWarpActivation(float dt);
    void updateEnding(float dt);

    void renderTitle();
    void renderPrologue();
    void renderSolarMap();
    void renderIntro();
    void renderPlaying();
    void renderBaseInterior();
    void renderWarpActivation();
    void renderEnding();

    void loadPlanet(int planetIdx);
    void onPlanetCleared();
    void updateCamera(float dt);
    void handlePlayerRockInteraction();
    void applyGimmickToPlayer();
    void updateGimmicks(float dt);
    void tryTriggerGimmickSpeech();
    int  tryGrabRock();
    bool isNearRock() const;
    void checkMarsRockBoundaries();
    void loseLife();
    void updateMarsMeteorites(float dt);
    void updateMercurySolarFlare(float dt);
    void updateJupiterGimmick(float dt);
    void updateVenusClouds(float dt);
    void updateSaturnFragments(float dt);
    void renderDevMenu();
    void handleDevMenuClick(int mx, int my);
    std::vector<AABB> collectWalls() const;
    std::vector<AABB> collectBaseWalls() const;

    void updateGameOver(float dt);
    void renderGameOver();

    const PlanetPhysics& curPhysics() const { return Planets::ALL[m_currentPlanet]; }
    static void drawStarfield(SDL_Renderer* r, int w, int h, float timer);
    static void drawStar(SDL_Renderer* r, float x, float y, float sz, Uint8 a);
};
