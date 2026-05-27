#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include "Physics.h"
#include "PuzzleSystem.h"

struct Popup {
    std::string text;
    float x, y;
    float life  = 2.f;
    float alpha = 255.f;
};

class UI {
public:
    bool init(SDL_Renderer* r, const std::string& fontPath);
    void shutdown();
    void update(float dt);

    void render(SDL_Renderer* r,
                int totalCollected, int totalParts,
                int screenW, int screenH,
                int planetCollected, int planetParts,
                int planetIdx, const PlanetPhysics& physics,
                // minimap data
                const std::vector<RigidBody>* rocks,
                const std::vector<Part>* parts,
                const std::vector<PressurePlate>* plates,
                Vec2 playerPos, Vec2 baseEntrPos,
                int mapW, int mapH,
                bool minimapDisabled,
                // gimmick edge glow
                bool gimmickActive,
                // wind warning
                bool windWarning,
                // stella speech bubble
                const std::string& stellaText, float stellaAlpha);

    void renderTitleScreen(SDL_Renderer* r, int sw, int sh, float timer);
    void renderPrologueLine(SDL_Renderer* r, const char* line, int lineIdx,
                            float cx, float cy, Uint8 alpha);
    void renderSkipHint(SDL_Renderer* r, int sw, int sh, Uint8 alpha);
    void renderPlanetIntro(SDL_Renderer* r, int sw, int sh,
                           const PlanetPhysics& p, int idx, Uint8 alpha);
    void renderWarpActivation(SDL_Renderer* r, int sw, int sh,
                              const PlanetPhysics& p, float timer);
    void renderEnding(SDL_Renderer* r, int sw, int sh, float timer);

    void showPopup(const std::string& msg, float x, float y);
    void setShipStage(int s) { m_shipStage = s; }

    TTF_Font* getFont()    const { return m_font; }
    TTF_Font* getFontBig() const { return m_fontBig; }

    void renderText(SDL_Renderer* r, TTF_Font* font,
                    const std::string& text, float x, float y,
                    SDL_Color color, bool centered = false);

private:
    TTF_Font* m_font    = nullptr;
    TTF_Font* m_fontBig = nullptr;
    int  m_shipStage    = 0;
    std::vector<Popup> m_popups;

    void renderHUD(SDL_Renderer* r,
                   int planetDone, int planetParts,
                   int planetIdx, const PlanetPhysics& physics,
                   int sw, int sh);
    void renderMinimap(SDL_Renderer* r, int sw, int sh,
                       const std::vector<RigidBody>& rocks,
                       const std::vector<Part>& parts,
                       const std::vector<PressurePlate>& plates,
                       Vec2 playerPos, Vec2 baseEntrPos,
                       int mapW, int mapH, bool disabled);
    void renderEdgeGlow(SDL_Renderer* r, int sw, int sh,
                        SDL_Color col, float alpha);
    void renderStellaBubble(SDL_Renderer* r, int sw, int sh,
                            const std::string& text, float alpha);
    void renderWindWarning(SDL_Renderer* r, int sw, int sh, float timer);
};
