#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "Player.h"
#include "TileMap.h"
#include "PuzzleSystem.h"
#include "UI.h"
#include "SolarMap.h"

enum class Scene {
    Title,
    Prologue,
    SolarMap,
    PlanetIntro,
    Playing,
    BaseInterior,
    WarpActivation,
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
    bool m_marsZoneShown[3] = {};

    // E-key rock grab/pull state
    int   m_grabbedRock = -1;
    Vec2  m_grabOffset  = {};
    float m_marsRockFlash[5] = {};

    // Base interior player position
    Vec2 m_basePlayerPos = {640.f, 550.f};

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
    std::vector<AABB> collectWalls() const;
    std::vector<AABB> collectBaseWalls() const;

    const PlanetPhysics& curPhysics() const { return Planets::ALL[m_currentPlanet]; }
    static void drawStarfield(SDL_Renderer* r, int w, int h, float timer);
    static void drawStar(SDL_Renderer* r, float x, float y, float sz, Uint8 a);
};
