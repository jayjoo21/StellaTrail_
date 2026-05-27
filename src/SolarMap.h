#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "Physics.h"

class SolarMap {
public:
    void init(int screenW, int screenH);
    void handleEvent(const SDL_Event& e);
    void update(float dt);
    void render(SDL_Renderer* r, TTF_Font* font, TTF_Font* fontBig);

    // Called by Game when a planet is cleared
    void setPlanetCleared(int idx);
    void setPlanetUnlocked(int idx);
    bool isUnlocked(int idx) const { return m_unlocked[idx]; }
    bool isCleared(int idx)  const { return m_cleared[idx]; }

    // Returns selected planet index (-1 if none pending)
    int consumeSelectedPlanet();

private:
    int   m_sw = 1280, m_sh = 720;
    float m_timer   = 0.f;
    bool  m_cleared[8]  = {};
    bool  m_unlocked[8] = {false,false,false,true,false,false,false,false}; // Mars start

    int   m_cursor   = 0;  // index into m_unlocked list
    int   m_pending  = -1;

    // Visual constants defined in SolarMap.cpp

    Vec2  planetPos(int idx) const;
    void  drawSun(SDL_Renderer* r);
    void  drawOrbit(SDL_Renderer* r, int idx);
    void  drawPlanet(SDL_Renderer* r, int idx, TTF_Font* font);
    void  drawSelection(SDL_Renderer* r);
    void  drawInfo(SDL_Renderer* r, TTF_Font* font, TTF_Font* fontBig);
    void  renderText(SDL_Renderer* r, TTF_Font* font,
                     const char* text, float x, float y,
                     SDL_Color col, bool centered = false);

    // Cycle cursor to prev/next unlocked planet
    void cycleCursor(int dir);
    int  cursorPlanetIdx() const; // planet index at current cursor
};
