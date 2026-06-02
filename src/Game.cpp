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

// Mars canyon bridge constants (shared by render + death logic)
static const float MARS_CANYON_X[3]    = {320.f, 672.f, 960.f};
static const float MARS_CANYON_W       = 32.f;
static const float MARS_BRIDGE_UPPER_Y = 208.f;  // upper 1/3 of map height (640/3≈213)
static const float MARS_BRIDGE_LOWER_Y = 432.f;  // lower 1/3 of map height (640*2/3≈427)
static const float MARS_BRIDGE_SAFE    = 22.f;   // ±22px from bridge center = safe zone
static const float MARS_BRIDGE_H       = 28.f;

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

// ---- Developer mode helpers ----

static const int   DEV_DISPLAY_ORDER[8] = {3, 0, 1, 2, 4, 5, 6, 7};
static const char* DEV_PLANET_NAMES[8]  = {
    "화성","수성","금성","지구","목성","토성","천왕성","해왕성"
};

struct DevBtn {
    SDL_FRect   rect;
    std::string label;
    int action;  // -1=solar, 0-7=planet surface, 10-17=planet base, 100=ending
};

static std::vector<DevBtn> buildDevMenu(int sw, int sh) {
    std::vector<DevBtn> btns;
    const float PW  = 440.f, PH = 510.f;
    const float PX  = (sw - PW) * 0.5f;
    const float PY  = (sh - PH) * 0.5f;
    const float BH  = 34.f, GAP = 7.f;
    const float BWF = PW - 20.f;
    const float BWH = (PW - 30.f) * 0.5f;
    const float LX  = PX + 10.f;
    const float RX  = PX + 10.f + BWH + 10.f;
    float y = PY + 52.f;

    DevBtn b;
    b.rect = {LX, y, BWF, BH}; b.label = "태양계 맵"; b.action = -1;
    btns.push_back(b);
    y += BH + GAP;

    for (int i = 0; i < 8; i++) {
        char la[32], lb[32];
        std::snprintf(la, sizeof(la), "%s 외부", DEV_PLANET_NAMES[i]);
        std::snprintf(lb, sizeof(lb), "%s 기지", DEV_PLANET_NAMES[i]);
        DevBtn bl, br;
        bl.rect = {LX, y, BWH, BH}; bl.label = la; bl.action = i;
        br.rect = {RX, y, BWH, BH}; br.label = lb; br.action = i + 10;
        btns.push_back(bl);
        btns.push_back(br);
        y += BH + GAP;
    }
    DevBtn e;
    e.rect = {LX, y, BWF, BH}; e.label = "엔딩"; e.action = 100;
    btns.push_back(e);
    return btns;
}

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
        // Log files (glowing data chips)
        m_puzzle.addLogFile(160.f, 350.f, 0);
        m_puzzle.addLogFile(490.f, 300.f, 1);
        m_puzzle.addLogFile(780.f, 250.f, 2);
        // Energy drink
        m_puzzle.addEnergyDrink(500.f, 180.f);
        // Meteor-triggered bonus puzzle (goal zone): plate + small door + energy drink
        m_puzzle.addPressurePlate(1000.f, 555.f, 48.f, 16.f, -1);  // meteor-only (no door link via PuzzleSystem)
        m_puzzle.addDoor(1012.f, 390.f, 32.f, 196.f);               // door 3 = meteor bonus door
        m_puzzle.addEnergyDrink(1018.f, 366.f);                     // bonus energy drink behind door 3
        m_marsMeteorPlateIdx = (int)m_puzzle.plates.size() - 1;
        m_marsMeteorites.clear();
        m_marsNextMeteor = 5.f + (float)(rand() % 30) / 10.f;
        m_marsMeteorDoorOpen = false;
        // Reset Mars log state
        for (int i = 0; i < 3; i++) m_marsLogsCollected[i] = false;
        m_marsLogReading   = -1;
        m_marsArchiveOpen  = false;
        m_marsArchiveSel   = 0;

        // Debug: print bridge positions
        SDL_Log("[Mars] Canyon bridges generated:");
        for (int ci = 0; ci < 3; ci++) {
            SDL_Log("  Canyon %d (x=%.0f): upper bridge y=%.0f  lower bridge y=%.0f  safe zone +/-%.0f",
                    ci, MARS_CANYON_X[ci], MARS_BRIDGE_UPPER_Y, MARS_BRIDGE_LOWER_Y, MARS_BRIDGE_SAFE);
        }

        m_player.pos = {80.f, 548.f};
        m_camX = m_camY = 0.f;
        m_marsZoneShown[0] = true;
        m_marsZoneTextIdx   = 0;
        m_marsZoneTextTimer = 4.f;
        m_stellaText  = "바위를 밀어서 압력판에 올려봐";
        m_stellaTimer = 5.f;
        applyGimmickToPlayer();
        int stage = std::min((m_totalPartsFound * 3) / TOTAL_PARTS, 3);
        m_ui.setShipStage(stage);
        return;
    }

    // Venus (1): maze-like corridors + toxic clouds
    if (idx == 1) {
        // Rocks (can block toxic clouds, 2s stun)
        m_puzzle.addRock(350.f, 490.f, 5.f);   // Rock A: left side
        m_puzzle.addRock(750.f, 490.f, 5.f);   // Rock B: right side

        // Single plate + canyon door
        m_puzzle.addPressurePlate(492.f, 372.f, 64.f, 20.f, 0);
        m_puzzle.addDoor(544.f, 256.f, 96.f, 64.f);  // door 0: canyon passage

        // Maze dead-end walls (doors 1-3: permanent, no plates linked)
        m_puzzle.addDoor(192.f, 300.f, 20.f, 220.f);  // door 1: far-left dead end
        m_puzzle.addDoor(940.f, 300.f, 20.f, 220.f);  // door 2: far-right dead end
        m_puzzle.addDoor(476.f, 440.f, 128.f, 20.f);  // door 3: center horizontal block

        // Parts in upper zone
        m_puzzle.addPart(200.f, 128.f);
        m_puzzle.addPart(900.f, 128.f);

        m_puzzle.setWarpGate(1096.f, 480.f);
        m_puzzle.setBaseEntrance(1096.f, 480.f);

        // Energy drinks near cloud patrol zones
        m_puzzle.addEnergyDrink(380.f, 240.f);
        m_puzzle.addEnergyDrink(820.f, 240.f);

        // Toxic clouds: (startPos, endPos, speed)
        m_venusClouds.clear();
        auto addCloud = [&](float x0,float y0,float x1,float y1,float spd){
            ToxicCloud c;
            c.startPos={x0,y0}; c.endPos={x1,y1};
            c.pos=c.startPos; c.speed=spd; c.t=0.f;
            m_venusClouds.push_back(c);
        };
        addCloud(220.f, 400.f, 550.f, 400.f, 0.20f);  // Cloud 1: lower-left horizontal
        addCloud(650.f, 400.f, 960.f, 400.f, 0.22f);  // Cloud 2: lower-right horizontal
        addCloud(300.f, 180.f, 700.f, 180.f, 0.18f);  // Cloud 3: upper horizontal
        addCloud(576.f, 310.f, 576.f, 500.f, 0.16f);  // Cloud 4: center vertical
        addCloud(100.f, 300.f, 100.f, 520.f, 0.14f);  // Cloud 5: far-left vertical

        m_player.pos = {80.f, 544.f};
        m_camX = m_camY = 0.f;

        m_stellaText  = "앞이 잘 안 보여... 천천히 가야겠어.";
        m_stellaTimer = 4.f;
        m_ui.showNotification("독성 구름이 있어! 바위로 막을 수 있을 것 같아.", NotifType::Warning);

        applyGimmickToPlayer();
        int stgV = std::min((m_totalPartsFound * 3) / TOTAL_PARTS, 3);
        m_ui.setShipStage(stgV);
        return;
    }

    // Jupiter (4): vortex-reversal puzzle
    // Vortex at (576,455). Rocks pushed toward vortex get pulled in,
    // then stopped by pocket walls just outside the plates.
    if (idx == 4) {
        m_puzzle.addDoor(576.f, 256.f, 64.f, 64.f);   // door 0: canyon (opens when both plates pressed)
        m_puzzle.addDoor(414.f, 418.f, 20.f, 72.f);   // door 1: right-catch wall for Plate A
        m_puzzle.addDoor(698.f, 418.f, 20.f, 72.f);   // door 2: left-catch wall for Plate B

        // Plates inside vortex warning zone — rocks roll in from outside and stop here
        m_puzzle.addPressurePlate(350.f, 448.f, 64.f, 20.f, 0);  // Plate A: left (x=350–414)
        m_puzzle.addPressurePlate(718.f, 448.f, 64.f, 20.f, 0);  // Plate B: right (x=718–782)

        // Rocks: start between map edge and plate; push toward vortex → wall catches them
        m_puzzle.addRock(200.f, 460.f, 15.f);   // Rock A: push RIGHT → stops on Plate A
        m_puzzle.addRock(950.f, 460.f, 15.f);   // Rock B: push LEFT  → stops on Plate B
        m_puzzle.addRock(290.f, 350.f, 15.f);   // Rock C: utility
        m_puzzle.addRock(870.f, 350.f, 15.f);   // Rock D: utility

        // Parts (upper zone)
        m_puzzle.addPart(200.f, 128.f);
        m_puzzle.addPart(900.f, 128.f);

        m_puzzle.setWarpGate(1096.f, 480.f);
        m_puzzle.setBaseEntrance(1096.f, 480.f);

        // Energy drinks near vortex
        m_puzzle.addEnergyDrink(576.f, 342.f);
        m_puzzle.addEnergyDrink(450.f, 455.f);
        m_puzzle.addEnergyDrink(700.f, 455.f);

        m_jupiterWindCycle   = 0.f;
        m_jupiterWindDir     = 0;
        m_jupiterWindActive  = true;
        m_jupiterWindWarning = false;
        m_jupiterPrevWarning = false;
        m_jupiterVortexTimer = 0.f;
        m_jupiterVortexWarn  = false;
        m_jupiterVortexDanger = false;
        m_jupiterHints       = 0;

        m_player.pos = {80.f, 544.f};
        m_camX = m_camY = 0.f;

        m_stellaText  = "중력이 너무 강해... 움직이기 힘들어.";
        m_stellaTimer = 4.f;

        applyGimmickToPlayer();
        int stg = std::min((m_totalPartsFound * 3) / TOTAL_PARTS, 3);
        m_ui.setShipStage(stg);
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

    // Mercury: energy cells + save initial rock positions for R-reset
    if (idx == 0) {
        m_puzzle.addEnergyCell(192.f, 160.f);
        m_puzzle.addEnergyCell(576.f, 192.f);
        m_puzzle.addEnergyCell(896.f, 160.f);
        m_initialRockPos.clear();
        for (const auto& rock : m_puzzle.rocks)
            m_initialRockPos.push_back(rock.pos);
        m_energyCellsFound = 0;
        m_resetFade  = 0.f;
        m_resetState = 0;
        m_mercuryDayCycle       = 0.f;
        m_mercurySolarCycle     = 0.f;
        m_mercurySolarWarning   = false;
        m_mercurySolarFlareActive = false;
        m_solarFlareTimer       = 0.f;
        m_solarFlareTint        = 0.f;
        m_solarBeams.clear();
        // Energy drinks
        m_puzzle.addEnergyDrink(288.f, 430.f);
        m_puzzle.addEnergyDrink(832.f, 400.f);
        // Heat cracks (death zones)
        m_heatCracks.clear();
        m_heatCracks.push_back({450.f, 455.f, 22.f, 14.f});  // rock0 can bridge this
        m_heatCracks.push_back({700.f, 460.f, 60.f, 14.f});  // platform 1 spans this
        m_heatCracks.push_back({880.f, 510.f, 26.f, 14.f});  // platform 2 spans this
        // Unstable platforms
        m_puzzle.addUnstablePlatform(461.f, 448.f, 64.f, 14.f);  // spans crack 1 before rock bridge
        m_puzzle.addUnstablePlatform(730.f, 453.f, 64.f, 14.f);  // spans crack 2
        m_puzzle.addUnstablePlatform(893.f, 503.f, 64.f, 14.f);  // spans crack 3
    } else {
        m_heatCracks.clear();
    }

    // Energy drinks per planet (not Mars/Mercury, handled above)
    // Venus=1, Earth=2, Jupiter=4, Saturn=5, Uranus=6, Neptune=7
    static const struct { int cnt; float xs[5]; float ys[5]; } DRINK_LAYOUT[8] = {
        {0,{},{} },  // 0 Mercury (handled above)
        {2,{224,864},{192,192}},  // 1 Venus
        {3,{160,576,960},{160,160,160}},  // 2 Earth
        {0,{},{} },  // 3 Mars (handled above)
        {3,{224,640,960},{160,160,160}},  // 4 Jupiter
        {4,{160,448,832,1056},{192,192,192,192}},  // 5 Saturn
        {4,{192,480,768,1024},{160,192,160,192}},  // 6 Uranus
        {5,{128,320,640,896,1024},{160,192,160,192,160}},  // 7 Neptune
    };
    if (idx < 8) {
        for (int di = 0; di < DRINK_LAYOUT[idx].cnt; di++)
            m_puzzle.addEnergyDrink(DRINK_LAYOUT[idx].xs[di], DRINK_LAYOUT[idx].ys[di]);
    }

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

    // Mercury: energy cells determine how many planets to unlock
    if (m_currentPlanet == 0) {
        // VISIT_ORDER: {3,0,1,2,4,5,6,7} — Mercury is index 1, next is Venus(1), Earth(2), Jupiter(4)
        int unlockCount = std::max(1, m_energyCellsFound);
        for (int u = 0; u < unlockCount && u < 3; u++) {
            int nextVisitIdx = 2 + u;  // positions 2,3,4 in VISIT_ORDER
            if (nextVisitIdx < Planets::COUNT)
                m_solarMap.setPlanetUnlocked(Planets::VISIT_ORDER[nextVisitIdx]);
        }
        for (int v = 0; v < Planets::COUNT; v++) {
            if (Planets::VISIT_ORDER[v] == m_currentPlanet) {
                m_visitProgress = v + 1;
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
        return;
    }

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
            // Developer mode toggle (F1 works in any scene)
            if (sym == SDLK_F1) {
                m_devMenuOpen = !m_devMenuOpen;
                continue;
            }
            // ESC closes dev menu first
            if (sym == SDLK_ESCAPE && m_devMenuOpen) {
                m_devMenuOpen = false;
                continue;
            }
            if ((sym == SDLK_RETURN || sym == SDLK_e) && m_scene == Scene::Playing
                && m_marsLogReading >= 0) {
                m_marsLogReading = -1;
            }
            if (sym == SDLK_e && m_scene == Scene::Playing && m_marsLogReading < 0) {
                if (m_grabbedRock >= 0) {
                    m_grabbedRock = -1;
                } else {
                    m_grabbedRock = tryGrabRock();
                }
            }
            if (sym == SDLK_r && m_scene == Scene::Playing
                && m_currentPlanet == 0 && m_resetState == 0) {
                m_resetState = 1;
            }
            // SPACE: jump (immune to ground hazards for JUMP_DURATION seconds)
            if (sym == SDLK_SPACE && m_scene == Scene::Playing
                && m_marsLogReading < 0 && m_deathState == 0
                && m_player.jumpCooldown <= 0.f) {
                // Can't jump if already on a death zone (already fallen)
                bool onDanger = false;
                if (m_currentPlanet == 0) {
                    AABB pa = m_player.getAABB();
                    for (const auto& crack : m_heatCracks)
                        if (pa.intersects(crack)) { onDanger = true; break; }
                }
                if (m_currentPlanet == 3) {
                    float pcx = m_player.pos.x, pcy = m_player.pos.y;
                    for (int i = 0; i < 3 && !onDanger; i++) {
                        if (i >= (int)m_puzzle.doors.size() || !m_puzzle.doors[i].open) continue;
                        if (pcx >= MARS_CANYON_X[i] && pcx <= MARS_CANYON_X[i] + MARS_CANYON_W) {
                            bool onUpper = std::abs(pcy - MARS_BRIDGE_UPPER_Y) <= MARS_BRIDGE_SAFE;
                            bool onLower = std::abs(pcy - MARS_BRIDGE_LOWER_Y) <= MARS_BRIDGE_SAFE;
                            if (!onUpper && !onLower) onDanger = true;
                        }
                    }
                }
                if (!onDanger) {
                    const Uint8* ks = SDL_GetKeyboardState(nullptr);
                    Vec2 jdir{};
                    if (ks[SDL_SCANCODE_W] || ks[SDL_SCANCODE_UP])    jdir.y -= 1.f;
                    if (ks[SDL_SCANCODE_S] || ks[SDL_SCANCODE_DOWN])  jdir.y += 1.f;
                    if (ks[SDL_SCANCODE_A] || ks[SDL_SCANCODE_LEFT])  jdir.x -= 1.f;
                    if (ks[SDL_SCANCODE_D] || ks[SDL_SCANCODE_RIGHT]) jdir.x += 1.f;
                    if (jdir.length() < 0.01f) {
                        // no key held: jump in facing direction
                        float rad = m_player.facing * 3.14159f / 180.f;
                        jdir = {std::cos(rad), std::sin(rad)};
                    }
                    m_player.startJump(jdir);
                }
            }
            if (sym == SDLK_e && m_scene == Scene::BaseInterior) {
                if (m_marsArchiveOpen) {
                    m_marsArchiveOpen = false;
                } else if (m_baseReadingBoard) {
                    m_baseReadingBoard = false;
                } else {
                    // Check warp gate proximity (E to warp)
                    AABB gateProbe = {560.f, 200.f, 160.f, 160.f};
                    if (m_puzzle.warpGate.active && m_player.getAABB().intersects(gateProbe)) {
                        m_puzzle.warpGate.triggered = true;
                        onPlanetCleared();
                        return;
                    }
                    // Mars: archive panel (right side)
                    if (m_currentPlanet == 3) {
                        AABB archiveProbe = {960.f, 100.f, 280.f, 260.f};
                        if (m_player.getAABB().intersects(archiveProbe)) {
                            int collected = 0;
                            for (int i = 0; i < 3; i++) if (m_marsLogsCollected[i]) collected++;
                            if (collected > 0) { m_marsArchiveOpen = true; m_marsArchiveSel = 0; }
                        }
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
            if (m_scene == Scene::GameOver &&
               (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_ESCAPE)) {
                m_lives = 3;
                m_deathFade = 0.f;
                m_deathState = 0;
                m_titleTimer = 0.f;
                m_scene = Scene::Title;
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            if (m_devMenuOpen) {
                handleDevMenuClick(e.button.x, e.button.y);
                continue;
            }
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
        case Scene::GameOver:       updateGameOver(dt);       break;
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
    // Death state machine
    if (m_deathState == 1) {
        m_deathFade += dt * 2.5f;
        if (m_deathFade >= 1.f) {
            m_deathFade = 1.f;
            if (m_lives <= 0) {
                m_scene = Scene::GameOver;
                m_gameOverTimer = 0.f;
                m_deathState = 0;
                return;
            }
            m_player.pos = m_respawnPos;
            m_player.vel = {};
            m_deathState = 2;
        }
        m_ui.update(dt);
        return;
    }
    if (m_deathState == 2) {
        m_deathFade -= dt * 2.f;
        if (m_deathFade <= 0.f) { m_deathFade = 0.f; m_deathState = 0; }
    }

    updateGimmicks(dt);

    auto walls = collectWalls();
    // Block all player input while a log popup is open
    static const Uint8 s_zeroKeys[512] = {};
    bool logOpen = (m_currentPlanet == 3 && m_marsLogReading >= 0);
    const Uint8* keys = logOpen ? s_zeroKeys : SDL_GetKeyboardState(nullptr);
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
        m_ui.showNotification(msg, NotifType::Normal);

        if (m_planetPartsFound >= LAYOUTS[m_currentPlanet].partCount) {
            m_puzzle.activateWarpGate();
            m_ui.showNotification("기지로 돌아가 워프 게이트를 활성화하자!", NotifType::Warning);
        }
    }

    // Mars: collect log files
    if (m_currentPlanet == 3) {
        int logId = m_puzzle.tryCollectLog(m_player.getAABB());
        if (logId >= 0 && logId < 3 && !m_marsLogsCollected[logId]) {
            m_marsLogsCollected[logId] = true;
            m_marsLogReading = logId;
        }
    }

    // Mercury: collect energy cells + handle R-reset fade
    if (m_currentPlanet == 0) {
        int cellIdx = m_puzzle.tryCollectCell(m_player.getAABB());
        if (cellIdx >= 0) {
            m_energyCellsFound++;
            m_ui.showNotification("에너지 셀 획득!", NotifType::Normal);
        }

        // Reset state machine: 1=fade out, 2=restore+fade in
        if (m_resetState == 1) {
            m_resetFade += dt * 3.f;
            if (m_resetFade >= 1.f) {
                m_resetFade = 1.f;
                // Restore rocks to initial positions
                for (int i = 0; i < (int)m_puzzle.rocks.size() && i < (int)m_initialRockPos.size(); i++) {
                    m_puzzle.rocks[i].pos = m_initialRockPos[i];
                    m_puzzle.rocks[i].vel = {};
                }
                m_resetState = 2;
            }
        } else if (m_resetState == 2) {
            m_resetFade -= dt * 3.f;
            if (m_resetFade <= 0.f) {
                m_resetFade  = 0.f;
                m_resetState = 0;
            }
        }
    }

    // Venus toxic cloud update
    if (m_currentPlanet == 1 && m_deathState == 0)
        updateVenusClouds(dt);

    // Mars meteor shower update
    if (m_currentPlanet == 3 && m_deathState == 0)
        updateMarsMeteorites(dt);

    // Mercury solar flare update
    if (m_currentPlanet == 0 && m_deathState == 0)
        updateMercurySolarFlare(dt);

    // Mars zone transition hints + boundary auto-reset
    if (m_currentPlanet == 3) {
        if (!m_marsZoneShown[1] && m_player.pos.x > 354.f) {
            m_marsZoneShown[1] = true;
            m_marsZoneTextIdx   = 1;
            m_marsZoneTextTimer = 4.f;
            m_stellaText  = "이번엔 당겨야 해! E키로 바위를 잡아봐";
            m_stellaTimer = 5.f;
        }
        if (!m_marsZoneShown[2] && m_player.pos.x > 706.f) {
            m_marsZoneShown[2] = true;
            m_marsZoneTextIdx   = 2;
            m_marsZoneTextTimer = 4.f;
            m_stellaText  = "밀기+당기기 조합으로 풀어야 해!";
            m_stellaTimer = 4.5f;
        }
        if (m_marsZoneTextTimer > 0.f) m_marsZoneTextTimer -= dt;
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

    // Energy drink collection (all planets)
    {
        int drinkIdx = m_puzzle.tryCollectDrink(m_player.getAABB());
        if (drinkIdx >= 0) {
            if (m_lives < 3) m_lives++;
            m_ui.showNotification("에너지 드링크! 목숨 +1", NotifType::Normal);
        }
    }

    // Mercury: day cycle + unstable platform death + heat crack death
    if (m_currentPlanet == 0) {
        m_mercuryDayCycle += dt;

        if (m_puzzle.updateUnstablePlatforms(dt, m_player.getAABB())) {
            loseLife();
        }
        if (!m_player.isAirborne()) {
            AABB pa = m_player.getAABB();
            for (const auto& crack : m_heatCracks) {
                if (pa.intersects(crack)) { loseLife(); break; }
            }
        }
    }

    // Mars: canyon death — safe only on bridges, skip if airborne
    if (m_currentPlanet == 3 && m_deathState == 0 && !m_player.isAirborne()) {
        float pcx = m_player.pos.x;
        float pcy = m_player.pos.y;
        for (int i = 0; i < 3 && i < (int)m_puzzle.doors.size(); i++) {
            if (!m_puzzle.doors[i].open) continue;
            if (pcx >= MARS_CANYON_X[i] && pcx <= MARS_CANYON_X[i] + MARS_CANYON_W) {
                bool onUpper = std::abs(pcy - MARS_BRIDGE_UPPER_Y) <= MARS_BRIDGE_SAFE;
                bool onLower = std::abs(pcy - MARS_BRIDGE_LOWER_Y) <= MARS_BRIDGE_SAFE;
                if (!onUpper && !onLower) { loseLife(); break; }
            }
        }
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

    if (gimmick == PlanetGimmick::HeavyGrav) {
        updateJupiterGimmick(dt);
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
    for (const auto& up : m_puzzle.unstablePlatforms)
        if (up.state != 2) walls.push_back(up.getAABB());
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

static void drawHeart(SDL_Renderer* r, float cx, float cy, float s) {
    float r1 = s * 0.4f;
    fillCircle(r, cx - r1*0.72f, cy - r1*0.35f, r1);
    fillCircle(r, cx + r1*0.72f, cy - r1*0.35f, r1);
    for (float dy = -r1*0.1f; dy <= s*0.72f; dy += 1.f) {
        float hw = (s*0.72f - dy) * 0.88f;
        if (hw > 0.f) SDL_RenderDrawLineF(r, cx - hw, cy + dy, cx + hw, cy + dy);
    }
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
        case Scene::GameOver:       renderGameOver();       break;
        case Scene::Ending:         renderEnding();         break;
    }

    // Developer mode: always-visible hint + menu overlay
    if (m_ui.getFont()) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 80);
        SDL_FRect hintBg = {(float)m_screenW - 186.f, (float)m_screenH - 28.f, 180.f, 22.f};
        SDL_RenderFillRectF(m_renderer, &hintBg);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        m_ui.renderText(m_renderer, m_ui.getFont(), "[F1: 개발자 모드]",
                        (float)m_screenW - 96.f, (float)m_screenH - 24.f,
                        {120, 130, 160, 150}, true);
    }
    renderDevMenu();

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

// Draw arrow showing wind direction (file-static helper)
static void drawWindArrow(SDL_Renderer* r,
                          float x, float y, float dirX, float dirY,
                          float sz, Uint8 a) {
    SDL_SetRenderDrawColor(r, 255, 200, 50, a);
    float ex = x + dirX * sz, ey = y + dirY * sz;
    SDL_RenderDrawLineF(r, x, y, ex, ey);
    float px = -dirY * sz * 0.35f, py = dirX * sz * 0.35f;
    SDL_RenderDrawLineF(r, ex, ey, ex - dirX*sz*0.4f + px, ey - dirY*sz*0.4f + py);
    SDL_RenderDrawLineF(r, ex, ey, ex - dirX*sz*0.4f - px, ey - dirY*sz*0.4f - py);
}

void Game::renderPlaying() {
    m_map.render(m_renderer, m_camX, m_camY);

    // Jupiter: orange/brown atmospheric band overlay
    if (m_currentPlanet == 4) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        struct Band { Uint8 r, g, b, a; };
        static const Band BANDS[] = {
            {190, 100, 45, 22}, {225, 138, 65, 18}, {255, 168, 85, 14},
            {205, 108, 55, 20}, {245, 150, 78, 16}, {175, 88,  38, 24},
            {235, 125, 60, 18}, {200, 105, 50, 20}, {250, 155, 80, 14},
        };
        const int NB = 9;
        float bh = (float)m_screenH / NB;
        for (int i = 0; i < NB; i++) {
            float by = (float)i * bh;
            SDL_SetRenderDrawColor(m_renderer, BANDS[i].r, BANDS[i].g, BANDS[i].b, BANDS[i].a);
            SDL_FRect band = {0.f, by, (float)m_screenW, bh + 1.f};
            SDL_RenderFillRectF(m_renderer, &band);
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }

    // Mercury: day/night cycle overlay
    if (m_currentPlanet == 0) {
        float phase = std::fmod(m_mercuryDayCycle, 30.f) / 30.f;
        float night = 0.5f * (1.f - std::cos(phase * 6.28318f));
        Uint8 dimA = (Uint8)(150 * night);
        if (dimA > 0) {
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 0, 0, 25, dimA);
            SDL_FRect full = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
            SDL_RenderFillRectF(m_renderer, &full);
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        }
        // Heat cracks (dark fissures with red glow)
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        for (const auto& crack : m_heatCracks) {
            float cx2 = crack.x - m_camX, cy2 = crack.y - m_camY;
            float glow = (std::sin(m_titleTimer * 4.f) + 1.f) * 0.5f;
            SDL_SetRenderDrawColor(m_renderer, (Uint8)(180 + 50*glow), 30, 5, (Uint8)(60 + 40*glow));
            SDL_FRect grf = {cx2 - 3.f, cy2 - 3.f, crack.w + 6.f, crack.h + 6.f};
            SDL_RenderFillRectF(m_renderer, &grf);
            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 240);
            SDL_FRect rf = {cx2, cy2, crack.w, crack.h};
            SDL_RenderFillRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, (Uint8)(220 * glow), (Uint8)(40 * glow), 0, (Uint8)(100 + 80*glow));
            SDL_RenderDrawRectF(m_renderer, &rf);
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }

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

    // Mars: 붉은 협곡 시각화
    if (m_currentPlanet == 3) {
        static const float CANYON_WORLD_X[] = {320.f, 672.f, 960.f};
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

        for (float cwx : CANYON_WORLD_X) {
            float sx = cwx - m_camX;
            float sh = (float)m_screenH;

            // 협곡 깊이 (중앙 검정 → 가장자리 어두운 붉은색)
            for (int layer = 0; layer < 10; layer++) {
                float t   = (float)layer / 9.f;          // 0=center, 1=edge
                float hw  = 6.f + 30.f * t;              // center narrow → edge wide
                Uint8 rv  = (Uint8)(80.f  * t);
                Uint8 gv  = (Uint8)(6.f   * t);
                Uint8 bv  = (Uint8)(4.f   * t);
                Uint8 av  = (Uint8)(230.f - 30.f * t);
                SDL_SetRenderDrawColor(m_renderer, rv, gv, bv, av);
                SDL_FRect strip = {sx - hw, 0.f, hw * 2.f, sh};
                SDL_RenderFillRectF(m_renderer, &strip);
            }

            // 바위 질감 가장자리 (왼/오른 각 8px)
            for (int ry2 = 0; ry2 < (int)sh; ry2 += 16) {
                float noise = std::sin(ry2 * 0.4f + cwx * 0.05f) * 4.f;
                Uint8 tc = (Uint8)(55 + 30 * std::sin(ry2 * 0.25f));
                SDL_SetRenderDrawColor(m_renderer, tc, (Uint8)(tc * 0.3f), (Uint8)(tc * 0.2f), 200);
                SDL_FRect lrk = {sx - 38.f + noise, (float)ry2, 10.f, 8.f};
                SDL_FRect rrk = {sx + 28.f - noise, (float)ry2, 10.f, 8.f};
                SDL_RenderFillRectF(m_renderer, &lrk);
                SDL_RenderFillRectF(m_renderer, &rrk);
            }

            // 용암 흐름 파티클 (아래로)
            srand((int)(m_titleTimer * 18) ^ (int)cwx);
            for (int p = 0; p < 18; p++) {
                float speed = 40.f + (rand() % 40);
                float py2   = std::fmod(m_titleTimer * speed + p * 37.f, sh + 20.f) - 10.f;
                float px2   = sx - 10.f + (float)(rand() % 21);
                float br    = (float)(rand() % 100) / 100.f;
                Uint8 pr2   = (Uint8)(180 + 70 * br);
                Uint8 pg    = (Uint8)(50  + 50 * br);
                Uint8 pa2   = (Uint8)(120 + 80 * br);
                SDL_SetRenderDrawColor(m_renderer, pr2, pg, 10, pa2);
                SDL_FRect dot = {px2, py2, 3.f, 4.f};
                SDL_RenderFillRectF(m_renderer, &dot);
            }

            // 협곡 가장자리 열기 파티클 (위로)
            srand((int)(m_titleTimer * 22) ^ (int)cwx ^ 999);
            for (int p = 0; p < 10; p++) {
                float speed = 30.f + (rand() % 25);
                float py2   = sh - std::fmod(m_titleTimer * speed + p * 64.f, sh + 30.f);
                float ex    = sx - 34.f + (float)(rand() % 6);
                Uint8 ea    = (Uint8)(40 + 60 * std::sin(m_titleTimer * 4.f + p));
                SDL_SetRenderDrawColor(m_renderer, 220, 60, 20, ea);
                SDL_FRect edL = {ex, py2, 2.f, 2.f};
                SDL_RenderFillRectF(m_renderer, &edL);
                ex = sx + 32.f + (float)(rand() % 6);
                SDL_FRect edR = {ex, py2, 2.f, 2.f};
                SDL_RenderFillRectF(m_renderer, &edR);
            }
        }

        // 협곡 돌다리: 각 협곡마다 상단 1/3, 하단 1/3 지점에 1개씩, 총 2개
        static const float BRIDGE_CENTERS[] = {MARS_BRIDGE_UPPER_Y, MARS_BRIDGE_LOWER_Y};
        static const float BRIDGE_W = 40.f;  // 4px overhang on each side of 32px canyon
        for (int ci = 0; ci < 3; ci++) {
            float bwx = CANYON_WORLD_X[ci] - 4.f - m_camX;  // 4px left of canyon edge
            for (float bcy : BRIDGE_CENTERS) {
                float bwy = bcy - MARS_BRIDGE_H * 0.5f - m_camY;
                // Pulsing warm glow behind bridge
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
                Uint8 ga = (Uint8)(80 + 40 * std::sin(m_titleTimer * 4.f));
                SDL_SetRenderDrawColor(m_renderer, 255, 210, 100, ga);
                SDL_FRect glow = {bwx - 4.f, bwy - 3.f, BRIDGE_W + 8.f, MARS_BRIDGE_H + 6.f};
                SDL_RenderFillRectF(m_renderer, &glow);
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
                // Bridge body — bright warm sandy stone
                SDL_SetRenderDrawColor(m_renderer, 205, 175, 125, 255);
                SDL_FRect bridge = {bwx, bwy, BRIDGE_W, MARS_BRIDGE_H};
                SDL_RenderFillRectF(m_renderer, &bridge);
                // Stone texture lines
                SDL_SetRenderDrawColor(m_renderer, 170, 140, 90, 220);
                for (float ly = bwy + 9.f; ly < bwy + MARS_BRIDGE_H; ly += 9.f)
                    SDL_RenderDrawLineF(m_renderer, bwx + 3.f, ly, bwx + BRIDGE_W - 3.f, ly);
                // Top highlight (lighter edge)
                SDL_SetRenderDrawColor(m_renderer, 235, 210, 165, 255);
                SDL_RenderDrawLineF(m_renderer, bwx, bwy, bwx + BRIDGE_W, bwy);
                SDL_RenderDrawLineF(m_renderer, bwx, bwy + 1.f, bwx + BRIDGE_W, bwy + 1.f);
                // Border
                SDL_SetRenderDrawColor(m_renderer, 130, 100, 60, 255);
                SDL_RenderDrawRectF(m_renderer, &bridge);
            }
        }

        // 협곡 근처 화면 가장자리 붉은 열기 효과
        float nearDist = 9999.f;
        for (float cwx : CANYON_WORLD_X) {
            float d = std::abs(m_player.pos.x - (cwx + 16.f));
            if (d < nearDist) nearDist = d;
        }
        if (nearDist < 200.f) {
            float intensity = 1.f - nearDist / 200.f;
            Uint8 ea2 = (Uint8)(60 * intensity * (0.7f + 0.3f * std::sin(m_titleTimer * 3.f)));
            SDL_SetRenderDrawColor(m_renderer, 180, 30, 10, ea2);
            const float ew = 80.f;
            SDL_FRect left  = {0.f,                           0.f, ew, (float)m_screenH};
            SDL_FRect right = {(float)m_screenW - ew,         0.f, ew, (float)m_screenH};
            SDL_FRect top   = {0.f, 0.f,                           (float)m_screenW, ew * 0.6f};
            SDL_FRect bot   = {0.f, (float)m_screenH - ew * 0.6f, (float)m_screenW, ew * 0.6f};
            SDL_RenderFillRectF(m_renderer, &left);
            SDL_RenderFillRectF(m_renderer, &right);
            SDL_RenderFillRectF(m_renderer, &top);
            SDL_RenderFillRectF(m_renderer, &bot);
        }

        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }

    // Jupiter: central vortex rendering
    if (m_currentPlanet == 4) {
        static const float VCX = 576.f, VCY = 455.f;
        static const float DEATH_R = 65.f, WARN_R = 180.f;
        float vcx = VCX - m_camX, vcy = VCY - m_camY;
        float ringPulse = (std::sin(m_titleTimer * 5.f) + 1.f) * 0.5f;

        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

        // Warning ring (flashing red outline)
        for (int ring = 0; ring < 4; ring++) {
            float rrad = WARN_R - ring * 6.f;
            Uint8 ra = (Uint8)(50 + 60 * ringPulse);
            SDL_SetRenderDrawColor(m_renderer, 220, 50, 15, ra);
            for (float a = 0.f; a < 6.28318f; a += 0.08f) {
                float px = vcx + rrad * std::cos(a);
                float py = vcy + rrad * std::sin(a);
                SDL_FRect dot = {px - 1.f, py - 1.f, 3.f, 3.f};
                SDL_RenderFillRectF(m_renderer, &dot);
            }
        }

        // Spiral vortex particles (orange → yellow)
        for (int i = 0; i < 28; i++) {
            float frac = (float)i / 28.f;
            float angle = m_jupiterVortexTimer * 2.2f + frac * 6.28318f * 2.f;
            float rad   = DEATH_R + (WARN_R - DEATH_R) * frac;
            float px    = vcx + rad * std::cos(angle);
            float py    = vcy + rad * std::sin(angle);
            Uint8 pa    = (Uint8)(70 + 150 * (1.f - frac));
            Uint8 pr    = 255;
            Uint8 pg    = (Uint8)(100 + 120 * (1.f - frac));
            SDL_SetRenderDrawColor(m_renderer, pr, pg, 15, pa);
            float sz = 3.f + 4.f * (1.f - frac);
            SDL_FRect dot = {px - sz*0.5f, py - sz*0.5f, sz, sz};
            SDL_RenderFillRectF(m_renderer, &dot);
        }

        // Inner glow around death zone
        SDL_SetRenderDrawColor(m_renderer, 255, 80, 20, (Uint8)(80 + 60 * ringPulse));
        for (float a = 0.f; a < 6.28318f; a += 0.12f) {
            float px = vcx + (DEATH_R + 4.f) * std::cos(a);
            float py = vcy + (DEATH_R + 4.f) * std::sin(a);
            SDL_FRect d = {px - 2.f, py - 2.f, 5.f, 5.f};
            SDL_RenderFillRectF(m_renderer, &d);
        }

        // Death zone core (dark swirling black)
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 220);
        fillCircle(m_renderer, vcx, vcy, DEATH_R);
        SDL_SetRenderDrawColor(m_renderer, 140, 20, 5, (Uint8)(100 + 60 * ringPulse));
        fillCircle(m_renderer, vcx, vcy, DEATH_R * 0.6f);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        fillCircle(m_renderer, vcx, vcy, DEATH_R * 0.25f);

        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
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
    // Index-based loop so Jupiter/Venus can skip permanent structural walls
    for (int _di = 0; _di < (int)m_puzzle.doors.size(); _di++) {
        const auto& door = m_puzzle.doors[_di];
        // Jupiter: doors 1-2 are catch-walls, rendered separately below
        if (m_currentPlanet == 4 && _di >= 1) continue;
        // Venus: doors 1-3 are maze walls, rendered separately below
        if (m_currentPlanet == 1 && _di >= 1) continue;

        float anim = door.openAnim;
        if (anim >= 0.98f) continue;
        if (m_currentPlanet == 3 && !door.open) continue;  // closed Mars doors = canyon
        float ddx   = door.area.x - m_camX;
        float baseY = door.area.y - m_camY;
        float ddw   = door.area.w;
        float dh_vis = door.area.h * (1.f - anim);
        float dy_vis = baseY + door.area.h * anim;
        SDL_FRect rf = {ddx, dy_vis, ddw, dh_vis};
        if (!door.open) {
            float pulse = (std::sin(m_titleTimer * 3.f) + 1.f) * 0.5f;
            SDL_SetRenderDrawColor(m_renderer, (Uint8)(155+45*pulse), 30, 30, 238);
            SDL_RenderFillRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, 255, 65, 45, 255);
            SDL_RenderDrawRectF(m_renderer, &rf);
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

    // Jupiter: catch-walls rendered as orange/brown stone barriers
    if (m_currentPlanet == 4) {
        for (int di = 1; di < (int)m_puzzle.doors.size(); di++) {
            const auto& d = m_puzzle.doors[di];
            float wx = d.area.x - m_camX, wy = d.area.y - m_camY;
            SDL_SetRenderDrawColor(m_renderer, 75, 52, 28, 255);
            SDL_FRect rf2 = {wx, wy, d.area.w, d.area.h};
            SDL_RenderFillRectF(m_renderer, &rf2);
            SDL_SetRenderDrawColor(m_renderer, 108, 78, 42, 255);
            SDL_RenderDrawRectF(m_renderer, &rf2);
            SDL_SetRenderDrawColor(m_renderer, 55, 38, 18, 200);
            for (float ly = wy + 12.f; ly < wy + d.area.h - 4.f; ly += 12.f)
                SDL_RenderDrawLineF(m_renderer, wx+2.f, ly, wx+d.area.w-2.f, ly);
        }
    }

    // Venus: maze walls rendered as dark foggy stone
    if (m_currentPlanet == 1) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        for (int di = 1; di < (int)m_puzzle.doors.size(); di++) {
            const auto& d = m_puzzle.doors[di];
            float wx = d.area.x - m_camX, wy = d.area.y - m_camY;
            SDL_SetRenderDrawColor(m_renderer, 35, 28, 14, 245);
            SDL_FRect rf2 = {wx, wy, d.area.w, d.area.h};
            SDL_RenderFillRectF(m_renderer, &rf2);
            SDL_SetRenderDrawColor(m_renderer, 75, 58, 28, 200);
            SDL_RenderDrawRectF(m_renderer, &rf2);
            // Fog haze around wall edges
            float fp = (std::sin(m_titleTimer * 1.8f + wx * 0.04f) + 1.f) * 0.5f;
            Uint8 fogA = (Uint8)(22 + 18 * fp);
            SDL_SetRenderDrawColor(m_renderer, 190, 150, 60, fogA);
            SDL_FRect fogL = {wx-10.f, wy, 10.f, d.area.h};
            SDL_FRect fogR = {wx+d.area.w, wy, 10.f, d.area.h};
            SDL_RenderFillRectF(m_renderer, &fogL);
            SDL_RenderFillRectF(m_renderer, &fogR);
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
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

    // Mercury: unstable platforms
    if (m_currentPlanet == 0 && !m_puzzle.unstablePlatforms.empty()) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        for (const auto& up : m_puzzle.unstablePlatforms) {
            if (up.state == 2) continue;
            float upx = up.pos.x - m_camX;
            float upy = up.pos.y + up.shakeAmt - m_camY;
            float upw = up.w, uph = up.h;
            Uint8 baseGray = (up.state == 1) ? 100 : 145;
            float crack = (up.state == 1) ? (1.f - up.timer / 3.f) : 0.f;
            SDL_SetRenderDrawColor(m_renderer, baseGray, (Uint8)(baseGray-12), (Uint8)(baseGray-22), 230);
            SDL_FRect rf = {upx - upw*0.5f, upy - uph*0.5f, upw, uph};
            SDL_RenderFillRectF(m_renderer, &rf);
            SDL_SetRenderDrawColor(m_renderer, (Uint8)(baseGray+40), (Uint8)(baseGray+28), (Uint8)(baseGray+16), 255);
            SDL_RenderDrawRectF(m_renderer, &rf);
            // Crack lines when shaking
            if (up.state == 1 && crack > 0.3f) {
                Uint8 ca = (Uint8)(200 * crack);
                SDL_SetRenderDrawColor(m_renderer, 200, 60, 10, ca);
                SDL_RenderDrawLineF(m_renderer, upx - upw*0.3f, upy - uph*0.5f,
                                    upx - upw*0.1f, upy + uph*0.5f);
                SDL_RenderDrawLineF(m_renderer, upx + upw*0.2f, upy - uph*0.5f,
                                    upx + upw*0.4f, upy + uph*0.5f);
            }
            // Countdown number above shaking platform
            if (up.state == 1 && m_ui.getFont()) {
                int cd = std::max(1, std::min(3, (int)std::ceil(up.timer)));
                char cdbuf[4];
                std::snprintf(cdbuf, sizeof(cdbuf), "%d", cd);
                SDL_Color cdCol;
                if      (cd == 1) cdCol = {255, 60,  60,  255};
                else if (cd == 2) cdCol = {255, 160, 40,  255};
                else              cdCol = {255, 220, 50,  255};
                m_ui.renderText(m_renderer, m_ui.getFont(), cdbuf,
                                upx, upy - uph*0.5f - 18.f, cdCol, true);
            }
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
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
    for (int i = 0; i < (int)m_puzzle.logFiles.size(); i++)
        if (!m_puzzle.logFiles[i].collected)
            dlist.push_back({m_puzzle.logFiles[i].pos.y, 3, i});
    for (int i = 0; i < (int)m_puzzle.energyCells.size(); i++)
        if (!m_puzzle.energyCells[i].collected)
            dlist.push_back({m_puzzle.energyCells[i].pos.y, 4, i});
    for (int i = 0; i < (int)m_puzzle.energyDrinks.size(); i++)
        if (!m_puzzle.energyDrinks[i].collected)
            dlist.push_back({m_puzzle.energyDrinks[i].pos.y, 5, i});
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
        } else if (d.type == 2) {
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
            // Mercury direction arrow on rock
            if (m_currentPlanet == 0) {
                bool warn = m_puzzle.mercuryWarning;
                float arAlpha = warn ? (0.5f + 0.5f * std::sin(m_titleTimer * 10.f)) : 1.f;
                Uint8 aa = (Uint8)(200 * arAlpha);
                bool goLeft = m_puzzle.mercuryGoingLeft;
                SDL_SetRenderDrawColor(m_renderer, 255, 200, 50, aa);
                // Arrow shaft
                float ax1 = goLeft ? rx + 8.f  : rx - 8.f;
                float ax2 = goLeft ? rx - 8.f  : rx + 8.f;
                SDL_RenderDrawLineF(m_renderer, ax1, ry, ax2, ry);
                // Arrow head
                float hx2 = ax2, hy2 = ry;
                float dx2 = goLeft ? -4.f : 4.f;
                SDL_RenderDrawLineF(m_renderer, hx2, hy2, hx2 - dx2, hy2 - 4.f);
                SDL_RenderDrawLineF(m_renderer, hx2, hy2, hx2 - dx2, hy2 + 4.f);
            }
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        } else if (d.type == 3) {
            // Log file: green data chip + glow
            const auto& lf = m_puzzle.logFiles[d.idx];
            float bob = std::sin(lf.bobTimer * 2.8f) * 3.f;
            float lfx = lf.pos.x - m_camX, lfy = lf.pos.y - m_camY + bob;
            float glw = (std::sin(lf.bobTimer * 3.5f) + 1.f) * 0.5f;
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 40, 255, 120, (Uint8)(30 + 40 * glw));
            SDL_FRect aura = {lfx - 18.f, lfy - 18.f, 36.f, 36.f};
            SDL_RenderFillRectF(m_renderer, &aura);
            SDL_SetRenderDrawColor(m_renderer, 20, 50, 30, 220);
            SDL_FRect chip = {lfx - 10.f, lfy - 8.f, 20.f, 16.f};
            SDL_RenderFillRectF(m_renderer, &chip);
            SDL_SetRenderDrawColor(m_renderer, 50, 220, 100, 255);
            SDL_RenderDrawRectF(m_renderer, &chip);
            SDL_SetRenderDrawColor(m_renderer, 80, 255, 150, (Uint8)(160 + 80 * glw));
            SDL_FRect dot1 = {lfx - 5.f, lfy - 3.f, 4.f, 4.f};
            SDL_FRect dot2 = {lfx + 2.f, lfy - 3.f, 4.f, 4.f};
            SDL_RenderFillRectF(m_renderer, &dot1);
            SDL_RenderFillRectF(m_renderer, &dot2);
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        } else if (d.type == 4) {
            // Energy cell: blue crystal + glow
            const auto& ec = m_puzzle.energyCells[d.idx];
            float bob = std::sin(ec.bobTimer * 3.2f) * 3.f;
            float ecx = ec.pos.x - m_camX, ecy = ec.pos.y - m_camY + bob;
            float glw = (std::sin(ec.bobTimer * 4.f) + 1.f) * 0.5f;
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 60, 160, 255, (Uint8)(30 + 50 * glw));
            SDL_FRect aura = {ecx - 20.f, ecy - 20.f, 40.f, 40.f};
            SDL_RenderFillRectF(m_renderer, &aura);
            // Crystal diamond shape
            SDL_SetRenderDrawColor(m_renderer, 30, 100, 220, 240);
            fillDiamond(m_renderer, ecx, ecy, 10.f);
            SDL_SetRenderDrawColor(m_renderer, 120, 200, 255, (Uint8)(180 + 70 * glw));
            fillDiamond(m_renderer, ecx, ecy, 5.f);
            SDL_SetRenderDrawColor(m_renderer, 200, 235, 255, 255);
            fillDiamond(m_renderer, ecx, ecy, 2.f);
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        } else if (d.type == 5) {
            // Energy drink: blue potion bottle + glow
            const auto& ed = m_puzzle.energyDrinks[d.idx];
            float bob = std::sin(ed.bobTimer * 2.6f) * 3.5f;
            float edx = ed.pos.x - m_camX, edy = ed.pos.y - m_camY + bob;
            float glw = (std::sin(ed.bobTimer * 3.5f) + 1.f) * 0.5f;
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 40, 100, 255, (Uint8)(25 + 35*glw));
            SDL_FRect aura = {edx-16.f, edy-22.f, 32.f, 44.f};
            SDL_RenderFillRectF(m_renderer, &aura);
            SDL_SetRenderDrawColor(m_renderer, 30, 80, 200, 230);
            SDL_FRect body = {edx-6.f, edy-8.f, 12.f, 18.f};
            SDL_RenderFillRectF(m_renderer, &body);
            SDL_SetRenderDrawColor(m_renderer, 40, 100, 220, 230);
            SDL_FRect neck = {edx-3.f, edy-16.f, 6.f, 10.f};
            SDL_RenderFillRectF(m_renderer, &neck);
            SDL_SetRenderDrawColor(m_renderer, 180, 210, 255, 255);
            SDL_FRect cap = {edx-4.f, edy-18.f, 8.f, 4.f};
            SDL_RenderFillRectF(m_renderer, &cap);
            SDL_SetRenderDrawColor(m_renderer, 80, 160, 255, (Uint8)(140 + 80*glw));
            SDL_FRect liq = {edx-3.f, edy-6.f, 6.f, 12.f};
            SDL_RenderFillRectF(m_renderer, &liq);
            SDL_SetRenderDrawColor(m_renderer, 100, 180, 255, 200);
            SDL_RenderDrawRectF(m_renderer, &body);
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

    // Venus: toxic cloud rendering + emotional texts at dead ends
    if (m_currentPlanet == 1) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

        // Toxic clouds
        for (const auto& c : m_venusClouds) {
            float cx2 = c.pos.x - m_camX, cy2 = c.pos.y - m_camY;
            // Pre-move warning: cloud grows near path endpoints
            float edgeProx = std::min(c.t, 1.f - c.t) / 0.18f;
            float grow = (edgeProx < 1.f) ? (1.f - edgeProx) * 7.f : 0.f;
            float vis = c.radius + grow;

            if (c.stunTimer > 0.f) {
                // Stunned: gray flicker
                Uint8 sa = (Uint8)(80 + 40 * std::sin(m_titleTimer * 10.f));
                SDL_SetRenderDrawColor(m_renderer, 140, 140, 140, sa);
                fillCircle(m_renderer, cx2, cy2, vis);
            } else {
                // Outer green glow
                SDL_SetRenderDrawColor(m_renderer, 40, 200, 80, 90);
                fillCircle(m_renderer, cx2, cy2, vis + 6.f);
                // Main cloud body (green-purple blend)
                SDL_SetRenderDrawColor(m_renderer, 35, 180, 70, 130);
                fillCircle(m_renderer, cx2, cy2, vis);
                // Inner darker core
                SDL_SetRenderDrawColor(m_renderer, 100, 50, 140, 160);
                fillCircle(m_renderer, cx2, cy2, vis * 0.55f);
                // Swirling particles
                for (int i = 0; i < 8; i++) {
                    float a = c.wobble * 2.5f + i * 0.785f;
                    float pr = vis * 0.72f;
                    float px2 = cx2 + pr * std::cos(a);
                    float py2 = cy2 + pr * std::sin(a);
                    Uint8 pa2 = (Uint8)(140 + 80 * std::sin(c.wobble * 3.f + i));
                    SDL_SetRenderDrawColor(m_renderer, 80, 255, 120, pa2);
                    SDL_FRect d2 = {px2-3.f, py2-3.f, 6.f, 6.f};
                    SDL_RenderFillRectF(m_renderer, &d2);
                }
            }
        }

        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

        // Emotional texts at dead ends (like Earth signs)
        static const struct { float x, y; const char* text; } VSIGNS[] = {
            {180.f, 480.f, "이 길은 아닌 것 같아... 다시 돌아가야겠어."},
            {980.f, 480.f, "짙은 안개 속에서 길을 잃었어. 천천히 생각해보자."},
            {576.f, 530.f, "금성의 대기는 너무 두꺼워. 숨이 막힐 것 같아."},
            {576.f, 160.f, "여기도 막혔어. 하지만 포기하지 않을 거야."},
        };
        if (m_ui.getFont()) {
            for (const auto& vs : VSIGNS) {
                float vsx = vs.x - m_camX, vsy = vs.y - m_camY;
                float dist2 = std::abs(m_player.pos.x - vs.x) + std::abs(m_player.pos.y - vs.y);
                float alpha2 = std::max(0.f, 1.f - dist2 / 220.f);
                if (alpha2 < 0.02f) continue;
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(m_renderer, 15, 10, 5, (Uint8)(150 * alpha2));
                SDL_FRect tbg = {vsx - 180.f, vsy - 18.f, 360.f, 28.f};
                SDL_RenderFillRectF(m_renderer, &tbg);
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
                SDL_Color vtc = {210, 185, 120, (Uint8)(210 * alpha2)};
                m_ui.renderText(m_renderer, m_ui.getFont(), vs.text, vsx, vsy - 12.f, vtc, true);
            }
        }
    }

    // Jupiter: hint texts near rocks ("소용돌이 방향으로 밀어봐")
    if (m_currentPlanet == 4 && m_ui.getFont()) {
        static const struct { float x, y; } JROCKS[] = {{200.f,460.f},{950.f,460.f}};
        for (const auto& jr : JROCKS) {
            float dist2 = std::abs(m_player.pos.x - jr.x) + std::abs(m_player.pos.y - jr.y);
            float alpha2 = std::max(0.f, 1.f - dist2 / 180.f);
            if (alpha2 < 0.02f) continue;
            float jx = jr.x - m_camX, jy = jr.y - m_camY - 40.f;
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, (Uint8)(140 * alpha2));
            SDL_FRect hbg = {jx - 115.f, jy - 2.f, 230.f, 22.f};
            SDL_RenderFillRectF(m_renderer, &hbg);
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
            SDL_Color htc = {255, 210, 80, (Uint8)(220 * alpha2)};
            m_ui.renderText(m_renderer, m_ui.getFont(), "소용돌이 방향으로 밀어봐!", jx, jy, htc, true);
        }
    }

    // Jupiter: wind direction arrows + vortex warning text (screen overlay)
    if (m_currentPlanet == 4) {
        const float SW = (float)m_screenW, SH = (float)m_screenH;

        // Vortex warning text (top-center when in danger zone)
        if (m_jupiterVortexWarn && m_ui.getFont()) {
            float pulse = 0.5f + 0.5f * std::sin(m_titleTimer * 9.f);
            SDL_Color wc = {255, 80, 30, (Uint8)(220 * pulse)};
            m_ui.renderText(m_renderer, m_ui.getFont(),
                            "소용돌이에 너무 가까워! 반대 방향으로 빠져나와!",
                            SW * 0.5f, 80.f, wc, true);
        }

        // Wind arrows on screen edges
        bool showArrows = m_jupiterWindActive || m_jupiterWindWarning;
        if (showArrows) {
            // During warning: show arrows for NEXT direction; active: current direction
            int arDir = m_jupiterWindWarning ? (m_jupiterWindDir + 1) % 4 : m_jupiterWindDir;
            float warnMult = m_jupiterWindWarning
                ? (0.5f + 0.5f * std::sin(m_titleTimer * 7.f))
                : 1.f;
            Uint8 arrA = (Uint8)(200 * warnMult);

            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            // Draw 5 arrows evenly spaced on the appropriate edge
            for (int ai = 0; ai < 5; ai++) {
                float t = (ai + 0.5f) / 5.f;
                switch (arDir) {
                    case 0: // East: arrows on left edge pointing right
                        drawWindArrow(m_renderer, 30.f, SH*t, 1.f, 0.f, 32.f, arrA);
                        break;
                    case 1: // West: arrows on right edge pointing left
                        drawWindArrow(m_renderer, SW-30.f, SH*t, -1.f, 0.f, 32.f, arrA);
                        break;
                    case 2: // South: arrows on top edge pointing down
                        drawWindArrow(m_renderer, SW*t, 30.f, 0.f, 1.f, 32.f, arrA);
                        break;
                    case 3: // North: arrows on bottom edge pointing up
                        drawWindArrow(m_renderer, SW*t, SH-30.f, 0.f, -1.f, 32.f, arrA);
                        break;
                }
            }

            // Wind-active edge shimmer (opposite edge from arrows)
            if (m_jupiterWindActive) {
                float shim = (std::sin(m_titleTimer * 14.f) + 1.f) * 0.5f;
                Uint8 shimA = (Uint8)(30 + 30 * shim);
                SDL_SetRenderDrawColor(m_renderer, 255, 200, 50, shimA);
                const float THICK = 8.f;
                switch (m_jupiterWindDir) {
                    case 0: { SDL_FRect r2 = {SW-THICK, 0.f, THICK, SH};   SDL_RenderFillRectF(m_renderer, &r2); break; }
                    case 1: { SDL_FRect r2 = {0.f,     0.f, THICK, SH};   SDL_RenderFillRectF(m_renderer, &r2); break; }
                    case 2: { SDL_FRect r2 = {0.f, SH-THICK, SW,  THICK}; SDL_RenderFillRectF(m_renderer, &r2); break; }
                    case 3: { SDL_FRect r2 = {0.f,     0.f,  SW,  THICK}; SDL_RenderFillRectF(m_renderer, &r2); break; }
                }
            }
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        }

        // Wind direction label (bottom-right area)
        if (m_ui.getFont()) {
            static const char* WIND_LABEL[4] = {"→ 동풍","← 서풍","↓ 남풍","↑ 북풍"};
            const char* label = m_jupiterWindWarning
                ? "바람 전환 예고!" : WIND_LABEL[m_jupiterWindDir];
            SDL_Color lc = m_jupiterWindWarning
                ? SDL_Color{255, 180, 40, 220} : SDL_Color{220, 180, 80, 190};
            m_ui.renderText(m_renderer, m_ui.getFont(), label,
                            SW - 70.f, SH - 52.f, lc, true);
        }
    }

    // Mars: meteor shower rendering (shadows, falling bodies, impact dust)
    if (m_currentPlanet == 3) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        for (const auto& met : m_marsMeteorites) {
            if (!met.active) continue;
            float sx = met.targetX - m_camX;
            float sy = met.landY   - m_camY;

            if (met.warnTimer > 0.f) {
                // Shadow warning circle on ground
                float progress = 1.f - met.warnTimer / 1.5f;
                float radius   = 8.f + 22.f * progress;
                Uint8 shadowA  = (Uint8)(60 + 180 * progress);
                SDL_SetRenderDrawColor(m_renderer, 200, 30, 10, shadowA);
                fillCircle(m_renderer, sx, sy, radius);
                SDL_SetRenderDrawColor(m_renderer, 255, 80, 30, (Uint8)(shadowA * 0.5f));
                fillCircle(m_renderer, sx, sy, radius * 0.4f);

                // Falling meteor body
                float meteY = met.y - m_camY;
                if (meteY > -30.f && meteY < (float)m_screenH + 30.f) {
                    // Fire trail (above meteor)
                    for (int fi = 4; fi >= 1; fi--) {
                        float tr = 9.f - fi * 1.6f;
                        if (tr <= 0.f) continue;
                        Uint8 fa = (Uint8)(210 - fi * 40);
                        SDL_SetRenderDrawColor(m_renderer, 255, (Uint8)(180 - fi * 30), 20, fa);
                        fillCircle(m_renderer, sx, meteY - fi * 9.f, tr);
                    }
                    // Meteor rock body
                    SDL_SetRenderDrawColor(m_renderer, 70, 50, 30, 245);
                    fillCircle(m_renderer, sx, meteY, 12.f);
                    SDL_SetRenderDrawColor(m_renderer, 110, 80, 50, 220);
                    fillCircle(m_renderer, sx, meteY, 8.f);
                    SDL_SetRenderDrawColor(m_renderer, 160, 130, 90, 180);
                    fillCircle(m_renderer, sx - 3.f, meteY - 3.f, 4.f);
                }
            } else if (met.landed) {
                // Dust particles
                for (const auto& d : met.dust) {
                    float dx = d.x - m_camX, dy = d.y - m_camY;
                    float dalpha = std::max(0.f, d.life / d.maxLife);
                    Uint8 da = (Uint8)(200.f * dalpha);
                    SDL_SetRenderDrawColor(m_renderer, 160, 120, 80, da);
                    SDL_FRect dot = {dx - 2.f, dy - 2.f, 4.f, 4.f};
                    SDL_RenderFillRectF(m_renderer, &dot);
                    // Some bright orange sparks
                    SDL_SetRenderDrawColor(m_renderer, 220, 100, 30, (Uint8)(da * 0.6f));
                    SDL_FRect spark = {dx - 1.f, dy - 1.f, 2.f, 2.f};
                    SDL_RenderFillRectF(m_renderer, &spark);
                }
                // Impact crater glow
                if (met.dustTimer > 0.f) {
                    float cratA = std::min(1.f, met.dustTimer / 1.5f);
                    SDL_SetRenderDrawColor(m_renderer, 180, 80, 30, (Uint8)(90 * cratA));
                    fillCircle(m_renderer, sx, sy, 20.f);
                    SDL_SetRenderDrawColor(m_renderer, 50, 30, 10, (Uint8)(160 * cratA));
                    fillCircle(m_renderer, sx, sy, 13.f);
                }
            }
        }

        // Meteor plate special visual (orange/red, distinct from regular plates)
        if (m_marsMeteorPlateIdx >= 0 &&
            m_marsMeteorPlateIdx < (int)m_puzzle.plates.size()) {
            const auto& mp = m_puzzle.plates[m_marsMeteorPlateIdx];
            float ppx = mp.area.x - m_camX, ppy = mp.area.y - m_camY;
            float ppw = mp.area.w, pph = mp.area.h;
            float pulse2 = (std::sin(m_titleTimer * 4.f) + 1.f) * 0.5f;
            SDL_SetRenderDrawColor(m_renderer, (Uint8)(110 + 60*pulse2), 35, 15, 230);
            SDL_FRect mpRf = {ppx, ppy, ppw, pph};
            SDL_RenderFillRectF(m_renderer, &mpRf);
            SDL_SetRenderDrawColor(m_renderer, 255, 100, 50, 210);
            SDL_RenderDrawRectF(m_renderer, &mpRf);
            // Crosshair target symbol
            SDL_SetRenderDrawColor(m_renderer, 255, 150, 80, 190);
            SDL_RenderDrawLineF(m_renderer, ppx + ppw*0.5f, ppy + 2.f,
                                ppx + ppw*0.5f, ppy + pph - 2.f);
            SDL_RenderDrawLineF(m_renderer, ppx + 2.f, ppy + pph*0.5f,
                                ppx + ppw - 2.f, ppy + pph*0.5f);
        }

        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }

    // Mercury: solar flare orange tint + beams (rendered before HUD, after world)
    if (m_currentPlanet == 0) {
        if (m_solarFlareTint > 0.f) {
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 255, 120, 20, (Uint8)(70 * m_solarFlareTint));
            SDL_FRect full = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
            SDL_RenderFillRectF(m_renderer, &full);
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        }
        if (m_mercurySolarFlareActive && !m_solarBeams.empty()) {
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            const float BEAM_H     = 28.f;
            const float BEAM_SPEED = 80.f;
            const float WARN_AHEAD = 1.0f;   // show dashes 1 second ahead

            for (const auto& b : m_solarBeams) {
                // --- 1-second warning dashed lines ---
                // Only shown while the beam is still above screen
                float warnY = b.y + BEAM_SPEED * WARN_AHEAD;
                bool beamAbove  = (b.y + BEAM_H < 0.f);
                bool warnOnScreen = (warnY + BEAM_H >= 0.f) && (warnY < (float)m_screenH);
                if (beamAbove && warnOnScreen) {
                    // Top edge dash
                    SDL_SetRenderDrawColor(m_renderer, 255, 220, 50, 190);
                    for (float x = 0.f; x < (float)m_screenW; x += 24.f) {
                        float ex = std::min(x + 14.f, (float)m_screenW);
                        SDL_RenderDrawLineF(m_renderer, x, warnY,          ex, warnY);
                        SDL_RenderDrawLineF(m_renderer, x, warnY + BEAM_H, ex, warnY + BEAM_H);
                    }
                    // Faint interior fill showing future beam area
                    SDL_SetRenderDrawColor(m_renderer, 255, 200, 40, 28);
                    SDL_FRect fillRf = {0.f, warnY, (float)m_screenW, BEAM_H};
                    SDL_RenderFillRectF(m_renderer, &fillRf);
                }

                // --- Actual beam ---
                if (b.y + BEAM_H <= 0.f || b.y >= (float)m_screenH) continue;
                // Outer glow
                SDL_SetRenderDrawColor(m_renderer, 255, 160, 40, 55);
                SDL_FRect glowRf = {0.f, b.y - 8.f, (float)m_screenW, BEAM_H + 16.f};
                SDL_RenderFillRectF(m_renderer, &glowRf);
                // Main beam body
                SDL_SetRenderDrawColor(m_renderer, 255, 180, 50, 215);
                SDL_FRect beamRf = {0.f, b.y, (float)m_screenW, BEAM_H};
                SDL_RenderFillRectF(m_renderer, &beamRf);
                // Bright center highlight
                SDL_SetRenderDrawColor(m_renderer, 255, 240, 160, 245);
                SDL_FRect ctrRf = {0.f, b.y + BEAM_H * 0.35f, (float)m_screenW, BEAM_H * 0.3f};
                SDL_RenderFillRectF(m_renderer, &ctrRf);
            }
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
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

    // Lives HUD (hearts panel, below planet/gravity panel)
    {
        const auto& ac2 = curPhysics().ambientColor;
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 8, 10, 20, 150);
        SDL_FRect hPanel = {8.f, 58.f, 110.f, 30.f};
        SDL_RenderFillRectF(m_renderer, &hPanel);
        SDL_SetRenderDrawColor(m_renderer, ac2.r, ac2.g, ac2.b, 150);
        SDL_FRect hAccent = {8.f, 58.f, 4.f, 30.f};
        SDL_RenderFillRectF(m_renderer, &hAccent);

        float hx0 = 24.f, hy0 = 73.f, hs = 10.f, gap = 26.f;
        for (int i = 0; i < 3; i++) {
            bool filled = (i < m_lives);
            float pulse = (m_lives == 1 && filled)
                ? (0.5f + 0.5f * std::sin(m_titleTimer * 6.f)) : 1.f;
            Uint8 alpha = filled ? (Uint8)(230 * pulse) : 70;
            if (filled) SDL_SetRenderDrawColor(m_renderer, 220, 50, 50, alpha);
            else        SDL_SetRenderDrawColor(m_renderer, 80, 80, 80, alpha);
            drawHeart(m_renderer, hx0 + i * gap, hy0, hs);
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }

    // Mercury-specific UI
    if (m_currentPlanet == 0) {
        // Solar flare warning text (left side, distinct from notification)
        if (m_mercurySolarWarning && m_ui.getFont()) {
            float wPulse = 0.5f + 0.5f * std::sin(m_titleTimer * 7.f);
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 160, 60, 5, (Uint8)(150 * wPulse));
            SDL_FRect wBg = {8.f, 122.f, 246.f, 38.f};
            SDL_RenderFillRectF(m_renderer, &wBg);
            SDL_SetRenderDrawColor(m_renderer, 255, 140, 40, (Uint8)(220 * wPulse));
            SDL_RenderDrawRectF(m_renderer, &wBg);
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
            SDL_Color wc = {255, 210, 100, (Uint8)(230 * wPulse)};
            m_ui.renderText(m_renderer, m_ui.getFont(), "⚠ 태양 플레어 경보!",
                            8.f + 123.f, 132.f, wc, true);
        }

        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

        // Warning particles before phase switch
        if (m_puzzle.mercuryWarning) {
            srand((int)(m_titleTimer * 20));
            for (int i = 0; i < 12; i++) {
                float px2 = (float)(rand() % m_screenW);
                float py2 = (float)(rand() % m_screenH);
                Uint8 wa = (Uint8)(60 + 80 * std::sin(m_titleTimer * 8.f + i));
                SDL_SetRenderDrawColor(m_renderer, 255, 160, 50, wa);
                SDL_FRect wd = {px2 - 2.f, py2 - 2.f, 4.f, 4.f};
                SDL_RenderFillRectF(m_renderer, &wd);
            }
        }

        // Reset fade overlay
        if (m_resetFade > 0.f) {
            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, (Uint8)(255 * m_resetFade));
            SDL_FRect full = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
            SDL_RenderFillRectF(m_renderer, &full);
        }
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

        // "R: 재시작" hint bottom-right
        if (m_ui.getFont() && m_resetState == 0) {
            SDL_Color rc = {180, 180, 200, 190};
            m_ui.renderText(m_renderer, m_ui.getFont(), "R: 재시작",
                            (float)m_screenW - 60.f, (float)m_screenH - 28.f, rc, true);
        }
        // Energy cell count display
        if (m_ui.getFont()) {
            char cellBuf[32];
            std::snprintf(cellBuf, sizeof(cellBuf), "에너지 셀: %d/3", m_energyCellsFound);
            SDL_Color cc = {100, 200, 255, 220};
            m_ui.renderText(m_renderer, m_ui.getFont(), cellBuf,
                            (float)m_screenW - 70.f, (float)m_screenH - 50.f, cc, true);
        }
    }

    // Mars log reading overlay
    if (m_currentPlanet == 3 && m_marsLogReading >= 0 && m_ui.getFont()) {
        static const char* LOG_TEXTS[3] = {
            "탐사대 기록 #001\n우리는 이 게이트를 발견했다.\n고대 문명의 것이 분명하다.",
            "탐사대 기록 #002\n게이트 작동 원리를 파악했다.\n부품만 있으면 어디든 갈 수 있어.",
            "탐사대 기록 #003\n우리 중 한 명이 사라졌다.\n게이트가... 뭔가 이상해."
        };
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
        SDL_FRect dim = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
        SDL_RenderFillRectF(m_renderer, &dim);
        float ox = m_screenW * 0.5f - 260.f, oy = m_screenH * 0.5f - 100.f;
        SDL_SetRenderDrawColor(m_renderer, 8, 20, 12, 250);
        SDL_FRect popup = {ox, oy, 520.f, 200.f};
        SDL_RenderFillRectF(m_renderer, &popup);
        SDL_SetRenderDrawColor(m_renderer, 50, 200, 100, 230);
        SDL_RenderDrawRectF(m_renderer, &popup);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        // Title
        m_ui.renderText(m_renderer, m_ui.getFont(), "데이터 칩",
                        m_screenW * 0.5f, oy + 20.f, {80, 240, 130, 240}, true);
        // Log text (split by \n manually — render 3 lines)
        const char* txt = LOG_TEXTS[m_marsLogReading];
        std::string s(txt);
        float ly = oy + 56.f;
        size_t pos2 = 0;
        while (pos2 < s.size()) {
            size_t nl = s.find('\n', pos2);
            std::string line = (nl == std::string::npos) ? s.substr(pos2) : s.substr(pos2, nl - pos2);
            m_ui.renderText(m_renderer, m_ui.getFont(), line.c_str(),
                            m_screenW * 0.5f, ly, {200, 240, 210, 230}, true);
            ly += 26.f;
            if (nl == std::string::npos) break;
            pos2 = nl + 1;
        }
        float blink = 0.55f + 0.45f * std::sin(m_titleTimer * 4.5f);
        m_ui.renderText(m_renderer, m_ui.getFont(), "[ ENTER로 닫기 ]",
                        m_screenW * 0.5f, oy + 172.f, {180, 255, 200, (Uint8)(220 * blink)}, true);
    }

    // Mars: 구역 표시 텍스트 (페이드인 → 유지 → 페이드아웃)
    if (m_currentPlanet == 3 && m_marsZoneTextIdx >= 0
        && m_marsZoneTextTimer > 0.f && m_ui.getFontBig()) {
        static const char* ZONE_NAMES[] = {
            "구역 1  -  착륙 지점",
            "구역 2  -  탐사 구역",
            "구역 3  -  워프 게이트 구역"
        };
        float t = m_marsZoneTextTimer;
        float alpha;
        if      (t > 3.5f) alpha = (4.f - t) / 0.5f;   // 0.5s 페이드인
        else if (t > 1.5f) alpha = 1.f;                  // 2s 유지
        else               alpha = t / 1.5f;              // 1.5s 페이드아웃

        Uint8 a = (Uint8)(230.f * alpha);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, (Uint8)(120.f * alpha));
        SDL_FRect bg = {m_screenW * 0.5f - 220.f, 58.f, 440.f, 38.f};
        SDL_RenderFillRectF(m_renderer, &bg);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        m_ui.renderText(m_renderer, m_ui.getFontBig(),
                        ZONE_NAMES[m_marsZoneTextIdx],
                        m_screenW * 0.5f, 68.f, {255, 160, 80, a}, true);
    }

    // Death fade overlay
    if (m_deathFade > 0.f) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, (Uint8)(255 * m_deathFade));
        SDL_FRect full = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
        SDL_RenderFillRectF(m_renderer, &full);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    }
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

    // Parts panel (right) — or planet-specific panel
    {
        const float px = 960.f, py = 110.f, pw = 280.f, ph = 250.f;

        if (m_currentPlanet == 3) {
            // Mars: archive panel showing collected log files
            int collectedCount = 0;
            for (int i = 0; i < 3; i++) if (m_marsLogsCollected[i]) collectedCount++;
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 8, 20, 12, 245);
            SDL_FRect panel = {px, py, pw, ph};
            SDL_RenderFillRectF(m_renderer, &panel);
            SDL_SetRenderDrawColor(m_renderer, 50, 180, 90, 200);
            SDL_RenderDrawRectF(m_renderer, &panel);
            // Log chip indicators
            for (int i = 0; i < 3; i++) {
                float cx2 = px + 36.f + i * 72.f;
                float cy2 = py + ph * 0.55f;
                if (m_marsLogsCollected[i]) {
                    SDL_SetRenderDrawColor(m_renderer, 50, 220, 100, 255);
                    SDL_FRect chip = {cx2 - 14.f, cy2 - 10.f, 28.f, 20.f};
                    SDL_RenderFillRectF(m_renderer, &chip);
                    SDL_SetRenderDrawColor(m_renderer, 150, 255, 180, 255);
                    SDL_RenderDrawRectF(m_renderer, &chip);
                } else {
                    SDL_SetRenderDrawColor(m_renderer, 25, 50, 32, 200);
                    SDL_FRect chip = {cx2 - 14.f, cy2 - 10.f, 28.f, 20.f};
                    SDL_RenderFillRectF(m_renderer, &chip);
                    SDL_SetRenderDrawColor(m_renderer, 40, 80, 50, 200);
                    SDL_RenderDrawRectF(m_renderer, &chip);
                }
            }
            // Base restoration lighting overlay based on parts found
            if (m_planetPartsFound == 0) {
                SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 80);
                SDL_FRect darkOverlay = {42.f, 54.f, 1196.f, 566.f};
                SDL_RenderFillRectF(m_renderer, &darkOverlay);
            } else if (m_planetPartsFound == 1) {
                SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 30);
                SDL_FRect dimOverlay = {42.f, 54.f, 1196.f, 566.f};
                SDL_RenderFillRectF(m_renderer, &dimOverlay);
            }
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        } else if (m_currentPlanet == 0) {
            // Mercury: charging panel showing energy cells and warp range
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 8, 16, 30, 245);
            SDL_FRect panel = {px, py, pw, ph};
            SDL_RenderFillRectF(m_renderer, &panel);
            SDL_SetRenderDrawColor(m_renderer, 40, 120, 220, 200);
            SDL_RenderDrawRectF(m_renderer, &panel);
            // Energy cell indicators
            for (int i = 0; i < 3; i++) {
                float cx2 = px + 36.f + i * 72.f;
                float cy2 = py + ph * 0.50f;
                bool hasCell = (i < m_energyCellsFound);
                float glow2 = hasCell ? (std::sin(m_baseTimer * 4.f + i) + 1.f) * 0.5f : 0.f;
                if (hasCell) {
                    SDL_SetRenderDrawColor(m_renderer, 30, 80, 200, (Uint8)(50 + 40 * glow2));
                    SDL_FRect aura = {cx2 - 18.f, cy2 - 18.f, 36.f, 36.f};
                    SDL_RenderFillRectF(m_renderer, &aura);
                    SDL_SetRenderDrawColor(m_renderer, 60, 160, 255, 240);
                    fillDiamond(m_renderer, cx2, cy2, 12.f);
                    SDL_SetRenderDrawColor(m_renderer, 180, 230, 255, 255);
                    fillDiamond(m_renderer, cx2, cy2, 5.f);
                } else {
                    SDL_SetRenderDrawColor(m_renderer, 22, 35, 60, 200);
                    fillDiamond(m_renderer, cx2, cy2, 12.f);
                    SDL_SetRenderDrawColor(m_renderer, 35, 55, 90, 200);
                    fillDiamond(m_renderer, cx2, cy2, 5.f);
                }
            }
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        } else {
            int partCount = LAYOUTS[m_currentPlanet].partCount;
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

        // Right panel title
        if (m_currentPlanet == 3) {
            m_ui.renderText(m_renderer, m_ui.getFont(), "데이터 아카이브",
                            960.f + 140.f, 110.f + 30.f, {80, 220, 120, 220}, true);
            int collectedCount = 0;
            for (int i = 0; i < 3; i++) if (m_marsLogsCollected[i]) collectedCount++;
            char logBuf[32];
            std::snprintf(logBuf, sizeof(logBuf), "로그 %d/3", collectedCount);
            m_ui.renderText(m_renderer, m_ui.getFont(), logBuf,
                            960.f + 140.f, 110.f + 56.f, {150, 240, 170, 200}, true);
            // E-hint if near archive and has logs
            AABB archiveProbe = {960.f, 100.f, 280.f, 260.f};
            if (m_player.getAABB().intersects(archiveProbe) && collectedCount > 0) {
                float blink2 = 0.6f + 0.4f * std::sin(m_baseTimer * 4.f);
                m_ui.renderText(m_renderer, m_ui.getFont(), "E: 열기",
                                960.f + 140.f, 110.f + 250.f + 16.f,
                                {100, 255, 150, (Uint8)(220 * blink2)}, true);
            }
        } else if (m_currentPlanet == 0) {
            m_ui.renderText(m_renderer, m_ui.getFont(), "충전 패널",
                            960.f + 140.f, 110.f + 30.f, {80, 160, 255, 220}, true);
            char cellBuf2[48];
            int cells = m_energyCellsFound;
            const char* range = cells >= 3 ? "자유 선택" : cells == 2 ? "2행성 범위" : "인접 행성만";
            std::snprintf(cellBuf2, sizeof(cellBuf2), "셀 %d/3  범위: %s", cells, range);
            m_ui.renderText(m_renderer, m_ui.getFont(), cellBuf2,
                            960.f + 140.f, 110.f + 56.f, {140, 200, 255, 200}, true);
        } else {
            m_ui.renderText(m_renderer, m_ui.getFont(), "수집 부품",
                            960.f + 140.f, 110.f + 30.f, {160, 200, 255, 220}, true);
        }

        // Exit label
        m_ui.renderText(m_renderer, m_ui.getFont(), "[ 외부로 나가기 ]",
                        640.f, 661.f, {100, 160, 255, 200}, true);
    }

    // Player
    m_player.render(m_renderer, 0.f, 0.f);

    // Mars archive overlay
    if (m_marsArchiveOpen && m_ui.getFont()) {
        static const char* LOG_TITLES[3] = {
            "탐사대 기록 #001",
            "탐사대 기록 #002",
            "탐사대 기록 #003"
        };
        static const char* LOG_BODIES[3] = {
            "우리는 이 게이트를 발견했다.\n고대 문명의 것이 분명하다.",
            "게이트 작동 원리를 파악했다.\n부품만 있으면 어디든 갈 수 있어.",
            "우리 중 한 명이 사라졌다.\n게이트가... 뭔가 이상해."
        };
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 185);
        SDL_FRect dim = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
        SDL_RenderFillRectF(m_renderer, &dim);
        float aox = m_screenW * 0.5f - 280.f, aoy = m_screenH * 0.5f - 140.f;
        SDL_SetRenderDrawColor(m_renderer, 8, 20, 12, 250);
        SDL_FRect apop = {aox, aoy, 560.f, 280.f};
        SDL_RenderFillRectF(m_renderer, &apop);
        SDL_SetRenderDrawColor(m_renderer, 50, 190, 90, 230);
        SDL_RenderDrawRectF(m_renderer, &apop);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
        m_ui.renderText(m_renderer, m_ui.getFont(), "데이터 아카이브",
                        m_screenW * 0.5f, aoy + 22.f, {80, 240, 130, 240}, true);
        // Tab selectors
        int tabIdx = 0;
        for (int i = 0; i < 3; i++) {
            if (!m_marsLogsCollected[i]) continue;
            float tx = aox + 40.f + tabIdx * 130.f;
            bool sel = (m_marsArchiveSel == i);
            SDL_Color tc2 = sel ? SDL_Color{80, 255, 140, 255} : SDL_Color{80, 150, 100, 180};
            m_ui.renderText(m_renderer, m_ui.getFont(), LOG_TITLES[i], tx + 50.f, aoy + 58.f, tc2, true);
            tabIdx++;
        }
        // Display selected log
        if (m_marsLogsCollected[m_marsArchiveSel]) {
            m_ui.renderText(m_renderer, m_ui.getFont(), LOG_TITLES[m_marsArchiveSel],
                            m_screenW * 0.5f, aoy + 100.f, {130, 255, 170, 230}, true);
            std::string body(LOG_BODIES[m_marsArchiveSel]);
            float ly2 = aoy + 132.f;
            size_t pos3 = 0;
            while (pos3 < body.size()) {
                size_t nl = body.find('\n', pos3);
                std::string line = (nl == std::string::npos) ? body.substr(pos3) : body.substr(pos3, nl - pos3);
                m_ui.renderText(m_renderer, m_ui.getFont(), line.c_str(),
                                m_screenW * 0.5f, ly2, {200, 240, 210, 230}, true);
                ly2 += 26.f;
                if (nl == std::string::npos) break;
                pos3 = nl + 1;
            }
        }
        float blink2 = 0.55f + 0.45f * std::sin(m_baseTimer * 4.5f);
        m_ui.renderText(m_renderer, m_ui.getFont(), "[ E: 닫기 ]",
                        m_screenW * 0.5f, aoy + 252.f, {180, 255, 200, (Uint8)(220 * blink2)}, true);
    }

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

void Game::renderDevMenu() {
    if (!m_devMenuOpen) return;

    auto btns = buildDevMenu(m_screenW, m_screenH);
    const float PW = 440.f, PH = 510.f;
    const float PX = (m_screenW - PW) * 0.5f;
    const float PY = (m_screenH - PH) * 0.5f;

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // Dark overlay
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 195);
    SDL_FRect full = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
    SDL_RenderFillRectF(m_renderer, &full);

    // Panel
    SDL_SetRenderDrawColor(m_renderer, 10, 14, 26, 248);
    SDL_FRect panel = {PX, PY, PW, PH};
    SDL_RenderFillRectF(m_renderer, &panel);
    SDL_SetRenderDrawColor(m_renderer, 65, 105, 195, 220);
    SDL_RenderDrawRectF(m_renderer, &panel);
    SDL_SetRenderDrawColor(m_renderer, 65, 105, 195, 180);
    SDL_FRect topBar = {PX, PY, PW, 4.f};
    SDL_RenderFillRectF(m_renderer, &topBar);

    // Buttons
    for (const auto& btn : btns) {
        bool hover = ((float)mx >= btn.rect.x && (float)mx <= btn.rect.x + btn.rect.w &&
                      (float)my >= btn.rect.y && (float)my <= btn.rect.y + btn.rect.h);
        if (hover)
            SDL_SetRenderDrawColor(m_renderer, 45, 75, 155, 235);
        else
            SDL_SetRenderDrawColor(m_renderer, 18, 25, 50, 220);
        SDL_RenderFillRectF(m_renderer, &btn.rect);
        Uint8 br = hover ? 100 : 50, bg = hover ? 138 : 72, bb = hover ? 215 : 128;
        SDL_SetRenderDrawColor(m_renderer, br, bg, bb, 200);
        SDL_RenderDrawRectF(m_renderer, &btn.rect);
    }
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    // Title
    if (m_ui.getFontBig())
        m_ui.renderText(m_renderer, m_ui.getFontBig(), "개발자 모드",
                        (float)m_screenW * 0.5f, PY + 10.f,
                        {155, 195, 255, 255}, true);

    // Button labels
    if (m_ui.getFont()) {
        for (const auto& btn : btns) {
            bool hover = ((float)mx >= btn.rect.x && (float)mx <= btn.rect.x + btn.rect.w &&
                          (float)my >= btn.rect.y && (float)my <= btn.rect.y + btn.rect.h);
            SDL_Color tc = hover ? SDL_Color{255, 255, 255, 255}
                                 : SDL_Color{155, 185, 225, 220};
            m_ui.renderText(m_renderer, m_ui.getFont(), btn.label,
                            btn.rect.x + btn.rect.w * 0.5f,
                            btn.rect.y + (btn.rect.h - 16.f) * 0.5f,
                            tc, true);
        }
        m_ui.renderText(m_renderer, m_ui.getFont(), "ESC : 닫기",
                        (float)m_screenW * 0.5f, PY + PH - 22.f,
                        {85, 105, 145, 175}, true);
    }
}

void Game::handleDevMenuClick(int mx, int my) {
    auto btns = buildDevMenu(m_screenW, m_screenH);
    for (const auto& btn : btns) {
        if ((float)mx < btn.rect.x || (float)mx > btn.rect.x + btn.rect.w) continue;
        if ((float)my < btn.rect.y || (float)my > btn.rect.y + btn.rect.h) continue;

        m_devMenuOpen  = false;
        m_lives        = 3;
        m_deathFade    = 0.f;
        m_deathState   = 0;
        m_grabbedRock  = -1;
        m_stellaTimer  = 0.f;
        m_stellaText.clear();

        if (btn.action == -1) {
            m_scene = Scene::SolarMap;
        } else if (btn.action == 100) {
            m_scene       = Scene::Ending;
            m_endingTimer = 0.f;
        } else if (btn.action >= 0 && btn.action < 8) {
            // Planet surface
            int pid = DEV_DISPLAY_ORDER[btn.action];
            loadPlanet(pid);
            m_scene = Scene::Playing;
        } else if (btn.action >= 10 && btn.action < 18) {
            // Planet base interior
            int pid = DEV_DISPLAY_ORDER[btn.action - 10];
            loadPlanet(pid);
            m_player.pos            = {640.f, 560.f};
            m_player.vel            = {};
            m_player.externalVel    = {};
            m_player.playerFriction = 1.0f;
            m_player.speedMult      = 1.0f;
            m_basePlayerPos         = {640.f, 560.f};
            m_baseTimer             = 0.f;
            m_puzzle.warpGate.active = true;
            m_puzzle.warpGate.glowTimer     = 0.f;
            m_puzzle.warpGate.particleTimer = 0.f;
            m_scene = Scene::BaseInterior;
        }
        break;
    }
}

void Game::updateMarsMeteorites(float dt) {
    // Update existing meteors
    for (auto& met : m_marsMeteorites) {
        if (!met.active) continue;

        if (met.warnTimer > 0.f) {
            met.warnTimer -= dt;
            float progress = 1.f - met.warnTimer / 1.5f;
            met.y = -50.f + (met.landY + 50.f) * progress;

            if (met.warnTimer <= 0.f) {
                met.warnTimer = 0.f;
                met.landed    = true;
                met.dustTimer = 1.8f;

                // Spawn dust particles
                for (int i = 0; i < 10; i++) {
                    MeteorDust d;
                    d.x = met.targetX + (float)((rand() % 20) - 10);
                    d.y = met.landY;
                    float angle = (float)i / 10.f * 6.28318f;
                    float speed = 40.f + (float)(rand() % 70);
                    d.vx    = std::cos(angle) * speed;
                    d.vy    = -std::abs(std::sin(angle)) * speed - 40.f;
                    d.life  = 0.7f + (float)(rand() % 8) * 0.1f;
                    d.maxLife = d.life;
                    met.dust.push_back(d);
                }

                // Check player hit (AABB around impact point)
                AABB meteorHit = {met.targetX - 16.f, met.landY - 22.f, 32.f, 32.f};
                AABB playerAABB = m_player.getAABB();
                if (meteorHit.intersects(playerAABB)) {
                    loseLife();
                    // Knockback away from impact
                    float kx = (playerAABB.x + playerAABB.w * 0.5f > met.targetX) ? 160.f : -160.f;
                    m_player.vel = {kx, -180.f};
                }

                // Check meteor plate
                if (!m_marsMeteorDoorOpen &&
                    m_marsMeteorPlateIdx >= 0 &&
                    m_marsMeteorPlateIdx < (int)m_puzzle.plates.size()) {
                    const AABB& plateArea = m_puzzle.plates[m_marsMeteorPlateIdx].area;
                    if (meteorHit.intersects(plateArea)) {
                        m_marsMeteorDoorOpen = true;
                        m_ui.showNotification("운석이 압력판을 눌렀다! 문이 열렸어!", NotifType::Warning);
                    }
                }
            }
        } else if (met.landed) {
            met.dustTimer -= dt;
            for (auto& d : met.dust) {
                d.x  += d.vx * dt;
                d.y  += d.vy * dt;
                d.vy += 220.f * dt; // gravity on dust
                d.life -= dt;
            }
            met.dust.erase(
                std::remove_if(met.dust.begin(), met.dust.end(),
                               [](const MeteorDust& d){ return d.life <= 0.f; }),
                met.dust.end());
            if (met.dustTimer <= 0.f && met.dust.empty())
                met.active = false;
        }
    }

    // Remove inactive meteors
    m_marsMeteorites.erase(
        std::remove_if(m_marsMeteorites.begin(), m_marsMeteorites.end(),
                       [](const Meteor& m){ return !m.active; }),
        m_marsMeteorites.end());

    // Keep meteor door open once triggered
    if (3 < (int)m_puzzle.doors.size())
        m_puzzle.doors[3].open = m_marsMeteorDoorOpen;

    // Countdown to next wave
    m_marsNextMeteor -= dt;
    if (m_marsNextMeteor <= 0.f) {
        int count = 3 + (rand() % 3); // 3-5 meteors
        for (int i = 0; i < count; i++) {
            Meteor met;
            met.targetX  = 100.f + (float)(rand() % 950);
            met.landY    = 575.f;
            met.x        = met.targetX;
            met.y        = -50.f;
            met.warnTimer = 1.5f;
            met.fallSpeed = (met.landY + 50.f) / 1.5f;
            met.active   = true;
            met.landed   = false;
            met.dustTimer = 0.f;
            m_marsMeteorites.push_back(met);
        }
        m_marsNextMeteor = 5.f + (float)(rand() % 30) / 10.f; // 5-8s
    }
}

void Game::updateVenusClouds(float dt) {
    AABB playerAABB = m_player.getAABB();
    for (auto& c : m_venusClouds) {
        c.wobble += dt;
        if (c.stunTimer > 0.f) { c.stunTimer -= dt; continue; }

        float step = c.speed * dt;
        c.t += c.forward ? step : -step;
        if (c.t >= 1.f) { c.t = 1.f; c.forward = false; }
        if (c.t <= 0.f) { c.t = 0.f; c.forward = true;  }

        c.pos.x = c.startPos.x + (c.endPos.x - c.startPos.x) * c.t;
        c.pos.y = c.startPos.y + (c.endPos.y - c.startPos.y) * c.t;

        // Collide with player
        if (c.getAABB().intersects(playerAABB) && m_deathState == 0)
            loseLife();

        // Collide with rocks → stun cloud 2s
        for (const auto& rock : m_puzzle.rocks) {
            if (!rock.active) continue;
            if (c.getAABB().intersects(rock.getAABB())) {
                c.stunTimer = 2.f;
                if (!c.stunShown) {
                    c.stunShown = true;
                    m_ui.showNotification("바위로 구름을 막았다!", NotifType::Normal);
                }
                break;
            }
        }
    }
}

void Game::updateJupiterGimmick(float dt) {
    static const float VORTEX_CX = 576.f, VORTEX_CY = 455.f;
    static const float DEATH_R   = 65.f,  WARN_R    = 180.f;
    static const float WIND_PERIOD= 20.f, WARN_BEFORE= 3.f;
    static const float WIND_FORCE = 55.f;

    m_jupiterVortexTimer += dt;

    // Wind cycle: 17s active → 3s warning (next direction) → switch
    m_jupiterWindCycle += dt;
    if (m_jupiterWindCycle >= WIND_PERIOD) {
        m_jupiterWindCycle -= WIND_PERIOD;
        m_jupiterWindDir = (m_jupiterWindDir + 1) % 4;
    }
    float timeLeft = WIND_PERIOD - m_jupiterWindCycle;
    m_jupiterWindWarning = (timeLeft <= WARN_BEFORE);
    m_jupiterWindActive  = !m_jupiterWindWarning;

    // Warning notification: once per transition
    if (m_jupiterWindWarning && !m_jupiterPrevWarning) {
        static const char* COMING[4] = {"동풍","서풍","남풍","북풍"};
        int nextDir = (m_jupiterWindDir + 1) % 4;
        char buf[48];
        std::snprintf(buf, sizeof(buf), "바람이 바뀐다! %s 예고!", COMING[nextDir]);
        m_ui.showNotification(buf, NotifType::Warning);
        if (m_stellaTimer <= 0.f) {
            m_stellaText  = "바람이 불어온다! 벽에 붙어!";
            m_stellaTimer = 2.5f;
        }
    }
    m_jupiterPrevWarning = m_jupiterWindWarning;

    // Wind force
    Vec2 windForce = {};
    if (m_jupiterWindActive) {
        switch (m_jupiterWindDir) {
            case 0: windForce = {WIND_FORCE,  0.f};       break; // East
            case 1: windForce = {-WIND_FORCE, 0.f};       break; // West
            case 2: windForce = {0.f,  WIND_FORCE};       break; // South
            case 3: windForce = {0.f, -WIND_FORCE};       break; // North
        }
    }

    // Vortex pull toward center
    float dx = m_player.pos.x - VORTEX_CX;
    float dy = m_player.pos.y - VORTEX_CY;
    float dist = std::sqrt(dx * dx + dy * dy);

    m_jupiterVortexWarn   = false;
    m_jupiterVortexDanger = false;

    if (dist < 0.1f) dist = 0.1f;  // avoid divide-by-zero

    if (dist < DEATH_R) {
        m_jupiterVortexDanger = true;
        if (m_deathState == 0) loseLife();
    } else if (dist < WARN_R) {
        m_jupiterVortexWarn = true;
        float t = 1.f - (dist - DEATH_R) / (WARN_R - DEATH_R);  // 0=edge, 1=death
        float pullStr = t * t * 95.f;
        windForce.x += (-dx / dist) * pullStr;
        windForce.y += (-dy / dist) * pullStr;

        // First-time vortex hint
        if (!(m_jupiterHints & 1)) {
            m_jupiterHints |= 1;
            m_ui.showNotification("소용돌이에 너무 가까워! 반대 방향으로 빠져나와!", NotifType::Danger);
        }
    }

    m_player.externalVel       = windForce;
    m_puzzle.rockExternalForce = {windForce.x * 2.5f, windForce.y * 2.5f};

    // Delayed entry hints
    if (!(m_jupiterHints & 2) && m_jupiterVortexTimer > 5.f) {
        m_jupiterHints |= 2;
        m_ui.showNotification("바위가 엄청 무거워. 바람을 이용해 압력판으로 날려봐!", NotifType::Normal);
    }
    if (!(m_jupiterHints & 4) && m_jupiterVortexTimer > 12.f) {
        m_jupiterHints |= 4;
        m_ui.showNotification("압력판 2개를 동시에 눌러야 문이 열려!", NotifType::Normal);
    }
}

void Game::updateMercurySolarFlare(float dt) {
    const float WARN_START  = 27.f;
    const float FLARE_START = 30.f;
    const float CYCLE_END   = 40.f;
    const float BEAM_SPEED  = 80.f;    // slower — more dodgeable
    const float BEAM_H      = 28.f;
    const float MIN_GAP     = 100.f;   // minimum gap between beams (3+ tiles = 96px)
    const float MAX_GAP     = 180.f;

    m_mercurySolarCycle += dt;

    m_mercurySolarWarning = (m_mercurySolarCycle >= WARN_START &&
                              m_mercurySolarCycle < FLARE_START);

    // Activate flare: 2 beams with randomized gap each time
    if (m_mercurySolarCycle >= FLARE_START && !m_mercurySolarFlareActive) {
        m_mercurySolarFlareActive = true;
        m_solarFlareTimer = 0.f;
        m_solarBeams.clear();

        // Randomize gap; guarantee minimum 3-tile clearance
        float gap = MIN_GAP + (float)(rand() % (int)(MAX_GAP - MIN_GAP + 1));

        // beam[0]: lower beam — enters screen first (starts just above top)
        SolarBeam b0;
        b0.y = -BEAM_H;
        b0.hitPlayer = false;
        // beam[1]: upper beam — enters screen second (further above = larger gap)
        SolarBeam b1;
        b1.y = -(2.f * BEAM_H + gap);
        b1.hitPlayer = false;
        m_solarBeams.push_back(b0);
        m_solarBeams.push_back(b1);

        m_ui.showNotification("☀ 태양 플레어 발동!", NotifType::Danger);
    }

    // Flare active: move beams downward
    if (m_mercurySolarFlareActive) {
        m_solarFlareTimer += dt;
        m_solarFlareTint = std::min(m_solarFlareTint + dt * 3.f, 1.f);

        float playerScreenY = m_player.pos.y - m_camY;
        bool anyActive = false;
        for (auto& b : m_solarBeams) {
            b.y += BEAM_SPEED * dt;
            if (b.y < (float)m_screenH + BEAM_H) anyActive = true;

            if (!b.hitPlayer && m_deathState == 0) {
                if (playerScreenY + PLAYER_HALF * 2.f > b.y &&
                    playerScreenY < b.y + BEAM_H) {
                    b.hitPlayer = true;
                    loseLife();
                }
            }
        }

        if (!anyActive) {
            m_mercurySolarFlareActive = false;
            m_solarBeams.clear();
        }
    }

    if (!m_mercurySolarFlareActive)
        m_solarFlareTint = std::max(m_solarFlareTint - dt * 1.5f, 0.f);

    if (m_mercurySolarCycle >= CYCLE_END) {
        m_mercurySolarCycle = 0.f;
        m_mercurySolarWarning = false;
    }
}

void Game::loseLife() {
    if (m_deathState != 0) return;
    m_lives--;
    if (m_lives == 1)
        m_ui.showNotification("⚠ 목숨 1개 남았어!", NotifType::Danger);
    m_deathFade = 0.f;
    m_deathState = 1;
    const PlanetLayout& L = LAYOUTS[m_currentPlanet];
    m_respawnPos = {L.startX, L.startY};
}

void Game::updateGameOver(float dt) {
    m_gameOverTimer += dt;
}

void Game::renderGameOver() {
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    drawStarfield(m_renderer, m_screenW, m_screenH, m_gameOverTimer * 0.3f);

    float fade = std::min(m_gameOverTimer / 0.8f, 1.f);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // Dim overlay
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, (Uint8)(180 * fade));
    SDL_FRect full = {0.f, 0.f, (float)m_screenW, (float)m_screenH};
    SDL_RenderFillRectF(m_renderer, &full);

    // Red pulse ring
    float ring = std::fmod(m_gameOverTimer * 0.8f, 1.f);
    SDL_SetRenderDrawColor(m_renderer, 180, 30, 30, (Uint8)(90 * (1.f - ring) * fade));
    float rr = ring * 400.f;
    SDL_FRect rrf = {m_screenW*0.5f - rr, m_screenH*0.5f - rr, rr*2.f, rr*2.f};
    SDL_RenderFillRectF(m_renderer, &rrf);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    if (m_ui.getFontBig()) {
        float pulse = 0.85f + 0.15f * std::sin(m_gameOverTimer * 2.5f);
        Uint8 a = (Uint8)(255 * fade * pulse);
        m_ui.renderText(m_renderer, m_ui.getFontBig(), "GAME OVER",
                        m_screenW * 0.5f, m_screenH * 0.42f, {220, 50, 50, a}, true);
    }
    if (m_ui.getFont()) {
        float blink = 0.55f + 0.45f * std::sin(m_gameOverTimer * 3.f);
        Uint8 ba = (Uint8)(220 * fade * blink);
        m_ui.renderText(m_renderer, m_ui.getFont(), "[ ENTER / SPACE: 처음으로 ]",
                        m_screenW * 0.5f, m_screenH * 0.58f, {200, 180, 180, ba}, true);
    }
}
