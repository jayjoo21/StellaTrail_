#include "SolarMap.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

static const float DEG2RAD     = 3.14159f / 180.f;
static const int   PLANET_R[8] = {5, 8, 9, 7, 18, 15, 11, 11};
static const float ORBIT_R[8]  = {60,88,115,148,188,220,252,280};
static const float INIT_ANG[8] = {0,45,90,135,180,225,270,315};
static const float ORBIT_SPD[8]= {0.08f,0.06f,0.05f,0.04f,0.02f,0.015f,0.01f,0.008f};

void SolarMap::init(int sw, int sh) {
    m_sw = sw; m_sh = sh;
    m_cursor = 0; // Will point to first unlocked (Mars)
    // Find first unlocked planet for cursor
    for (int i = 0; i < 8; i++) {
        if (m_unlocked[Planets::VISIT_ORDER[i]]) { m_cursor = i; break; }
    }
}

void SolarMap::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_LEFT  || e.key.keysym.sym == SDLK_a) cycleCursor(-1);
        if (e.key.keysym.sym == SDLK_RIGHT || e.key.keysym.sym == SDLK_d) cycleCursor(+1);
        if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
            int pi = cursorPlanetIdx();
            if (pi >= 0 && m_unlocked[pi]) m_pending = pi;
        }
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;
        for (int i = 0; i < 8; i++) {
            if (!m_unlocked[i]) continue;
            Vec2 p = planetPos(i);
            float dx = mx - p.x, dy = my - p.y;
            float r = (float)PLANET_R[i] + 8.f;
            if (dx*dx + dy*dy <= r*r) { m_pending = i; break; }
        }
    }
}

void SolarMap::update(float dt) {
    m_timer += dt;
}

void SolarMap::render(SDL_Renderer* r, TTF_Font* font, TTF_Font* fontBig) {
    // Background
    SDL_SetRenderDrawColor(r, 5, 8, 20, 255);
    SDL_RenderClear(r);

    // Stars (static seeded)
    srand(12345);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 200; i++) {
        float x = (float)(rand() % m_sw);
        float y = (float)(rand() % m_sh);
        float tw = (std::sin(m_timer * 1.5f + i * 0.8f) + 1.f) * 0.5f;
        Uint8 a  = (Uint8)(80 + 120 * tw);
        SDL_SetRenderDrawColor(r, 255, 255, 240, a);
        SDL_FRect s = {x, y, (i%5==0) ? 2.f : 1.f, (i%5==0) ? 2.f : 1.f};
        SDL_RenderFillRectF(r, &s);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    // Orbits
    for (int i = 0; i < 8; i++) drawOrbit(r, i);
    // Sun
    drawSun(r);
    // Planets
    for (int i = 0; i < 8; i++) drawPlanet(r, i, font);
    // Selection cursor
    drawSelection(r);
    // Info panel
    drawInfo(r, font, fontBig);
}

Vec2 SolarMap::planetPos(int idx) const {
    float angle = (INIT_ANG[idx] + m_timer * ORBIT_SPD[idx] * (180.f / 3.14159f))
                  * DEG2RAD;
    float cx = m_sw * 0.5f;
    float cy = m_sh * 0.5f + 10.f;
    return {cx + ORBIT_R[idx] * std::cos(angle),
            cy + ORBIT_R[idx] * std::sin(angle)};
}

void SolarMap::drawSun(SDL_Renderer* r) {
    float cx = m_sw * 0.5f, cy = m_sh * 0.5f + 10.f;
    float pulse = (std::sin(m_timer * 2.f) + 1.f) * 0.5f;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // Corona layers
    for (int ring = 5; ring >= 1; ring--) {
        float rad = 28.f + ring * 6.f + pulse * 3.f;
        Uint8 a = (Uint8)(20 + ring * 8);
        SDL_SetRenderDrawColor(r, 255, 200, 50, a);
        SDL_FRect rf = {cx-rad, cy-rad, rad*2.f, rad*2.f};
        SDL_RenderFillRectF(r, &rf);
    }
    // Core
    SDL_SetRenderDrawColor(r, 255, 220, 80, 255);
    SDL_FRect core = {cx-18.f, cy-18.f, 36.f, 36.f};
    SDL_RenderFillRectF(r, &core);
    SDL_SetRenderDrawColor(r, 255, 250, 200, 255);
    SDL_FRect coreInner = {cx-10.f, cy-10.f, 20.f, 20.f};
    SDL_RenderFillRectF(r, &coreInner);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void SolarMap::drawOrbit(SDL_Renderer* r, int idx) {
    float cx = m_sw * 0.5f, cy = m_sh * 0.5f + 10.f;
    float rad = ORBIT_R[idx];

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 60, 70, 100, 60);
    // Approximate circle with 32-segment line loop
    const int SEGS = 32;
    for (int s = 0; s < SEGS; s++) {
        float a0 = s     * 2.f * 3.14159f / SEGS;
        float a1 = (s+1) * 2.f * 3.14159f / SEGS;
        SDL_RenderDrawLineF(r,
            cx + rad * std::cos(a0), cy + rad * std::sin(a0),
            cx + rad * std::cos(a1), cy + rad * std::sin(a1));
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void SolarMap::drawPlanet(SDL_Renderer* r, int idx, TTF_Font* font) {
    Vec2 pos = planetPos(idx);
    float prad = (float)PLANET_R[idx];
    const auto& col = Planets::ALL[idx].ambientColor;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    if (!m_unlocked[idx]) {
        // Locked: dark grey
        SDL_SetRenderDrawColor(r, 40, 40, 50, 160);
        SDL_FRect rf = {pos.x-prad, pos.y-prad, prad*2.f, prad*2.f};
        SDL_RenderFillRectF(r, &rf);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        return;
    }

    if (m_cleared[idx]) {
        // Cleared: bright + check glow
        SDL_SetRenderDrawColor(r, col.r, col.g, col.b, 255);
        SDL_FRect rf = {pos.x-prad, pos.y-prad, prad*2.f, prad*2.f};
        SDL_RenderFillRectF(r, &rf);
        // Glow ring
        float glow = (std::sin(m_timer * 3.f + idx) + 1.f) * 0.5f;
        SDL_SetRenderDrawColor(r, col.r, col.g, col.b, (Uint8)(60 * glow));
        SDL_FRect grf = {pos.x-prad-6.f, pos.y-prad-6.f, (prad+6.f)*2.f, (prad+6.f)*2.f};
        SDL_RenderFillRectF(r, &grf);
        // Cleared checkmark (small cross)
        SDL_SetRenderDrawColor(r, 255, 255, 255, 220);
        SDL_RenderDrawLineF(r, pos.x-3.f, pos.y, pos.x, pos.y+3.f);
        SDL_RenderDrawLineF(r, pos.x, pos.y+3.f, pos.x+5.f, pos.y-3.f);
    } else {
        // Unlocked not cleared: normal color
        SDL_SetRenderDrawColor(r, col.r, col.g, col.b, 220);
        SDL_FRect rf = {pos.x-prad, pos.y-prad, prad*2.f, prad*2.f};
        SDL_RenderFillRectF(r, &rf);
    }

    // Saturn ring
    if (idx == 5) {
        SDL_SetRenderDrawColor(r, 210, 190, 120, 100);
        SDL_FRect ring = {pos.x-prad-8.f, pos.y-3.f, (prad+8.f)*2.f, 6.f};
        SDL_RenderFillRectF(r, &ring);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    // Name label
    if (font) {
        SDL_Color tc = m_unlocked[idx]
            ? SDL_Color{220, 220, 240, 255}
            : SDL_Color{80, 80, 90, 160};
        // Short Korean name for display
        const char* knames[8] = {"수성","금성","지구","화성","목성","토성","천왕성","해왕성"};
        renderText(r, font, knames[idx], pos.x, pos.y + prad + 4.f, tc, true);
    }
}

void SolarMap::drawSelection(SDL_Renderer* r) {
    int pi = cursorPlanetIdx();
    if (pi < 0 || !m_unlocked[pi]) return;

    Vec2 pos = planetPos(pi);
    float prad = (float)PLANET_R[pi] + 6.f;
    float pulse = (std::sin(m_timer * 5.f) + 1.f) * 0.5f;
    Uint8 a = (Uint8)(160 + 95 * pulse);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, a);
    // Selection box outline
    SDL_FRect sel = {pos.x-prad, pos.y-prad, prad*2.f, prad*2.f};
    SDL_RenderDrawRectF(r, &sel);
    // Corner accents
    float cs = 4.f;
    SDL_RenderDrawLineF(r, pos.x-prad, pos.y-prad, pos.x-prad+cs, pos.y-prad);
    SDL_RenderDrawLineF(r, pos.x-prad, pos.y-prad, pos.x-prad, pos.y-prad+cs);
    SDL_RenderDrawLineF(r, pos.x+prad, pos.y-prad, pos.x+prad-cs, pos.y-prad);
    SDL_RenderDrawLineF(r, pos.x+prad, pos.y-prad, pos.x+prad, pos.y-prad+cs);
    SDL_RenderDrawLineF(r, pos.x-prad, pos.y+prad, pos.x-prad+cs, pos.y+prad);
    SDL_RenderDrawLineF(r, pos.x-prad, pos.y+prad, pos.x-prad, pos.y+prad-cs);
    SDL_RenderDrawLineF(r, pos.x+prad, pos.y+prad, pos.x+prad-cs, pos.y+prad);
    SDL_RenderDrawLineF(r, pos.x+prad, pos.y+prad, pos.x+prad, pos.y+prad-cs);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void SolarMap::drawInfo(SDL_Renderer* r, TTF_Font* font, TTF_Font* fontBig) {
    // Bottom info panel
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_FRect panel = {m_sw/2.f - 280.f, m_sh - 100.f, 560.f, 90.f};
    SDL_RenderFillRectF(r, &panel);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    int pi = cursorPlanetIdx();
    if (pi < 0) return;

    const PlanetPhysics& p = Planets::ALL[pi];
    const char* knames[8] = {"수성(Mercury)","금성(Venus)","지구(Earth)","화성(Mars)",
                              "목성(Jupiter)","토성(Saturn)","천왕성(Uranus)","해왕성(Neptune)"};

    if (fontBig) {
        SDL_Color nc = {p.ambientColor.r, p.ambientColor.g, p.ambientColor.b, 255};
        renderText(r, fontBig, knames[pi], m_sw/2.f, m_sh - 95.f, nc, true);
    }
    if (font) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "중력: %.1f m/s²   마찰: %.2f   %s",
                      p.gravity, p.friction,
                      (Planets::GIMMICK_DESC[pi][0] ? Planets::GIMMICK_DESC[pi] : "기믹: 없음"));
        renderText(r, font, buf, m_sw/2.f, m_sh - 62.f, {200, 210, 230, 255}, true);

        if (m_cleared[pi]) {
            renderText(r, font, "✓ 탐험 완료", m_sw/2.f, m_sh - 40.f,
                       {150, 255, 150, 255}, true);
        } else if (m_unlocked[pi]) {
            float pulse = (std::sin(m_timer * 3.f) + 1.f) * 0.5f;
            SDL_Color ec = {255, 220, 80, (Uint8)(160 + 95 * pulse)};
            renderText(r, font, "ENTER / SPACE: 행성 진입", m_sw/2.f, m_sh - 40.f, ec, true);
        } else {
            renderText(r, font, "잠김", m_sw/2.f, m_sh - 40.f, {100, 100, 120, 200}, true);
        }

        // Nav hint
        renderText(r, font, "← → : 행성 선택", 20.f, m_sh - 30.f, {120, 130, 150, 200});
    }
}

void SolarMap::cycleCursor(int dir) {
    // Find unlocked planets in visit order
    int count = 0;
    int positions[8];
    for (int v = 0; v < 8; v++) {
        int pi = Planets::VISIT_ORDER[v];
        if (m_unlocked[pi]) positions[count++] = v;
    }
    if (count == 0) return;

    // Find current cursor position in the unlocked list
    int cur = 0;
    for (int i = 0; i < count; i++) {
        if (positions[i] == m_cursor) { cur = i; break; }
    }
    cur = (cur + dir + count) % count;
    m_cursor = positions[cur];
}

int SolarMap::cursorPlanetIdx() const {
    if (m_cursor < 0 || m_cursor >= 8) return -1;
    return Planets::VISIT_ORDER[m_cursor];
}

int SolarMap::consumeSelectedPlanet() {
    int p = m_pending;
    m_pending = -1;
    return p;
}

void SolarMap::setPlanetCleared(int idx) {
    m_cleared[idx] = true;
}

void SolarMap::setPlanetUnlocked(int idx) {
    m_unlocked[idx] = true;
    // Update cursor to newly unlocked if nothing else available
    for (int v = 0; v < 8; v++) {
        if (Planets::VISIT_ORDER[v] == idx) {
            m_cursor = v;
            break;
        }
    }
}

void SolarMap::renderText(SDL_Renderer* r, TTF_Font* font, const char* text,
                          float x, float y, SDL_Color col, bool centered) {
    if (!font || !text) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, col);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_SetTextureAlphaMod(tex, col.a);
        SDL_FRect dst = {centered ? x - surf->w/2.f : x, y,
                         (float)surf->w, (float)surf->h};
        SDL_RenderCopyF(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}
