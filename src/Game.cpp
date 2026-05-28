#include "Game.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

// ---- Data tables ----

static const char* PLANET_MAPS[8] = {
    "assets/maps/planet0_mercury.csv",
    "assets/maps/planet1_venus.csv",
    "assets/maps/planet2_earth.csv",
    "assets/maps/planet3_mars.csv",
    "assets/maps/planet4_jupiter.csv",
    "assets/maps/planet5_saturn.csv",
    "assets/maps/planet6_uranus.csv",
    "assets/maps/planet7_neptune.csv",
};

struct PlanetLayout {
    float rockX[4]; float rockY[4]; int rockCount;
    float plateX, plateY, plateW, plateH;
    float doorX,  doorY,  doorW,  doorH;
    float partX[3], partY[3]; int partCount;
    float baseEntrX, baseEntrY;
    float startX, startY;
};

// All maps: 36 cols × 20 rows = 1152 × 640 px (32px tiles)
// Canyon divider at rows 8-9 (y=256-288). Gap positions vary per planet.
// Upper zone: y < 256 (parts here)
// Lower zone: y > 288 (player, rock, plate, base entrance here)
static const PlanetLayout LAYOUTS[8] = {
    // Mercury (0): 2 rocks, gap at cols 18-19 (x=576-639, y=256), door 64×64
    { {384,672,0,0}, {512,512,0,0}, 2,
      512,352, 32,32,   576,256, 64,64,
      {224,864,0}, {128,128,0}, 2,
      1088,480,   80,544 },
    // Venus (1): 2 rocks, gap at cols 17-19 (x=544, y=256), door 96×64
    { {352,672,0,0}, {480,480,0,0}, 2,
      512,320, 32,32,   544,256, 96,64,
      {192,896,0}, {160,160,0}, 2,
      1088,480,   80,544 },
    // Earth (2): 2 rocks, gap at cols 17-19, door 96×64
    { {320,704,0,0}, {496,496,0,0}, 2,
      512,336, 32,32,   544,256, 96,64,
      {192,896,0}, {128,128,0}, 2,
      1088,480,   80,544 },
    // Mars (3): 1 rock, gap at cols 18-21 (x=576, y=256), door 128×64
    { {384,0,0,0}, {512,0,0,0}, 1,
      384,352, 32,32,   576,256, 128,64,
      {256,832,0}, {128,128,0}, 2,
      1088,480,   80,544 },
    // Jupiter (4): 4 rocks (heavy), gap at cols 18-19, door 64×64
    { {320,576,320,576}, {512,512,448,448}, 4,
      448,352, 32,32,   576,256, 64,64,
      {224,864,0}, {128,128,0}, 2,
      1088,480,   80,544 },
    // Saturn (5): 2 rocks, gap at cols 18-19, door 64×64
    { {352,640,0,0}, {512,512,0,0}, 2,
      512,352, 32,32,   576,256, 64,64,
      {224,864,0}, {128,128,0}, 2,
      1088,480,   80,544 },
    // Uranus (6): 2 rocks, gap at cols 17-19, door 96×64
    { {352,640,0,0}, {480,480,0,0}, 2,
      480,320, 32,32,   544,256, 96,64,
      {192,864,0}, {160,160,0}, 2,
      1088,480,   80,544 },
    // Neptune (7): 2 rocks, gap at cols 17-18, door 64×64, 3 parts
    { {384,640,0,0}, {480,480,0,0}, 2,
      512,320, 32,32,   544,256, 64,64,
      {192,576,896}, {128,160,128}, 3,
      1088,480,   80,544 },
};

static const int TOTAL_PARTS = 7*2 + 3;  // 17: 7 planets × 2 + Neptune × 3

// Updated prologue (spec version)
static const char* PROLOGUE[7] = {
    "...여기가 어디지?",
    "나는 스텔라. 태양계 횡단 임무 중 소행성과 충돌했어.",
    "비행선이 완전히 박살났고... 화성에 불시착했어.",
    "근처에 이상한 구조물이 있어. 가봐야겠어.",
    "...워프 게이트? 고대 외계 문명의 유적인가?",
    "부품을 모아서 게이트를 수리하면 다른 행성으로 이동할 수 있을 것 같아.",
    "좋아. 여기서부터 시작이야.",
};

static const char* EARTH_POPUPS[] = {
    "누군가 살았던 흔적이 있어...",
    "이 별도 언젠간 집이었겠지.",
    "여기서도 누군가 별을 바라봤을까.",
    "폐허가 된 도시... 마음이 아파.",
    "빨리 집에 가고 싶어.",
    "부품 발견! 어서 수리해야 해.",
    "이 행성이 그리울 것 같아.",
    "지구... 언젠가 다시 오고 싶어.",
};
static int s_earthPopupIdx = 0;

static const char* GENERIC_POPUPS[] = {
    "부품 발견!", "워프 게이트에 가까워지고 있어!",
    "조금만 더...", "거의 다 됐어!",
};
static int s_popupIdx = 0;

// Gimmick first-trigger speech
static const char* GIMMICK_SPEECH[8] = {
    "뜨거운 바위들이 저절로 움직여! 타이밍을 잘 봐야겠어.",  // 0 Mercury
    "앞이 잘 안 보여... 천천히 가야겠어.",                   // 1 Venus
    "",                                                       // 2 Earth
    "",                                                       // 3 Mars
    "몸이 너무 무거워... 힘을 더 써야겠어.",                  // 4 Jupiter
    "너무 미끄러워! 조금씩 밀어야겠어.",                     // 5 Saturn
    "뭔가 자꾸 옆으로 밀리는 느낌이야...",                   // 6 Uranus
    "강풍이다! 바위 뒤에 숨거나 타이밍을 기다려!",           // 7 Neptune
};

// Earth emotional text positions (world coords)
struct EmotionalText { float x, y; const char* text; };
static const EmotionalText EARTH_SIGNS[] = {
    {400, 160, "누군가 살았던 흔적이 있어..."},
    {700, 128, "이 별도 언젠간 집이었겠지."},
    {256, 352, "여기서도 누군가 별을 바라봤을까."},
    {800, 352, "폐허가 된 도시... 마음이 아파."},
    {550, 480, "빨리 집에 가고 싶어."},
};

// ---- Init / Shutdown ----

bool Game::init(int sw, int sh, const std::string& title) {
    m_screenW = sw; m_screenH = sh;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
    SDL_Init(SDL_INIT_AUDIO);   // optional — ignore failure if no audio device
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

    m_window = SDL_CreateWindow(title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        sw, sh, SDL_WINDOW_SHOWN);
    if (!m_window) return false;

    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) return false;

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    m_ui.init(m_renderer, "assets/fonts/NotoSansKR.ttf");
    m_solarMap.init(sw, sh);
    m_running = true;
    return true;
}

void Game::shutdown() {
    m_ui.shutdown();
    IMG_Quit();
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

// ---- Level loading ----

void Game::loadPlanet(int idx) {
    m_currentPlanet    = idx;
    m_planetPartsFound = 0;
    m_windCycle  = 0.f;
    m_windActive = false;
    m_windWarning = false;
    m_gimmickSpoken = false;
    m_stellaText.clear();
    m_stellaTimer = 0.f;
    for (int i = 0; i < 3; i++) m_marsZoneShown[i] = false;

    m_grabbedRock = -1;
    m_baseReadingBoard = false;
    for (int i = 0; i < 5; i++) m_marsRockFlash[i] = 0.f;

    m_map = TileMap();
    m_map.load(PLANET_MAPS[idx], m_renderer, "");

    m_puzzle = PuzzleSystem();
    m_puzzle.setPlanetPhysics(Planets::ALL[idx]);

    // Mars: 3-zone push+pull puzzle
    if (idx == 3) {
        // Zone 1 (push only tutorial): rock pushed right to plate
        m_puzzle.addRock(192.f, 580.f, 5.f);                       // rock 0
        m_puzzle.addPressurePlate(224.f, 572.f, 96.f, 32.f, 0);    // right edge=320=Door0
        m_puzzle.addDoor(320.f, 0.f, 32.f, 608.f);
        // Zone 2 (push + pull): rock1 push right, rock2 pull left to upper plate
        m_puzzle.addRock(420.f, 580.f, 5.f);                       // rock 1 — push right
        m_puzzle.addRock(620.f, 400.f, 5.f);                       // rock 2 — pull left (Door1 blocks right side)
        m_puzzle.addPressurePlate(544.f, 572.f, 128.f, 32.f, 1);   // lower plate, right edge=672=Door1
        m_puzzle.addPressurePlate(400.f, 388.f,  80.f, 32.f, 1);   // upper plate for rock2
        m_puzzle.addDoor(672.f, 0.f, 32.f, 608.f);
        // Zone 3 (push + pull combo): rock3 push right, rock4 pull left to upper plate
        m_puzzle.addRock(760.f, 580.f, 5.f);                       // rock 3 — push right
        m_puzzle.addRock(920.f, 350.f, 5.f);                       // rock 4 — pull left (Door2 blocks right side)
        m_puzzle.addPressurePlate(864.f, 572.f,  96.f, 32.f, 2);   // lower plate, right edge=960=Door2
        m_puzzle.addPressurePlate(750.f, 336.f,  80.f, 32.f, 2);   // upper plate for rock4
        m_puzzle.addDoor(960.f, 0.f, 32.f, 608.f);
        // Parts and goal
        m_puzzle.addPart(368.f, 580.f);
        m_puzzle.addPart(714.f, 580.f);
        m_puzzle.setWarpGate(1096.f, 548.f);
        m_puzzle.setBaseEntrance(1096.f, 548.f);
        m_player.pos = {80.f, 548.f};
        m_camX = m_camY = 0.f;
        m_marsZoneShown[0] = true;
        m_stellaText  = "바위를 밀어서 압력판에 올려봐";
        m_stellaTimer = 5.f;
        applyGimmickToPlayer();
        int stage = std::min((m_totalPartsFound * 3) / TOTAL_PARTS, 3);
        m_ui.setShipStage(stage);
        return;
    }

    const PlanetLayout& L = LAYOUTS[idx];

    float rockMass = (idx == 4) ? 15.f : 5.f;
    for (int i = 0; i < L.rockCount; i++)
        m_puzzle.addRock(L.rockX[i], L.rockY[i], rockMass);

    m_puzzle.addPressurePlate(L.plateX, L.plateY, L.plateW, L.plateH, 0);
    m_puzzle.addDoor(L.doorX, L.doorY, L.doorW, L.doorH);

    for (int i = 0; i < L.partCount; i++)
        m_puzzle.addPart(L.partX[i], L.partY[i]);

    // Warp gate stored but used in base interior (position unused on surface)
    m_puzzle.setWarpGate(L.baseEntrX, L.baseEntrY);
    m_puzzle.setBaseEntrance(L.baseEntrX, L.baseEntrY);

    m_player.pos = {L.startX, L.startY};
    m_camX = m_camY = 0.f;

    applyGimmickToPlayer();

    int stage = std::min((m_totalPartsFound * 3) / TOTAL_PARTS, 3);
    m_ui.setShipStage(stage);

    // Trigger speech on planet entry for always-on gimmicks
    auto g = curPhysics().gimmick;
    if (g == PlanetGimmick::HazeVision ||
        g == PlanetGimmick::HeavyGrav  ||
        g == PlanetGimmick::Slippery   ||
        g == PlanetGimmick::SideDrift) {
        tryTriggerGimmickSpeech();
    }
}

void Game::applyGimmickToPlayer() {
    const PlanetPhysics& p = curPhysics();
    m_player.speedMult      = p.playerSpeed;
    m_player.playerFriction = p.playerFriction;
    m_player.externalVel    = {};
    if (p.gimmick == PlanetGimmick::SideDrift)
        m_player.externalVel = {-10.f, 0.f};   // leftward drift
}

void Game::onPlanetCleared() {
    m_solarMap.setPlanetCleared(m_currentPlanet);
    for (int v = 0; v < Planets::COUNT; v++) {
        if (Planets::VISIT_ORDER[v] == m_currentPlanet) {
            m_visitProgress = v + 1;
            if (v + 1 < Planets::COUNT)
                m_solarMap.setPlanetUnlocked(Planets::VISIT_ORDER[v + 1]);
            break;
        }
    }
    if (m_visitProgress >= Planets::COUNT) {
        m_scene = Scene::Ending;
        m_endingTimer = 0.f;
    } else {
        m_warpTimer = 0.f;
        m_scene = Scene::WarpActivation;
    }
}

void Game::tryTriggerGimmickSpeech() {
    if (m_gimmickSpoken) return;
    const char* text = GIMMICK_SPEECH[m_currentPlanet];
    if (!text || text[0] == '\0') { m_gimmickSpoken = true; return; }
    m_gimmickSpoken = true;
    m_stellaText  = text;
    m_stellaTimer = 3.5f;
}

// ---- Main loop ----

void Game::run() {
    using Clock = std::chrono::high_resolution_clock;
    auto prev = Clock::now();
    while (m_running) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - prev).count();
        prev = now;
        dt = std::min(dt, 0.05f);
        handleEvents();
        update(dt);
        render();
    }
}

void Game::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { m_running = false; return; }

        if (m_scene == Scene::SolarMap) m_solarMap.handleEvent(e);

        if (e.type == SDL_KEYDOWN) {
            auto sym = e.key.keysym.sym;
            if (sym == SDLK_e && m_scene == Scene::Playing) {
                if (m_grabbedRock >= 0) {
                    m_grabbedRock = -1;
                } else {
                    m_grabbedRock = tryGrabRock();
                }
            }
            if (sym == SDLK_e && m_scene == Scene::BaseInterior) {
                if (m_baseReadingBoard) {
                    m_baseReadingBoard = false;
                } else {
                    // Check warp gate proximity (E to warp)
                    AABB gateProbe = {560.f, 200.f, 160.f, 160.f};
                    if (m_puzzle.warpGate.active && m_player.getAABB().intersects(gateProbe)) {
                        m_puzzle.warpGate.triggered = true;
                        onPlanetCleared();
                        return;
                    }
                    // Check warning board proximity (E to read)
                    AABB boardProbe = {42.f, 100.f, 290.f, 260.f};
                    if (m_player.getAABB().intersects(boardProbe)) {
                        m_baseReadingBoard = true;
                    }
                }
            }
            if (sym == SDLK_ESCAPE) {
                if (m_scene == Scene::Playing || m_scene == Scene::PlanetIntro) {
                    m_grabbedRock = -1;
                    m_scene = Scene::SolarMap;
                }
                else if (m_scene == Scene::BaseInterior) {
                    m_baseReadingBoard = false;
                    // Return to surface at base entrance
                    const PlanetLayout& L = LAYOUTS[m_currentPlanet];
                    m_player.pos = {L.baseEntrX - 80.f, L.baseEntrY};
                    m_player.vel = {};
                    applyGimmickToPlayer();
                    m_scene = Scene::Playing;
                }
                else if (m_scene == Scene::SolarMap)
                    m_scene = Scene::Title;
                else if (m_scene == Scene::Title)
                    m_running = false;
            }
            if (m_scene == Scene::Title &&
               (sym == SDLK_RETURN || sym == SDLK_SPACE)) {
                m_prologueTimer = 0.f; m_prologueLine = 0;
                m_scene = Scene::Prologue;
            }
            if (m_scene == Scene::Prologue) {
                if (sym == SDLK_ESCAPE) {
                    m_scene = Scene::SolarMap;
                } else if (sym == SDLK_RETURN || sym == SDLK_SPACE) {
                    m_prologueLine++;
                    m_prologueTimer = 0.f;
                    if (m_prologueLine >= 7) m_scene = Scene::SolarMap;
                }
            }
            if (m_scene == Scene::PlanetIntro &&
               (sym == SDLK_RETURN || sym == SDLK_SPACE))
                m_scene = Scene::Playing;
            if (m_scene == Scene::Ending &&
               (sym == SDLK_RETURN || sym == SDLK_SPACE))
                m_running = false;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            if (m_scene == Scene::Prologue) {
                m_prologueLine++;
                m_prologueTimer = 0.f;
                if (m_prologueLine >= 7) m_scene = Scene::SolarMap;
            }
        }
    }
}

void Game::update(float dt) {
    m_starAngle += dt * 0.2f;
    if (m_stellaTimer > 0.f) m_stellaTimer -= dt;
    switch (m_scene) {
        case Scene::Title:          updateTitle(dt);          break;
        case Scene::Prologue:       updatePrologue(dt);       break;
        case Scene::SolarMap:       updateSolarMap(dt);       break;
        case Scene::PlanetIntro:    updateIntro(dt);          break;
        case Scene::Playing:        updatePlaying(dt);        break;
        case Scene::BaseInterior:   updateBaseInterior(dt);   break;
        case Scene::WarpActivation: updateWarpActivation(dt); break;
        case Scene::Ending:         updateEnding(dt);         break;
    }
}

void Game::updateTitle(float dt)  { m_titleTimer += dt; }
void Game::updateEnding(float dt) { m_endingTimer += dt; }
void Game::updatePrologue(float dt) { m_prologueTimer += dt; }

void Game::updateSolarMap(float dt) {
    m_solarMap.update(dt);
    int sel = m_solarMap.consumeSelectedPlanet();
    if (sel >= 0) {
        loadPlanet(sel);
        m_introTimer = 0.f;
        m_scene = Scene::PlanetIntro;
    }
}

void Game::updateIntro(float dt) {
    m_introTimer += dt;
    if (m_introTimer >= 3.5f) m_scene = Scene::Playing;
}

void Game::updatePlaying(float dt) {
    updateGimmicks(dt);

    auto walls = collectWalls();
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    m_player.update(dt, keys, walls);

    // Grabbed rock follows player at fixed offset
    if (m_grabbedRock >= 0) {
        if (m_grabbedRock < (int)m_puzzle.rocks.size() && m_puzzle.rocks[m_grabbedRock].active) {
            m_puzzle.rocks[m_grabbedRock].pos = m_player.pos + m_grabOffset;
            m_puzzle.rocks[m_grabbedRock].vel = {};
        } else {
            m_grabbedRock = -1;
        }
    }

    handlePlayerRockInteraction();
    m_puzzle.resolveRockVsWalls(walls);
    m_puzzle.playerPos = m_player.pos;
    m_puzzle.update(dt);
    updateCamera(dt);

    // Collect parts
    int idx = m_puzzle.tryCollect(m_player.getAABB());
    if (idx >= 0) {
        m_planetPartsFound++;
        m_totalPartsFound++;
        int stage = std::min((m_totalPartsFound * 3) / TOTAL_PARTS, 3);
        m_ui.setShipStage(stage);

        const char* msg = nullptr;
        if (curPhysics().gimmick == PlanetGimmick::Emotional) {
            msg = EARTH_POPUPS[s_earthPopupIdx++ % 8];
        } else {
            msg = GENERIC_POPUPS[s_popupIdx++ % 4];
        }
        m_ui.showPopup(msg, (float)(m_screenW/2), (float)(m_screenH - 120));

        if (m_planetPartsFound >= LAYOUTS[m_currentPlanet].partCount) {
            m_puzzle.activateWarpGate();
            m_ui.showPopup("기지로 돌아가 워프 게이트를 활성화하자!",
                           (float)(m_screenW/2), (float)(m_screenH - 120));
        }
    }

    // Mars zone transition hints + boundary auto-reset
    if (m_currentPlanet == 3) {
        if (!m_marsZoneShown[1] && m_player.pos.x > 354.f) {
            m_marsZoneShown[1] = true;
            m_stellaText  = "이번엔 당겨야 해! E키로 바위를 잡아봐";
            m_stellaTimer = 5.f;
        }
        if (!m_marsZoneShown[2] && m_player.pos.x > 706.f) {
            m_marsZoneShown[2] = true;
            m_stellaText  = "밀기+당기기 조합으로 풀어야 해!";
            m_stellaTimer = 4.5f;
        }
        for (int i = 0; i < 5; i++) {
            if (m_marsRockFlash[i] > 0.f)
                m_marsRockFlash[i] = std::max(0.f, m_marsRockFlash[i] - dt * 2.f);
        }
        checkMarsRockBoundaries();
    }

    // Check base entrance
    if (m_player.getAABB().intersects(m_puzzle.baseEntrance.getAABB())) {
        m_grabbedRock = -1;
        m_baseTimer = 0.f;
        m_player.vel = {};
        m_basePlayerPos = {640.f, 560.f};
        m_player.pos = m_basePlayerPos;
        m_player.externalVel = {};
        m_player.playerFriction = 1.0f;
        m_scene = Scene::BaseInterior;
    }

    m_ui.update(dt);
}

void Game::updateBaseInterior(float dt) {
    m_baseTimer += dt;

    auto walls = collectBaseWalls();
    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    // Temporarily disable all gimmick effects inside base
    float savedFriction = m_player.playerFriction;
    Vec2  savedExtVel   = m_player.externalVel;
    m_player.playerFriction = 1.0f;
    m_player.externalVel    = {};
    m_player.speedMult      = 1.0f;

    m_player.update(dt, keys, walls);

    m_player.playerFriction = savedFriction;
    m_player.externalVel    = savedExtVel;

    // Update warp gate glow
    if (m_puzzle.warpGate.active) {
        m_puzzle.warpGate.glowTimer     += dt;
        m_puzzle.warpGate.particleTimer += dt;
    }

    // Check exit portal (bottom-center, walk-up)
    AABB exitAABB = {560.f, 640.f, 160.f, 60.f};
    if (m_player.getAABB().intersects(exitAABB)) {
        const PlanetLayout& L = LAYOUTS[m_currentPlanet];
        m_player.pos = {L.baseEntrX - 80.f, L.baseEntrY};
        m_player.vel = {};
        applyGimmickToPlayer();
        m_scene = Scene::Playing;
        return;
    }

    m_ui.update(dt);
}

void Game::updateGimmicks(float dt) {
    auto gimmick = curPhysics().gimmick;

    if (gimmick == PlanetGimmick::WindStorm) {
        m_windCycle += dt;
        if (m_windCycle >= 5.0f) m_windCycle = 0.f;

        bool newWarning = (m_windCycle > 1.5f && m_windCycle < 3.5f);
        bool newActive  = (m_windCycle >= 3.5f);

        // First wind warning
        if (newWarning && !m_windWarning) {
            tryTriggerGimmickSpeech();
        }
        m_windWarning = newWarning;
        m_windActive  = newActive;

        if (m_windActive) {
            float wf = 80.f;  // always rightward per spec
            m_player.externalVel       = {wf, 0.f};
            m_puzzle.rockExternalForce = {wf * 2.f, 0.f};
        } else {
            m_player.externalVel       = {};
            m_puzzle.rockExternalForce = {};
        }
    }

    if (gimmick == PlanetGimmick::SideDrift) {
        m_puzzle.rockExternalForce = {-6.f, 0.f};  // leftward drift for rocks
    }
}

void Game::handlePlayerRockInteraction() {
    if (m_grabbedRock >= 0) return;
    if (!m_player.tryPush) return;
    AABB pBox  = m_player.getAABB();
    AABB probe = {pBox.x-4.f, pBox.y-4.f, pBox.w+8.f, pBox.h+8.f};
    for (auto& rock : m_puzzle.rocks) {
        if (!rock.active) continue;
        if (!rock.getAABB().intersects(probe)) continue;
        Physics::applyImpulse(rock, m_player.pushDir, 200.f, m_puzzle.getPhysics());
        Vec2 push = Physics::resolveOverlap(pBox, rock.getAABB());
        m_player.pos += push;
    }
}

void Game::updateCamera(float dt) {
    float tx = m_player.pos.x - m_screenW * 0.5f;
    float ty = m_player.pos.y - m_screenH * 0.5f;
    float mx = (float)(m_map.getPixelWidth()  - m_screenW);
    float my = (float)(m_map.getPixelHeight() - m_screenH);
    tx = std::max(0.f, std::min(tx, std::max(0.f, mx)));
    ty = std::max(0.f, std::min(ty, std::max(0.f, my)));
    m_camX += (tx - m_camX) * 5.f * dt;
    m_camY += (ty - m_camY) * 5.f * dt;
}

std::vector<AABB> Game::collectWalls() const {
    auto walls = m_map.getSolidAABBs();
    for (const auto& door : m_puzzle.doors)
        if (!door.open) walls.push_back(door.area);
    return walls;
}

std::vector<AABB> Game::collectBaseWalls() const {
    return {
        {0,   0,   1280, 40},
        {0,   680, 1280, 40},
        {0,   0,   40,   720},
        {1240, 0,  40,   720},
    };
}

void Game::updateWarpActivation(float dt) {
    m_warpTimer += dt;
    if (m_warpTimer >= 3.0f)
        m_scene = Scene::SolarMap;
}

// ---- Drawing helpers ----

static void fillCircle(SDL_Renderer* r, float cx, float cy, float rad) {
    for (float dy = -rad; dy <= rad; dy += 1.f) {
        float hw = std::sqrt(std::max(0.f, rad*rad - dy*dy));
        SDL_RenderDrawLineF(r, cx - hw, cy + dy, cx + hw, cy + dy);
    }
}

static void fillDiamond(SDL_Renderer* r, float cx, float cy, float s) {
    for (float dy = -s; dy <= s; dy += 1.f) {
        float hw = s - std::abs(dy);
        if (hw > 0.f)
            SDL_RenderDrawLineF(r, cx - hw, cy + dy, cx + hw, cy + dy);
    }
}

static void fillCircleStripe(SDL_Renderer* r, float cx, float cy, float rad,
                              float dyMin, float dyMax) {
    float lo = std::max(-rad, dyMin), hi = std::min(rad, dyMax);
    for (float dy = lo; dy <= hi; dy += 1.f) {
        float hw = std::sqrt(std::max(0.f, rad*rad - dy*dy));
        SDL_RenderDrawLineF(r, cx - hw, cy + dy, cx + hw, cy + dy);
    }
}

static void drawTitlePlanet(SDL_Renderer* r, int idx, float cx, float cy,
                             float rad, float timer) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    switch (idx) {
        case 0: // Mercury
            SDL_SetRenderDrawColor(r, 158, 143, 128, 255);
            fillCircle(r, cx, cy, rad);
            SDL_SetRenderDrawColor(r, 92, 82, 72, 202);
            fillCircle(r, cx - rad*0.30f, cy - rad*0.20f, rad*0.22f);
            fillCircle(r, cx + rad*0.22f, cy + rad*0.32f, rad*0.17f);
            fillCircle(r, cx + rad*0.08f, cy - rad*0.42f, rad*0.13f);
            break;
        case 1: // Venus
            SDL_SetRenderDrawColor(r, 198, 160, 76, 255);
            fillCircle(r, cx, cy, rad);
            for (int b = 0; b < 5; b++) {
                float dy0   = -rad + rad * 0.40f * b;
                float phase = std::sin(timer * 0.5f + b * 1.2f) * rad * 0.08f;
                SDL_SetRenderDrawColor(r, 222, 183, 102, 50);
                fillCircleStripe(r, cx, cy, rad, dy0+phase, dy0+rad*0.15f+phase);
            }
            break;
        case 2: // Earth
            SDL_SetRenderDrawColor(r, 26, 76, 156, 255);
            fillCircle(r, cx, cy, rad);
            SDL_SetRenderDrawColor(r, 36, 128, 56, 202);
            fillCircle(r, cx - rad*0.08f, cy - rad*0.08f, rad*0.36f);
            fillCircle(r, cx + rad*0.30f, cy + rad*0.22f, rad*0.26f);
            fillCircle(r, cx - rad*0.32f, cy + rad*0.28f, rad*0.22f);
            break;
        case 3: // Mars
            SDL_SetRenderDrawColor(r, 176, 78, 46, 255);
            fillCircle(r, cx, cy, rad);
            SDL_SetRenderDrawColor(r, 126, 50, 26, 140);
            fillCircle(r, cx + rad*0.15f, cy + rad*0.12f, rad*0.42f);
            SDL_SetRenderDrawColor(r, 236, 230, 215, 190);
            fillCircleStripe(r, cx, cy, rad, -rad, -rad*0.63f);
            break;
        case 4: // Jupiter
            {
                Uint8 sR[] = {208,152,225,145,202};
                Uint8 sG[] = {160, 95,175, 85,150};
                Uint8 sB[] = { 88, 56,115, 45, 95};
                for (int s = 0; s < 5; s++) {
                    float dy0 = -rad + rad * 2.f * s / 5.f;
                    SDL_SetRenderDrawColor(r, sR[s], sG[s], sB[s], 255);
                    fillCircleStripe(r, cx, cy, rad, dy0, dy0 + rad*2.f/5.f);
                }
                SDL_SetRenderDrawColor(r, 196, 76, 56, 202);
                fillCircle(r, cx + rad*0.18f, cy + rad*0.16f, rad*0.24f);
            }
            break;
        case 5: // Saturn
            {
                float rRx = rad*1.95f, rRy = rad*0.42f, rIn = rad*1.12f;
                for (float dy = 0.f; dy <= rRy; dy += 1.f) {
                    float hw  = rRx * std::sqrt(std::max(0.f, 1.f - (dy/rRy)*(dy/rRy)));
                    float hwI = std::sqrt(std::max(0.f, rIn*rIn - dy*dy));
                    if (hw > hwI) {
                        SDL_SetRenderDrawColor(r, 205, 185, 125, 122);
                        SDL_RenderDrawLineF(r, cx-hw, cy+dy, cx-hwI, cy+dy);
                        SDL_RenderDrawLineF(r, cx+hwI, cy+dy, cx+hw, cy+dy);
                    }
                }
                SDL_SetRenderDrawColor(r, 210, 194, 146, 255);
                fillCircle(r, cx, cy, rad);
                for (float dy = -rRy; dy < 0.f; dy += 1.f) {
                    float hw  = rRx * std::sqrt(std::max(0.f, 1.f - (dy/rRy)*(dy/rRy)));
                    float hwI = std::sqrt(std::max(0.f, rIn*rIn - dy*dy));
                    if (hw > hwI) {
                        SDL_SetRenderDrawColor(r, 215, 195, 135, 160);
                        SDL_RenderDrawLineF(r, cx-hw, cy+dy, cx-hwI, cy+dy);
                        SDL_RenderDrawLineF(r, cx+hwI, cy+dy, cx+hw, cy+dy);
                    }
                }
            }
            break;
        case 6: // Uranus
            SDL_SetRenderDrawColor(r, 76, 196, 206, 255);
            fillCircle(r, cx, cy, rad);
            SDL_SetRenderDrawColor(r, 96, 215, 222, 70);
            fillCircleStripe(r, cx, cy, rad, -rad, -rad*0.48f);
            break;
        case 7: // Neptune
            SDL_SetRenderDrawColor(r, 36, 56, 196, 255);
            fillCircle(r, cx, cy, rad);
            SDL_SetRenderDrawColor(r, 195, 215, 255, 170);
            fillCircle(r, cx + rad*0.22f, cy - rad*0.18f, rad*0.16f);
            break;
        default: break;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void drawAstronaut(SDL_Renderer* r, float cx, float cy, float timer) {
    const float s  = 1.4f;
    float bob      = std::sin(timer * 1.5f) * 4.f;
    float swing    = std::sin(timer * 1.5f) * 5.f;
    cy += bob;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // Backpack
    SDL_SetRenderDrawColor(r, 183, 192, 205, 255);
    SDL_FRect pack = {cx+15.f*s, cy-8.f*s, 14.f*s, 30.f*s};
    SDL_RenderFillRectF(r, &pack);
    // Legs
    SDL_SetRenderDrawColor(r, 213, 218, 232, 255);
    SDL_FRect lLeg = {cx-12.f*s, cy+26.f*s, 11.f*s, 24.f*s};
    SDL_FRect rLeg = {cx+ 1.f*s, cy+26.f*s, 11.f*s, 24.f*s};
    SDL_RenderFillRectF(r, &lLeg);
    SDL_RenderFillRectF(r, &rLeg);
    // Boots
    SDL_SetRenderDrawColor(r, 143, 152, 168, 255);
    SDL_FRect lBoot = {cx-14.f*s, cy+47.f*s, 14.f*s, 8.f*s};
    SDL_FRect rBoot = {cx+ 0.f,   cy+47.f*s, 14.f*s, 8.f*s};
    SDL_RenderFillRectF(r, &lBoot);
    SDL_RenderFillRectF(r, &rBoot);
    // Arms
    SDL_SetRenderDrawColor(r, 216, 222, 234, 255);
    SDL_FRect lArm = {cx-26.f*s, cy-6.f*s+swing*s, 11.f*s, 26.f*s};
    SDL_FRect rArm = {cx+15.f*s, cy-6.f*s-swing*s, 11.f*s, 26.f*s};
    SDL_RenderFillRectF(r, &lArm);
    SDL_RenderFillRectF(r, &rArm);
    // Body
    SDL_SetRenderDrawColor(r, 220, 226, 240, 255);
    SDL_FRect body = {cx-16.f*s, cy-8.f*s, 32.f*s, 36.f*s};
    SDL_RenderFillRectF(r, &body);
    // Chest panel
    SDL_SetRenderDrawColor(r, 92, 146, 255, 205);
    SDL_FRect cl1 = {cx-8.f*s, cy+0.5f*s, 5.5f*s, 5.f*s};
    SDL_RenderFillRectF(r, &cl1);
    SDL_SetRenderDrawColor(r, 92, 255, 146, 170);
    SDL_FRect cl2 = {cx+0.5f*s, cy+0.5f*s, 4.5f*s, 4.5f*s};
    SDL_RenderFillRectF(r, &cl2);
    // Helmet
    SDL_SetRenderDrawColor(r, 198, 208, 226, 255);
    fillCircle(r, cx, cy-30.f*s, 23.f*s);
    SDL_SetRenderDrawColor(r, 33, 62, 112, 222);
    fillCircle(r, cx, cy-28.f*s, 15.f*s);
    SDL_SetRenderDrawColor(r, 188, 213, 255, 105);
    fillCircle(r, cx-5.f*s, cy-34.f*s, 6.f*s);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::drawStar(SDL_Renderer* r, float x, float y, float sz, Uint8 a) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 240, a);
    SDL_FRect s = {x-sz/2.f, y-sz/2.f, sz, sz};
    SDL_RenderFillRectF(r, &s);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void Game::drawStarfield(SDL_Renderer* r, int w, int h, float timer) {
    srand(42);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 150; i++) {
        float x = (float)(rand() % w);
        float y = (float)(rand() % h);
        float tw = (std::sin(timer * 2.f + i * 0.7f) + 1.f) * 0.5f;
        Uint8 a  = (Uint8)(80 + 120 * tw);
        SDL_SetRenderDrawColor(r, 255, 255, 240, a);
        SDL_FRect s = {x, y, (i%4==0) ? 2.f : 1.f, (i%4==0) ? 2.f : 1.f};
        SDL_RenderFillRectF(r, &s);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// ---- Rendering ----

void Game::render() {
    const auto& sky = curPhysics().skyColor;
    SDL_SetRenderDrawColor(m_renderer, sky.r, sky.g, sky.b, 255);
    SDL_RenderClear(m_renderer);

    switch (m_scene) {
        case Scene::Title:          renderTitle();          break;
        case Scene::Prologue:       renderPrologue();       break;
        case Scene::SolarMap:       renderSolarMap();       break;
        case Scene::PlanetIntro:    renderIntro();          break;
        case Scene::Playing:        renderPlaying();        break;
        case Scene::BaseInterior:   renderBaseInterior();   break;
        case Scene::WarpActivation: renderWarpActivation(); break;
        case Scene::Ending:         renderEnding();         break;
    }
    SDL_RenderPresent(m_renderer);
}

void Game::renderTitle() {
    SDL_SetRenderDrawColor(m_renderer, 4, 6, 18, 255);
    SDL_RenderClear(m_renderer);
    drawStarfield(m_renderer, m_screenW, m_screenH, m_titleTimer);

    // 8 planets in circular arrangement at bottom
    static const float PLANET_RADII[8] = {14.f,18.f,18.f,16.f,26.f,24.f,20.f,20.f};
    float cx0 = m_screenW * 0.5f;
    float cy0 = m_screenH * 0.78f;
    float ringR = m_screenW * 0.38f;
    for (int p = 0; p < 8; p++) {
        float ang = -3.14159f + p * (2.f * 3.14159f / 8.f);
        float px  = cx0 + ringR * std::cos(ang);
        float py  = cy0 + ringR * 0.35f * std::sin(ang);  // elliptical
        float bob = std::sin(m_titleTimer * 1.1f + p * 0.75f) * 5.f;
        drawTitlePlanet(m_renderer, p, px, py + bob, PLANET_RADII[p], m_titleTimer);
    }

    // Title panel
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 150);
    SDL_FRect panel = {m_screenW/2.f-260.f, 110.f, 520.f, 230.f};
    SDL_RenderFillRectF(m_renderer, &panel);
    SDL_SetRenderDrawColor(m_renderer, 88, 128, 255, 175);
    SDL_FRect bar = {m_screenW/2.f-260.f, 110.f, 520.f, 3.f};
    SDL_RenderFillRectF(m_renderer, &bar);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    m_ui.renderTitleScreen(m_renderer, m_screenW, m_screenH, m_titleTimer);
}

void Game::renderPrologue() {
    SDL_SetRenderDrawColor(m_renderer, 2, 4, 12, 255);
    SDL_RenderClear(m_renderer);
    drawStarfield(m_renderer, m_screenW, m_screenH, m_prologueTimer * 0.25f);

    if (m_prologueLine >= 7) return;

    float astX = m_screenW * 0.115f;
    float astY = m_screenH * 0.64f;
    drawAstronaut(m_renderer, astX, astY, m_prologueTimer);

    float fadeIn = std::min(m_prologueTimer / 0.18f, 1.f);
    float bx = m_screenW * 0.25f;
    float by = m_screenH * 0.43f;
    float bw = m_screenW * 0.68f;
    float bh = 148.f;

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, (Uint8)(68 * fadeIn));
    SDL_FRect shadow = {bx+5.f, by+5.f, bw, bh};
    SDL_RenderFillRectF(m_renderer, &shadow);
    SDL_SetRenderDrawColor(m_renderer, 12, 16, 38, (Uint8)(215 * fadeIn));
    SDL_FRect bg = {bx, by, bw, bh};
    SDL_RenderFillRectF(m_renderer, &bg);
    SDL_SetRenderDrawColor(m_renderer, 90, 138, 215, (Uint8)(192 * fadeIn));
    SDL_RenderDrawRectF(m_renderer, &bg);

    // Bubble tail
    float ty = by + bh * 0.48f;
    SDL_SetRenderDrawColor(m_renderer, 12, 16, 38, (Uint8)(215 * fadeIn));
    for (int i = 1; i <= 14; i++) {
        float hh = i * 0.40f;
        SDL_RenderDrawLineF(m_renderer, bx-(float)i, ty-hh, bx-(float)i, ty+hh);
    }
    SDL_SetRenderDrawColor(m_renderer, 90, 138, 215, (Uint8)(192 * fadeIn));
    SDL_RenderDrawLineF(m_renderer, bx, ty, bx-14.f, ty-5.6f);
    SDL_RenderDrawLineF(m_renderer, bx, ty, bx-14.f, ty+5.6f);

    // Progress dots
    float pulse = (std::sin(m_prologueTimer * 4.f) + 1.f) * 0.5f;
    for (int i = 0; i < 7; i++) {
        float dx = bx + 14.f + i * 16.f;
        float dy = by + bh - 20.f;
        if (i < m_prologueLine)
            SDL_SetRenderDrawColor(m_renderer, 90, 138, 215, (Uint8)(172 * fadeIn));
        else if (i == m_prologueLine)
            SDL_SetRenderDrawColor(m_renderer, 178, 218, 255, (Uint8)((152 + 98*pulse) * fadeIn));
        else
            SDL_SetRenderDrawColor(m_renderer, 40, 56, 92, (Uint8)(145 * fadeIn));
        SDL_FRect dot = {dx, dy, 9.f, 9.f};
        SDL_RenderFillRectF(m_renderer, &dot);
    }
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    float textCX = bx + bw * 0.5f;
    float textCY = by + bh * 0.38f;
    m_ui.renderPrologueLine(m_renderer, PROLOGUE[m_prologueLine], m_prologueLine,
                             textCX, textCY, (Uint8)(255 * fadeIn));

    float blink = (std::sin(m_prologueTimer * 2.5f) + 1.f) * 0.5f;
    m_ui.renderSkipHint(m_renderer, m_screenW, m_screenH, (Uint8)(85 + 88*blink));
}

void Game::renderSolarMap() {
    m_solarMap.render(m_renderer, m_ui.getFont(), m_ui.getFontBig());
}

void Game::renderIntro() {
    drawStarfield(m_renderer, m_screenW, m_screenH, m_introTimer);
    float alpha = std::min(m_introTimer / 0.5f, 1.f);
    if (m_introTimer > 3.0f) alpha = std::max(0.f, 1.f - (m_introTimer-3.0f)/0.5f);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, (Uint8)(170 * alpha));
    SDL_FRect panel = {m_screenW/2.f-300.f, m_screenH/2.f-100.f, 600.f, 200.f};
    SDL_RenderFillRectF(m_renderer, &panel);
    const auto& ac = curPhysics().ambientColor;
    SDL_SetRenderDrawColor(m_renderer, ac.r, ac.g, ac.b, (Uint8)(220*alpha));
    SDL_FRect bar = {m_screenW/2.f-300.f, m_screenH/2.f-100.f, 600.f, 6.f};
    SDL_RenderFillRectF(m_renderer, &bar);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    m_ui.renderPlanetIntro(m_renderer, m_screenW, m_screenH,
                            curPhysics(), m_currentPlanet, (Uint8)(255*alpha));
}

void Game::renderPlaying() {
    m_map.render(m_renderer, m_camX, m_camY);

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // Mercury heat particles around rocks
    if (curPhysics().gimmick == PlanetGimmick::AutoRock) {
        srand(77);
        for (const auto& rock : m_puzzle.rocks) {
            if (!rock.active) continue;
            float rx = rock.pos.x - m_camX, ry = rock.pos.y - m_camY;
            for (int p = 0; p < 6; p++) {
                float a = m_titleTimer * 3.f + p * 1.047f;
                float pr = 16.f + 6.f * std::sin(m_titleTimer * 4.f + p);
                float px = rx + pr * std::cos(a);
                float py = ry + pr * std::sin(a);
                Uint8 pa = (Uint8)(80 + 60 * std::sin(m_titleTimer * 5.f + p * 0.7f));
                SDL_SetRenderDrawColor(m_renderer, 255, 80, 20, pa);
                SDL_FRect dot = {px-2.f, py-2.f, 4.f, 4.f};
                SDL_RenderFillRectF(m_renderer, &dot);
            }
        }
    }

    // Saturn floor shimmer
    if (curPhysics().gimmick == PlanetGimmick::Slippery) {
        float shimmer = (std::sin(m_titleTimer * 4.f) + 1.f) * 0.5f;
        SDL_SetRenderDrawColor(m_renderer, 230, 215, 170, (Uint8)(20 + 15 * shimmer));
        SDL_FRect overlay = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
        SDL_RenderFillRectF(m_renderer, &overlay);
    }

    // Pressure plates — button look: gray inactive / green+glow active
    for (const auto& plate : m_puzzle.plates) {
        float ppx = plate.area.x - m_camX, ppy = plate.area.y - m_camY;
        float ppw = plate.area.w, pph = plate.area.h;
        float glow = (std::sin(m_titleTimer * 5.f) + 1.f) * 0.5f;
        if (plate.pressed) {
            SDL_SetRenderDrawColor(m_renderer, 25, (Uint8)(145+65*glow), 45, 200);
            SDL_FRect outer = {ppx-4.f, ppy-4.f, ppw+8.f, pph+8.f};
            SDL_RenderFillRectF(m_renderer, &outer);
            SDL_SetRenderDrawColor(m_renderer, 55, 225, 85, 255);
            SDL_FRect rf = {ppx, ppy, ppw, pph};
            SDL_RenderFillRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, 150, 255, 170, 220);
            SDL_RenderDrawRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, 190, 255, 205, 220);
            SDL_RenderDrawLineF(m_renderer, ppx+ppw*0.2f, ppy+pph*0.5f, ppx+ppw*0.8f, ppy+pph*0.5f);
            SDL_RenderDrawLineF(m_renderer, ppx+ppw*0.5f, ppy+pph*0.12f, ppx+ppw*0.5f, ppy+pph*0.88f);
        } else {
            SDL_SetRenderDrawColor(m_renderer, 68, 70, 80, 210);
            SDL_FRect rf = {ppx, ppy, ppw, pph};
            SDL_RenderFillRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, 100, 103, 118, 255);
            SDL_RenderDrawRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, 50, 52, 60, 210);
            SDL_FRect inner = {ppx+3.f, ppy+3.f, ppw-6.f, pph-6.f};
            SDL_RenderFillRectF(m_renderer, &inner);
            SDL_SetRenderDrawColor(m_renderer, 90, 93, 108, 130);
            SDL_RenderDrawLineF(m_renderer, ppx+ppw*0.2f, ppy+pph*0.5f, ppx+ppw*0.8f, ppy+pph*0.5f);
            SDL_RenderDrawLineF(m_renderer, ppx+ppw*0.5f, ppy+pph*0.12f, ppx+ppw*0.5f, ppy+pph*0.88f);
        }
    }

    // Doors — red+lock when closed, green slide-up animation when opening
    for (const auto& door : m_puzzle.doors) {
        float anim = door.openAnim;
        if (anim >= 0.98f) continue;
        float ddx  = door.area.x - m_camX;
        float baseY = door.area.y - m_camY;
        float ddw  = door.area.w;
        float dh_vis = door.area.h * (1.f - anim);
        float dy_vis = baseY + door.area.h * anim;  // top slides up
        SDL_FRect rf = {ddx, dy_vis, ddw, dh_vis};
        if (!door.open) {
            float pulse = (std::sin(m_titleTimer * 3.f) + 1.f) * 0.5f;
            SDL_SetRenderDrawColor(m_renderer, (Uint8)(155+45*pulse), 30, 30, 238);
            SDL_RenderFillRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, 255, 65, 45, 255);
            SDL_RenderDrawRectF(m_renderer, &rf);
            // Lock icon at door center
            if (anim < 0.05f) {
                float lx = ddx + ddw * 0.5f;
                float ly = baseY + door.area.h * 0.5f;
                SDL_SetRenderDrawColor(m_renderer, 218, 182, 68, 230);
                SDL_FRect lockBody = {lx-7.f, ly, 14.f, 11.f};
                SDL_RenderFillRectF(m_renderer, &lockBody);
                SDL_SetRenderDrawColor(m_renderer, 238, 202, 88, 255);
                SDL_RenderDrawLineF(m_renderer, lx-5.f, ly,    lx-5.f, ly-8.f);
                SDL_RenderDrawLineF(m_renderer, lx-5.f, ly-8.f, lx+5.f, ly-8.f);
                SDL_RenderDrawLineF(m_renderer, lx+5.f, ly-8.f, lx+5.f, ly);
            }
        } else {
            Uint8 ga = (Uint8)(215 * (1.f - anim));
            SDL_SetRenderDrawColor(m_renderer, 45, 195, 65, ga);
            SDL_RenderFillRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, 95, 250, 115, ga);
            SDL_RenderDrawRectF(m_renderer, &rf);
        }
    }

    // Base entrance
    {
        float bx = m_puzzle.baseEntrance.pos.x - m_camX;
        float by = m_puzzle.baseEntrance.pos.y - m_camY;
        float bw = m_puzzle.baseEntrance.w;
        float bh = m_puzzle.baseEntrance.h;
        float pulse = (std::sin(m_titleTimer * 2.5f) + 1.f) * 0.5f;
        SDL_SetRenderDrawColor(m_renderer, 40, 80, 180, (Uint8)(120 + 60 * pulse));
        SDL_FRect rf = {bx - bw*0.5f, by - bh*0.5f, bw, bh};
        SDL_RenderFillRectF(m_renderer, &rf);
        SDL_SetRenderDrawColor(m_renderer, 80, 140, 255, (Uint8)(180 + 60 * pulse));
        SDL_RenderDrawRectF(m_renderer, &rf);
        // Door frame top bar
        SDL_SetRenderDrawColor(m_renderer, 100, 120, 200, 200);
        SDL_FRect top = {bx - bw*0.5f, by - bh*0.5f, bw, 6.f};
        SDL_RenderFillRectF(m_renderer, &top);
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    // "E키로 입장" label above base entrance
    if (m_ui.getFont()) {
        float bx2 = m_puzzle.baseEntrance.pos.x - m_camX;
        float by2 = m_puzzle.baseEntrance.pos.y - m_camY;
        float bh2 = m_puzzle.baseEntrance.h;
        float labelA = 0.55f + 0.45f * std::sin(m_titleTimer * 2.5f);
        SDL_Color lc = {130, 195, 255, (Uint8)(225 * labelA)};
        m_ui.renderText(m_renderer, m_ui.getFont(), "E키로 입장",
                        bx2, by2 - bh2 * 0.5f - 22.f, lc, true);
    }

    // Earth emotional signs
    if (m_currentPlanet == 2 && m_ui.getFont()) {
        for (const auto& sign : EARTH_SIGNS) {
            float sx = sign.x - m_camX, sy = sign.y - m_camY;
            float dist = std::abs(m_player.pos.x - sign.x) + std::abs(m_player.pos.y - sign.y);
            float alpha = std::max(0.f, 1.f - dist / 200.f);
            if (alpha > 0.02f) {
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(m_renderer, 20, 30, 60, (Uint8)(140 * alpha));
                SDL_FRect bg = {sx - 100.f, sy - 18.f, 200.f, 28.f};
                SDL_RenderFillRectF(m_renderer, &bg);
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
                SDL_Color tc = {200, 220, 255, (Uint8)(200 * alpha)};
                m_ui.renderText(m_renderer, m_ui.getFont(), sign.text, sx, sy - 12.f, tc, true);
            }
        }
    }

    // Y-sorted drawables
    struct Drawable { float y; int type; int idx; };
    std::vector<Drawable> dlist;
    dlist.push_back({m_player.pos.y, 0, 0});
    for (int i = 0; i < (int)m_puzzle.parts.size(); i++)
        if (!m_puzzle.parts[i].collected)
            dlist.push_back({m_puzzle.parts[i].pos.y, 1, i});
    for (int i = 0; i < (int)m_puzzle.rocks.size(); i++)
        if (m_puzzle.rocks[i].active)
            dlist.push_back({m_puzzle.rocks[i].pos.y, 2, i});
    std::sort(dlist.begin(), dlist.end(),
              [](const Drawable& a, const Drawable& b){ return a.y < b.y; });

    for (const auto& d : dlist) {
        if (d.type == 0) {
            // Yellow outline around player when grabbing
            if (m_grabbedRock >= 0) {
                float px = m_player.pos.x - m_camX, py = m_player.pos.y - m_camY;
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(m_renderer, 255, 230, 0, 200);
                SDL_FRect pOutline = {px - 20.f, py - 20.f, 40.f, 40.f};
                SDL_RenderDrawRectF(m_renderer, &pOutline);
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
            }
            m_player.render(m_renderer, m_camX, m_camY);
        } else if (d.type == 1) {
            // Part: yellow diamond star + sparkle particles
            const auto& pt = m_puzzle.parts[d.idx];
            float bob  = std::sin(pt.bobTimer * 2.5f) * 4.f;
            float ptx  = pt.pos.x - m_camX, pty = pt.pos.y - m_camY + bob;
            float glw  = (std::sin(pt.bobTimer * 3.f) + 1.f) * 0.5f;
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            // Outer aura
            SDL_SetRenderDrawColor(m_renderer, 255, 215, 40, (Uint8)(35 + 30*glw));
            SDL_FRect aura = {ptx-18.f, pty-18.f, 36.f, 36.f};
            SDL_RenderFillRectF(m_renderer, &aura);
            // Diamond body
            SDL_SetRenderDrawColor(m_renderer, 255, 210, 35, 255);
            fillDiamond(m_renderer, ptx, pty, 10.f);
            // Bright core
            SDL_SetRenderDrawColor(m_renderer, 255, 255, 175, 255);
            fillDiamond(m_renderer, ptx, pty, 4.f);
            // Cross spikes
            SDL_SetRenderDrawColor(m_renderer, 255, 238, 115, (Uint8)(175 + 70*glw));
            SDL_RenderDrawLineF(m_renderer, ptx, pty-15.f, ptx, pty+15.f);
            SDL_RenderDrawLineF(m_renderer, ptx-15.f, pty, ptx+15.f, pty);
            // Orbiting sparkles
            srand((int)(pt.bobTimer * 10) + d.idx * 7);
            for (int sp = 0; sp < 4; sp++) {
                float sa  = pt.bobTimer * 2.2f + sp * 1.5708f;
                float sr  = 14.f + 4.f * std::sin(pt.bobTimer * 4.f + sp);
                float spx = ptx + sr * std::cos(sa);
                float spy = pty + sr * std::sin(sa);
                Uint8 spa = (Uint8)(110 + 110 * std::sin(pt.bobTimer * 5.f + sp * 2.f));
                SDL_SetRenderDrawColor(m_renderer, 255, 238, 95, spa);
                SDL_FRect sdot = {spx-2.f, spy-2.f, 4.f, 4.f};
                SDL_RenderFillRectF(m_renderer, &sdot);
            }
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        } else {
            // Rock: brown circle + highlight + dark border
            const auto& rock = m_puzzle.rocks[d.idx];
            float rx  = rock.pos.x - m_camX, ry = rock.pos.y - m_camY;
            float rad = rock.radius;
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            // Ellipse shadow
            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 55);
            SDL_FRect shadow = {rx - rad + 4.f, ry + rad * 0.65f, (rad - 4.f)*2.f, rad * 0.45f};
            SDL_RenderFillRectF(m_renderer, &shadow);
            // Dark border ring
            SDL_SetRenderDrawColor(m_renderer, 70, 48, 28, 210);
            fillCircle(m_renderer, rx, ry, rad + 1.5f);
            // Rock body
            SDL_SetRenderDrawColor(m_renderer, 115, 85, 55, 255);
            fillCircle(m_renderer, rx, ry, rad);
            // Highlight (top-left)
            SDL_SetRenderDrawColor(m_renderer, 158, 122, 84, 200);
            fillCircle(m_renderer, rx - rad*0.28f, ry - rad*0.28f, rad * 0.42f);
            // Yellow outline when grabbed
            if (d.idx == m_grabbedRock) {
                SDL_SetRenderDrawColor(m_renderer, 255, 230, 0, 230);
                SDL_FRect rOutline = {rx - rad - 3.f, ry - rad - 3.f, (rad + 3.f)*2.f, (rad + 3.f)*2.f};
                SDL_RenderDrawRectF(m_renderer, &rOutline);
            }
            // White flash sparkle on zone-reset
            if (m_currentPlanet == 3 && d.idx < 5 && m_marsRockFlash[d.idx] > 0.f) {
                Uint8 fa = (Uint8)(m_marsRockFlash[d.idx] * 255.f);
                SDL_SetRenderDrawColor(m_renderer, 255, 255, 200, fa);
                fillCircle(m_renderer, rx, ry, rad + 6.f);
            }
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        }
    }

    // E-key hint above player
    if (m_ui.getFont()) {
        float hx = m_player.pos.x - m_camX;
        float hy = m_player.pos.y - m_camY - 42.f;
        if (m_grabbedRock >= 0) {
            SDL_Color hc = {255, 230, 50, 230};
            m_ui.renderText(m_renderer, m_ui.getFont(), "E: 놓기", hx, hy, hc, true);
        } else if (isNearRock()) {
            SDL_Color hc = {200, 240, 100, 210};
            m_ui.renderText(m_renderer, m_ui.getFont(), "E: 잡기", hx, hy, hc, true);
        }
    }

    // Venus haze
    if (curPhysics().gimmick == PlanetGimmick::HazeVision) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        srand(5678);
        for (int i = 0; i < 30; i++) {
            float nx = (float)(rand() % m_screenW);
            float ny = (float)(rand() % m_screenH);
            float nh = 10.f + (rand() % 30);
            float na = std::sin(m_titleTimer * 3.f + i * 1.3f);
            Uint8 haze = (Uint8)(20 + 25 * ((na + 1.f) * 0.5f));
            SDL_SetRenderDrawColor(m_renderer, 200, 160, 60, haze);
            SDL_FRect hrf = {nx, ny, (float)m_screenW, nh};
            SDL_RenderFillRectF(m_renderer, &hrf);
        }
        // Edge fade-out (vision limit)
        const float edgeFade = 120.f;
        SDL_Color vc = curPhysics().skyColor;
        for (int i = 0; i < 8; i++) {
            float t = i / 8.f;
            Uint8 fa = (Uint8)(200 * (1.f - t));
            SDL_SetRenderDrawColor(m_renderer, vc.r, vc.g, vc.b, fa);
            SDL_FRect top = {0, 0, (float)m_screenW, edgeFade * (1.f - t)};
            SDL_FRect bot = {0, (float)m_screenH - edgeFade*(1.f-t), (float)m_screenW, edgeFade*(1.f-t)};
            SDL_FRect lft = {0, 0, edgeFade*(1.f-t), (float)m_screenH};
            SDL_FRect rgt = {(float)m_screenW - edgeFade*(1.f-t), 0, edgeFade*(1.f-t), (float)m_screenH};
            SDL_RenderFillRectF(m_renderer, &top);
            SDL_RenderFillRectF(m_renderer, &bot);
            SDL_RenderFillRectF(m_renderer, &lft);
            SDL_RenderFillRectF(m_renderer, &rgt);
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }

    // Neptune wind streaks
    if (m_windActive) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        srand((int)(m_windCycle * 30));
        for (int i = 0; i < 24; i++) {
            float sy2 = (float)(rand() % m_screenH);
            float len2 = 50.f + (rand() % 100);
            float alpha2 = (rand() % 80) + 30.f;
            SDL_SetRenderDrawColor(m_renderer, 150, 180, 255, (Uint8)alpha2);
            SDL_RenderDrawLineF(m_renderer, 0, sy2, len2, sy2 + (rand()%8 - 4));
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }

    // Goal text (bottom center, above total parts HUD)
    if (m_ui.getFont()) {
        const char* goal = m_puzzle.warpGate.active
            ? "기지로 돌아가 워프 게이트를 활성화하라!"
            : "바위를 압력판에 올려 부품을 획득하라";
        float gp = 0.65f + 0.35f * std::sin(m_titleTimer * 2.f);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 115);
        SDL_FRect goalBg = {m_screenW*0.5f-225.f, (float)m_screenH-58.f, 450.f, 26.f};
        SDL_RenderFillRectF(m_renderer, &goalBg);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        SDL_Color gc = {175, 210, 255, (Uint8)(195 * gp)};
        m_ui.renderText(m_renderer, m_ui.getFont(), goal,
                        m_screenW*0.5f, (float)m_screenH - 44.f, gc, true);
    }

    // HUD
    bool gimmickActive = (curPhysics().gimmick != PlanetGimmick::None);
    bool minimapDisabled = (curPhysics().gimmick == PlanetGimmick::HazeVision);

    float stellaAlpha = m_stellaTimer > 0.f
        ? std::min(m_stellaTimer / 0.5f, 1.f) * std::min(1.f, m_stellaTimer)
        : 0.f;

    m_ui.render(m_renderer,
                m_totalPartsFound, TOTAL_PARTS,
                m_screenW, m_screenH,
                m_planetPartsFound, LAYOUTS[m_currentPlanet].partCount,
                m_currentPlanet, curPhysics(),
                &m_puzzle.rocks, &m_puzzle.parts, &m_puzzle.plates,
                m_player.pos, m_puzzle.baseEntrance.pos,
                m_map.getPixelWidth(), m_map.getPixelHeight(),
                minimapDisabled,
                gimmickActive,
                m_windWarning,
                m_stellaText, stellaAlpha);
}

int Game::tryGrabRock() {
    AABB pBox  = m_player.getAABB();
    AABB probe = {pBox.x - 16.f, pBox.y - 16.f, pBox.w + 32.f, pBox.h + 32.f};
    for (int i = 0; i < (int)m_puzzle.rocks.size(); i++) {
        if (!m_puzzle.rocks[i].active) continue;
        if (m_puzzle.rocks[i].getAABB().intersects(probe)) {
            m_grabOffset = m_puzzle.rocks[i].pos - m_player.pos;
            return i;
        }
    }
    return -1;
}

bool Game::isNearRock() const {
    AABB pBox  = m_player.getAABB();
    AABB probe = {pBox.x - 16.f, pBox.y - 16.f, pBox.w + 32.f, pBox.h + 32.f};
    for (const auto& rock : m_puzzle.rocks) {
        if (rock.active && rock.getAABB().intersects(probe)) return true;
    }
    return false;
}

void Game::checkMarsRockBoundaries() {
    static const float XMIN[]    = {64.f, 352.f, 352.f, 704.f, 704.f};
    static const float XMAX[]    = {320.f, 672.f, 672.f, 960.f, 960.f};
    static const float RESET_X[] = {192.f, 420.f, 620.f, 760.f, 920.f};
    static const float RESET_Y[] = {580.f, 580.f, 400.f, 580.f, 350.f};
    int count = std::min((int)m_puzzle.rocks.size(), 5);
    for (int i = 0; i < count; i++) {
        auto& rock = m_puzzle.rocks[i];
        if (!rock.active) continue;
        if (rock.pos.x < XMIN[i] || rock.pos.x > XMAX[i]) {
            rock.pos = {RESET_X[i], RESET_Y[i]};
            rock.vel = {};
            m_marsRockFlash[i] = 1.0f;
            if (m_grabbedRock == i) m_grabbedRock = -1;
        }
    }
}

void Game::renderBaseInterior() {
    SDL_SetRenderDrawColor(m_renderer, 16, 20, 28, 255);
    SDL_RenderClear(m_renderer);

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // Header bar
    {
        SDL_SetRenderDrawColor(m_renderer, 20, 24, 36, 255);
        SDL_FRect hdr = {0.f, 0.f, (float)m_screenW, 52.f};
        SDL_RenderFillRectF(m_renderer, &hdr);
        SDL_SetRenderDrawColor(m_renderer, 60, 90, 180, 200);
        SDL_FRect hdrLine = {0.f, 52.f, (float)m_screenW, 2.f};
        SDL_RenderFillRectF(m_renderer, &hdrLine);
    }

    // Floor
    SDL_SetRenderDrawColor(m_renderer, 30, 35, 46, 255);
    SDL_FRect flr = {0.f, 620.f, (float)m_screenW, 100.f};
    SDL_RenderFillRectF(m_renderer, &flr);

    // Left / right walls
    SDL_SetRenderDrawColor(m_renderer, 28, 32, 44, 255);
    SDL_FRect lwl = {0.f, 54.f, 42.f, 620.f};
    SDL_RenderFillRectF(m_renderer, &lwl);
    SDL_FRect rwl = {1238.f, 54.f, 42.f, 620.f};
    SDL_RenderFillRectF(m_renderer, &rwl);

    // Ceiling strip + lights
    SDL_SetRenderDrawColor(m_renderer, 40, 48, 64, 255);
    SDL_FRect ceil = {42.f, 54.f, 1196.f, 24.f};
    SDL_RenderFillRectF(m_renderer, &ceil);
    static const float LIGHTS[] = {160.f, 400.f, 640.f, 880.f, 1120.f};
    for (float lx : LIGHTS) {
        float gw = (std::sin(m_baseTimer * 1.8f + lx * 0.009f) + 1.f) * 0.5f;
        SDL_SetRenderDrawColor(m_renderer, 255, 248, 215, (Uint8)(18 + 14 * gw));
        SDL_FRect cone = {lx - 60.f, 54.f, 120.f, 120.f};
        SDL_RenderFillRectF(m_renderer, &cone);
        SDL_SetRenderDrawColor(m_renderer, 255, 250, 225, 210);
        SDL_FRect bulb = {lx - 18.f, 60.f, 36.f, 10.f};
        SDL_RenderFillRectF(m_renderer, &bulb);
    }

    // Warning board (left)
    const float boardX = 50.f, boardY = 110.f, boardW = 280.f, boardH = 250.f;
    {
        SDL_SetRenderDrawColor(m_renderer, 22, 20, 8, 245);
        SDL_FRect bg = {boardX, boardY, boardW, boardH};
        SDL_RenderFillRectF(m_renderer, &bg);
        SDL_SetRenderDrawColor(m_renderer, 200, 130, 20, 220);
        SDL_RenderDrawRectF(m_renderer, &bg);
        for (int i = 0; i < 10; i++) {
            Uint8 sc = (i % 2 == 0) ? 220 : 70;
            SDL_SetRenderDrawColor(m_renderer, sc, (Uint8)(sc * 0.58f), 0, 170);
            SDL_FRect st = {boardX + i * (boardW / 10.f), boardY, boardW / 10.f, 9.f};
            SDL_RenderFillRectF(m_renderer, &st);
            SDL_FRect sb = {boardX + i * (boardW / 10.f), boardY + boardH - 9.f, boardW / 10.f, 9.f};
            SDL_RenderFillRectF(m_renderer, &sb);
        }
        SDL_SetRenderDrawColor(m_renderer, 240, 200, 30, 200);
        SDL_FRect icon = {boardX + boardW*0.5f - 14.f, boardY + 18.f, 28.f, 28.f};
        SDL_RenderFillRectF(m_renderer, &icon);
        SDL_SetRenderDrawColor(m_renderer, 22, 20, 8, 230);
        SDL_FRect iconInner = {boardX + boardW*0.5f - 3.f, boardY + 24.f, 6.f, 14.f};
        SDL_RenderFillRectF(m_renderer, &iconInner);
    }

    // Warp gate (center)
    const float wgx = 640.f, wgy = 310.f;
    {
        bool active = m_puzzle.warpGate.active;
        float glow = active ? (std::sin(m_puzzle.warpGate.glowTimer * 3.5f) + 1.f) * 0.5f : 0.f;
        SDL_SetRenderDrawColor(m_renderer, 42, 46, 60, 255);
        SDL_FRect ped = {wgx - 60.f, wgy + 56.f, 120.f, 20.f};
        SDL_RenderFillRectF(m_renderer, &ped);
        SDL_SetRenderDrawColor(m_renderer, 60, 66, 88, 255);
        SDL_RenderDrawRectF(m_renderer, &ped);
        if (!active) {
            SDL_SetRenderDrawColor(m_renderer, 50, 50, 68, 200);
            SDL_FRect outer = {wgx-56.f, wgy-56.f, 112.f, 112.f};
            SDL_RenderFillRectF(m_renderer, &outer);
            SDL_SetRenderDrawColor(m_renderer, 38, 38, 52, 255);
            SDL_RenderDrawRectF(m_renderer, &outer);
            SDL_SetRenderDrawColor(m_renderer, 30, 30, 42, 220);
            SDL_FRect inner = {wgx-36.f, wgy-36.f, 72.f, 72.f};
            SDL_RenderFillRectF(m_renderer, &inner);
            SDL_SetRenderDrawColor(m_renderer, 55, 55, 72, 180);
            SDL_RenderDrawLineF(m_renderer, wgx, wgy-30.f, wgx, wgy+30.f);
            SDL_RenderDrawLineF(m_renderer, wgx-30.f, wgy, wgx+30.f, wgy);
        } else {
            SDL_SetRenderDrawColor(m_renderer, 60, 20, 180, (Uint8)(40 + 50 * glow));
            SDL_FRect aura = {wgx-80.f, wgy-80.f, 160.f, 160.f};
            SDL_RenderFillRectF(m_renderer, &aura);
            SDL_SetRenderDrawColor(m_renderer, 100, 40, 240, (Uint8)(180 + 60 * glow));
            SDL_FRect outer = {wgx-56.f, wgy-56.f, 112.f, 112.f};
            SDL_RenderFillRectF(m_renderer, &outer);
            SDL_SetRenderDrawColor(m_renderer, (Uint8)(130+100*glow), (Uint8)(50*glow), 255, 230);
            SDL_FRect inner = {wgx-38.f, wgy-38.f, 76.f, 76.f};
            SDL_RenderFillRectF(m_renderer, &inner);
            SDL_SetRenderDrawColor(m_renderer, 220, 180, 255, (Uint8)(200 + 55 * glow));
            SDL_FRect core = {wgx-16.f, wgy-16.f, 32.f, 32.f};
            SDL_RenderFillRectF(m_renderer, &core);
            for (int i = 0; i < 8; i++) {
                float a = m_puzzle.warpGate.particleTimer * 2.2f + i * 0.7854f;
                float pr = 66.f + 8.f * std::sin(m_puzzle.warpGate.glowTimer * 2.5f + i);
                float ppx = wgx + pr * std::cos(a);
                float ppy = wgy + pr * std::sin(a);
                SDL_SetRenderDrawColor(m_renderer, 200, 140, 255, (Uint8)(140 + 100 * glow));
                SDL_FRect dot = {ppx-4.f, ppy-4.f, 8.f, 8.f};
                SDL_RenderFillRectF(m_renderer, &dot);
            }
        }
    }

    // Parts panel (right)
    {
        int partCount = LAYOUTS[m_currentPlanet].partCount;
        const float px = 960.f, py = 110.f, pw = 280.f, ph = 250.f;
        SDL_SetRenderDrawColor(m_renderer, 18, 22, 32, 240);
        SDL_FRect panel = {px, py, pw, ph};
        SDL_RenderFillRectF(m_renderer, &panel);
        SDL_SetRenderDrawColor(m_renderer, 60, 90, 140, 200);
        SDL_RenderDrawRectF(m_renderer, &panel);
        for (int i = 0; i < partCount; i++) {
            float ix = px + 24.f + i * 56.f;
            float iy = py + ph * 0.55f;
            bool got = (i < m_planetPartsFound);
            if (got) {
                SDL_SetRenderDrawColor(m_renderer, 255, 210, 40, 255);
                fillDiamond(m_renderer, ix, iy, 12.f);
                SDL_SetRenderDrawColor(m_renderer, 255, 255, 160, 255);
                fillDiamond(m_renderer, ix, iy, 5.f);
            } else {
                SDL_SetRenderDrawColor(m_renderer, 55, 60, 80, 200);
                fillDiamond(m_renderer, ix, iy, 12.f);
                SDL_SetRenderDrawColor(m_renderer, 40, 44, 60, 255);
                fillDiamond(m_renderer, ix, iy, 5.f);
            }
        }
    }

    // Exit portal (bottom center)
    {
        float ex = 640.f, ey = 645.f;
        float pulse = (std::sin(m_baseTimer * 2.2f) + 1.f) * 0.5f;
        SDL_SetRenderDrawColor(m_renderer, 20, 50, 150, (Uint8)(70 + 55 * pulse));
        SDL_FRect eGlow = {ex - 80.f, ey - 26.f, 160.f, 52.f};
        SDL_RenderFillRectF(m_renderer, &eGlow);
        SDL_SetRenderDrawColor(m_renderer, 55, 110, 240, (Uint8)(155 + 65 * pulse));
        SDL_FRect eFrame = {ex - 56.f, ey - 18.f, 112.f, 36.f};
        SDL_RenderFillRectF(m_renderer, &eFrame);
        SDL_SetRenderDrawColor(m_renderer, 120, 185, 255, 255);
        SDL_RenderDrawRectF(m_renderer, &eFrame);
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    if (m_ui.getFont()) {
        // Header text
        const char* pName = Planets::ALL[m_currentPlanet].name;
        m_ui.renderText(m_renderer, m_ui.getFont(), pName,
                        m_screenW * 0.5f, 16.f, {180, 210, 255, 240}, true);
        {
            char buf[64];
            int partCount = LAYOUTS[m_currentPlanet].partCount;
            if (m_puzzle.warpGate.active)
                std::snprintf(buf, sizeof(buf), "게이트 활성화 완료!");
            else
                std::snprintf(buf, sizeof(buf), "부품 수집 (%d/%d)", m_planetPartsFound, partCount);
            SDL_Color hsc = m_puzzle.warpGate.active
                ? SDL_Color{100, 255, 160, 230} : SDL_Color{200, 190, 100, 210};
            m_ui.renderText(m_renderer, m_ui.getFont(), buf,
                            m_screenW * 0.5f + 180.f, 16.f, hsc, true);
        }

        // Board title + E-hint
        m_ui.renderText(m_renderer, m_ui.getFont(), "주의사항",
                        boardX + boardW * 0.5f, boardY + 60.f, {240, 200, 60, 230}, true);
        AABB boardProbe = {42.f, 100.f, 290.f, 260.f};
        if (m_player.getAABB().intersects(boardProbe)) {
            float blink = 0.6f + 0.4f * std::sin(m_baseTimer * 4.f);
            m_ui.renderText(m_renderer, m_ui.getFont(), "E: 읽기",
                            boardX + boardW * 0.5f, boardY + boardH + 16.f,
                            {255, 220, 80, (Uint8)(220 * blink)}, true);
        }

        // Warp gate label
        const char* gateLabel = m_puzzle.warpGate.active
            ? "워프 게이트 [활성]" : "워프 게이트 [비활성]";
        SDL_Color glc = m_puzzle.warpGate.active
            ? SDL_Color{180, 130, 255, 255} : SDL_Color{90, 90, 110, 190};
        m_ui.renderText(m_renderer, m_ui.getFont(), gateLabel, wgx, wgy + 80.f, glc, true);

        // E: 워프! prompt when active + near
        if (m_puzzle.warpGate.active) {
            AABB gateProbe = {560.f, 200.f, 160.f, 160.f};
            if (m_player.getAABB().intersects(gateProbe)) {
                float blink = 0.6f + 0.4f * std::sin(m_baseTimer * 5.f);
                m_ui.renderText(m_renderer, m_ui.getFont(), "E: 워프!",
                                wgx, wgy - 80.f,
                                {220, 180, 255, (Uint8)(240 * blink)}, true);
            }
        }

        // Parts panel title
        m_ui.renderText(m_renderer, m_ui.getFont(), "수집 부품",
                        960.f + 140.f, 110.f + 30.f, {160, 200, 255, 220}, true);

        // Exit label
        m_ui.renderText(m_renderer, m_ui.getFont(), "[ 외부로 나가기 ]",
                        640.f, 661.f, {100, 160, 255, 200}, true);
    }

    // Player
    m_player.render(m_renderer, 0.f, 0.f);

    // Board reading overlay
    if (m_baseReadingBoard && m_ui.getFont()) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 190);
        SDL_FRect dimmer = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
        SDL_RenderFillRectF(m_renderer, &dimmer);
        float ox = m_screenW * 0.5f - 280.f, oy = m_screenH * 0.5f - 130.f;
        float ow = 560.f, oh = 260.f;
        SDL_SetRenderDrawColor(m_renderer, 14, 12, 4, 245);
        SDL_FRect popup = {ox, oy, ow, oh};
        SDL_RenderFillRectF(m_renderer, &popup);
        SDL_SetRenderDrawColor(m_renderer, 220, 160, 20, 230);
        SDL_RenderDrawRectF(m_renderer, &popup);
        for (int i = 0; i < 10; i++) {
            Uint8 sc = (i % 2 == 0) ? 200 : 60;
            SDL_SetRenderDrawColor(m_renderer, sc, (Uint8)(sc * 0.58f), 0, 180);
            SDL_FRect st = {ox + i * (ow / 10.f), oy, ow / 10.f, 8.f};
            SDL_RenderFillRectF(m_renderer, &st);
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        m_ui.renderText(m_renderer, m_ui.getFont(), "주의사항",
                        ox + ow * 0.5f, oy + 24.f, {240, 200, 50, 240}, true);
        m_ui.renderText(m_renderer, m_ui.getFont(),
                        Planets::BASE_HINTS[m_currentPlanet],
                        ox + ow * 0.5f, oy + oh * 0.5f, {220, 215, 180, 230}, true);
        float blink = 0.55f + 0.45f * std::sin(m_baseTimer * 4.5f);
        m_ui.renderText(m_renderer, m_ui.getFont(), "[ E: 닫기 ]",
                        ox + ow * 0.5f, oy + oh - 28.f,
                        {180, 210, 255, (Uint8)(220 * blink)}, true);
    }
}

void Game::renderWarpActivation() {
    renderPlaying();

    float t = m_warpTimer;
    float alpha = std::min(t / 0.3f, 1.f);

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, (Uint8)(180 * alpha));
    SDL_FRect full = {0,0,(float)m_screenW,(float)m_screenH};
    SDL_RenderFillRectF(m_renderer, &full);

    float gx = (float)m_screenW * 0.5f;
    float gy = (float)m_screenH * 0.5f;
    for (int ring = 0; ring < 6; ring++) {
        float delay = ring * 0.4f;
        if (t < delay) continue;
        float rt = t - delay;
        float rad = rt * 200.f;
        float ra  = std::max(0.f, 1.f - rt * 0.6f) * alpha;
        SDL_SetRenderDrawColor(m_renderer, 120, 60, 255, (Uint8)(160 * ra));
        SDL_FRect rf = {gx-rad, gy-rad, rad*2.f, rad*2.f};
        SDL_RenderFillRectF(m_renderer, &rf);
    }
    if (t > 2.2f) {
        float flash = std::min((t - 2.2f) / 0.8f, 1.f);
        SDL_SetRenderDrawColor(m_renderer, 180, 140, 255, (Uint8)(200 * flash));
        SDL_RenderFillRectF(m_renderer, &full);
    }
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    m_ui.renderWarpActivation(m_renderer, m_screenW, m_screenH, curPhysics(), t);
}

void Game::renderEnding() {
    SDL_SetRenderDrawColor(m_renderer, 5, 8, 20, 255);
    SDL_RenderClear(m_renderer);

    // Warp star streaks (stars rushing downward)
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    srand(99);
    float warpSpeed = std::min(m_endingTimer * 30.f, 400.f);
    for (int i = 0; i < 80; i++) {
        float sx = (float)(rand() % m_screenW);
        float sy = (float)(rand() % m_screenH);
        float len = warpSpeed * 0.1f + 2.f;
        Uint8 sa = (Uint8)(60 + 80 * std::sin(m_endingTimer + i * 0.3f));
        SDL_SetRenderDrawColor(m_renderer, 200, 210, 255, sa);
        SDL_RenderDrawLineF(m_renderer, sx, sy, sx, sy + len);
    }
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    // Warp gate light burst at center
    if (m_endingTimer < 2.f) {
        float wt = m_endingTimer / 2.f;
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        for (int ring = 0; ring < 8; ring++) {
            float rad = ring * 40.f + wt * 100.f;
            float ra = std::max(0.f, 1.f - rad / 400.f);
            SDL_SetRenderDrawColor(m_renderer, 160, 100, 255, (Uint8)(120 * ra));
            SDL_FRect rf = {(float)m_screenW/2.f - rad,
                            (float)m_screenH/2.f - rad, rad*2.f, rad*2.f};
            SDL_RenderFillRectF(m_renderer, &rf);
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }

    // Ship flying up
    float shipY = m_screenH * 0.7f - m_endingTimer * 80.f;
    float shipX = m_screenW * 0.5f - 40.f;

    // Engine flames
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    for (int i = 1; i <= 10; i++) {
        float flicker = std::sin(m_endingTimer * 20.f + i) * 5.f;
        SDL_SetRenderDrawColor(m_renderer, 255, (Uint8)(160-i*12), 20, (Uint8)(200/i));
        SDL_FRect flame = {shipX+10.f+i*2.f+flicker, shipY+80.f+i*8.f, 60.f-i*4.f, 14.f};
        SDL_RenderFillRectF(m_renderer, &flame);
    }
    // Flame particles
    srand((int)(m_endingTimer * 40));
    for (int i = 0; i < 12; i++) {
        float px = shipX + 20.f + (rand() % 40);
        float py = shipY + 90.f + (rand() % 30);
        Uint8 pa = (Uint8)(100 + rand() % 80);
        SDL_SetRenderDrawColor(m_renderer, 255, 120, 20, pa);
        SDL_FRect dot = {px, py, 4.f, 4.f};
        SDL_RenderFillRectF(m_renderer, &dot);
    }
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    // Ship body
    SDL_SetRenderDrawColor(m_renderer, 180, 200, 230, 255);
    SDL_FRect body = {shipX+10.f, shipY+20.f, 60.f, 70.f};
    SDL_RenderFillRectF(m_renderer, &body);
    // Cockpit
    SDL_SetRenderDrawColor(m_renderer, 100, 200, 255, 220);
    SDL_FRect ck = {shipX+25.f, shipY+30.f, 30.f, 20.f};
    SDL_RenderFillRectF(m_renderer, &ck);
    // Wings
    SDL_SetRenderDrawColor(m_renderer, 140, 170, 220, 255);
    SDL_FRect wL = {shipX, shipY+50.f, 20.f, 25.f};
    SDL_FRect wR = {shipX+60.f, shipY+50.f, 20.f, 25.f};
    SDL_RenderFillRectF(m_renderer, &wL);
    SDL_RenderFillRectF(m_renderer, &wR);
    // Nose cone
    for (int i = 0; i < 5; i++) {
        float w = 60.f - i*10.f;
        SDL_SetRenderDrawColor(m_renderer, 200, 220, 255, 255);
        SDL_FRect nose = {shipX+10+(60.f-w)/2.f, shipY+5.f+i*3.f, w, 4.f};
        SDL_RenderFillRectF(m_renderer, &nose);
    }

    m_ui.renderEnding(m_renderer, m_screenW, m_screenH, m_endingTimer);
}
