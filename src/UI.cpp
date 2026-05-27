#include "UI.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace Planets {
    const char* STORY[8] = {
        "태양에 너무 가까워... 뜨거운 바위들이 저절로 움직이고 있어.",
        "짙은 대기로 앞이 잘 안 보여. 조심해야겠어.",
        "인류의 흔적이 남아있다. 폐허가 된 도시... 마음이 아파.",
        "여기서 시작이야. 붉은 사막 어딘가에 게이트 부품이 있을 거야.",
        "중력이 너무 강해... 몸이 무거워. 바위도 엄청 무겁겠지.",
        "링 위를 걷고 있어. 얼음이 미끄러워서 조심해야 해.",
        "모든 게 옆으로 기울어진 느낌이야. 이상한 행성이야.",
        "마지막이야. 강풍이 몰아치고 있어. 여기서 끝내자.",
    };
    const char* GIMMICK_DESC[8] = {
        "기믹: 바위 자동 이동 (3초 주기)",
        "기믹: 시야 방해 + 미니맵 비활성화",
        "기믹: 감성 연출",
        "",
        "기믹: 고중력 / 저속 / 바위 3배 무게",
        "기믹: 극미끄러움 (마찰 없음)",
        "기믹: 좌측 편향 드리프트",
        "기믹: 주기 강풍 (5초 주기, 3초 예고)",
    };
    const char* BASE_HINTS[8] = {
        "⚠ 외부경고: 바위가 3초마다 자동이동합니다.\n타이밍을 맞춰 압력판에 올려라.",
        "⚠ 외부경고: 시야 방해, 미니맵 불능.\n화면 가장자리까지 천천히 탐색하라.",
        "⚠ 외부안내: 감성 오브젝트 구역.\n행성의 흔적을 천천히 살펴보라.",
        "⚠ 튜토리얼: WASD로 이동. 바위를 밀어\n압력판에 올리면 문이 열린다.",
        "⚠ 외부경고: 고중력 구역. 바위 질량 3배.\n이동속도 감소. 힘을 더 써야 한다.",
        "⚠ 외부경고: 마찰 없음. 바위가 멈추지 않는다.\n방향과 힘 조절 필수.",
        "⚠ 외부경고: 좌측 편향 중력.\n모든 물체가 왼쪽으로 밀린다. 역방향 활용.",
        "⚠ 외부경고: 강풍 주기 5초. 3초 전 경보 발령.\n바위를 방패로, 강풍 타이밍을 피하라.",
    };
}

bool UI::init(SDL_Renderer* /*r*/, const std::string& fontPath) {
    if (TTF_Init() < 0) return false;
    m_font    = TTF_OpenFont(fontPath.c_str(), 16);
    m_fontBig = TTF_OpenFont(fontPath.c_str(), 28);
    return true;
}

void UI::shutdown() {
    if (m_font)    { TTF_CloseFont(m_font);    m_font    = nullptr; }
    if (m_fontBig) { TTF_CloseFont(m_fontBig); m_fontBig = nullptr; }
    TTF_Quit();
}

void UI::update(float dt) {
    for (auto& p : m_popups) {
        p.life  -= dt;
        p.y     -= 22.f * dt;
        p.alpha  = std::max(0.f, std::min(p.life / 2.f * 255.f, 255.f));
    }
    m_popups.erase(
        std::remove_if(m_popups.begin(), m_popups.end(),
                       [](const Popup& p){ return p.life <= 0.f; }),
        m_popups.end());
}

void UI::showPopup(const std::string& msg, float x, float y) {
    m_popups.push_back({msg, x, y, 2.5f, 255.f});
}

void UI::render(SDL_Renderer* r,
                int totalCollected, int totalParts,
                int screenW, int screenH,
                int planetCollected, int planetParts,
                int planetIdx, const PlanetPhysics& physics,
                const std::vector<RigidBody>* rocks,
                const std::vector<Part>* parts,
                const std::vector<PressurePlate>* plates,
                Vec2 playerPos, Vec2 baseEntrPos,
                int mapW, int mapH,
                bool minimapDisabled,
                bool gimmickActive,
                bool windWarning,
                const std::string& stellaText, float stellaAlpha)
{
    // Edge glow when gimmick active
    if (gimmickActive) {
        const auto& ac = physics.ambientColor;
        renderEdgeGlow(r, screenW, screenH, ac, 0.55f);
    }

    // Wind warning overlay
    if (windWarning) {
        static float warnPulse = 0.f;
        warnPulse += 0.05f;
        SDL_Color wc = {255, 80, 80, 255};
        renderEdgeGlow(r, screenW, screenH, wc, 0.4f + 0.3f * std::sin(warnPulse * 6.f));
    }

    renderHUD(r, planetCollected, planetParts, planetIdx, physics, screenW, screenH);

    // Total parts bottom-left
    if (m_font) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "총 부품: %d / %d", totalCollected, totalParts);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 130);
        SDL_FRect bg = {8.f, (float)screenH - 36.f, 200.f, 28.f};
        SDL_RenderFillRectF(r, &bg);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        renderText(r, m_font, buf, 16.f, (float)screenH - 30.f, {190, 210, 240, 220});
    }

    // Minimap (top-right)
    if (rocks && parts && plates) {
        renderMinimap(r, screenW, screenH, *rocks, *parts, *plates,
                      playerPos, baseEntrPos, mapW, mapH, minimapDisabled);
    }

    // Wind warning text
    if (windWarning && m_fontBig) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 180, 0, 0, 160);
        SDL_FRect panel = {(float)screenW/2-180.f, 8.f, 360.f, 48.f};
        SDL_RenderFillRectF(r, &panel);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        renderText(r, m_fontBig, "⚠ 강풍 경보",
                   (float)screenW/2, 14.f, {255, 120, 80, 255}, true);
    }

    // Stella speech bubble
    if (!stellaText.empty() && stellaAlpha > 0.f) {
        renderStellaBubble(r, screenW, screenH, stellaText, stellaAlpha);
    }

    // Popups
    for (const auto& p : m_popups) {
        if (!m_font) continue;
        TTF_Font* f = (m_fontBig && p.life > 1.5f) ? m_fontBig : m_font;
        SDL_Color col = {255, 230, 100, (Uint8)p.alpha};
        renderText(r, f, p.text, p.x, p.y, col, true);
    }
}

void UI::renderHUD(SDL_Renderer* r,
                   int planetDone, int planetParts,
                   int planetIdx, const PlanetPhysics& physics,
                   int sw, int sh)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Top-left panel: planet name + gravity
    SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
    SDL_FRect panel = {8.f, 8.f, 240.f, 56.f};
    SDL_RenderFillRectF(r, &panel);
    const auto& ac = physics.ambientColor;
    SDL_SetRenderDrawColor(r, ac.r, ac.g, ac.b, 200);
    SDL_FRect accent = {8.f, 8.f, 4.f, 56.f};
    SDL_RenderFillRectF(r, &accent);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    if (m_font) {
        const char* knames[8] = {"수성","금성","지구","화성","목성","토성","천왕성","해왕성"};
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s (%s)", knames[planetIdx], physics.name);
        renderText(r, m_font, buf, 18.f, 14.f, {220, 225, 240, 255});
        std::snprintf(buf, sizeof(buf), "중력: %.1f m/s²", physics.gravity);
        renderText(r, m_font, buf, 18.f, 36.f, {170, 190, 215, 220});
    }

    // Planet part icons (below name panel)
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < planetParts; i++) {
        float ix = 18.f + i * 22.f;
        SDL_FRect icon = {ix, 70.f, 14.f, 14.f};
        if (i < planetDone) {
            SDL_SetRenderDrawColor(r, 255, 200, 50, 255);
        } else {
            SDL_SetRenderDrawColor(r, 55, 55, 60, 180);
        }
        SDL_RenderFillRectF(r, &icon);
        SDL_SetRenderDrawColor(r, 180, 160, 80, 120);
        SDL_RenderDrawRectF(r, &icon);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    (void)sw; (void)sh;
}

void UI::renderMinimap(SDL_Renderer* r, int sw, int sh,
                       const std::vector<RigidBody>& rocks,
                       const std::vector<Part>& parts,
                       const std::vector<PressurePlate>& plates,
                       Vec2 playerPos, Vec2 baseEntrPos,
                       int mapW, int mapH, bool disabled)
{
    const float mmW = 160.f, mmH = 120.f;
    const float mmX = (float)sw - mmW - 8.f;
    const float mmY = 8.f;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
    SDL_FRect bg = {mmX - 2.f, mmY - 2.f, mmW + 4.f, mmH + 4.f};
    SDL_RenderFillRectF(r, &bg);
    SDL_SetRenderDrawColor(r, 60, 80, 120, 220);
    SDL_RenderDrawRectF(r, &bg);

    if (disabled) {
        SDL_SetRenderDrawColor(r, 15, 15, 20, 200);
        SDL_FRect fill = {mmX, mmY, mmW, mmH};
        SDL_RenderFillRectF(r, &fill);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        if (m_font)
            renderText(r, m_font, "신호 없음",
                       mmX + mmW * 0.5f, mmY + mmH * 0.5f - 8.f,
                       {80, 80, 90, 180}, true);
        return;
    }

    SDL_SetRenderDrawColor(r, 20, 22, 30, 220);
    SDL_FRect fill = {mmX, mmY, mmW, mmH};
    SDL_RenderFillRectF(r, &fill);

    float scaleX = (mapW > 0) ? mmW / mapW : 1.f;
    float scaleY = (mapH > 0) ? mmH / mapH : 1.f;

    // Pressure plates: green
    SDL_SetRenderDrawColor(r, 80, 220, 80, 200);
    for (const auto& p : plates) {
        float px = mmX + (p.area.x + p.area.w * 0.5f) * scaleX;
        float py = mmY + (p.area.y + p.area.h * 0.5f) * scaleY;
        SDL_FRect dot = {px - 2.f, py - 2.f, 4.f, 4.f};
        SDL_RenderFillRectF(r, &dot);
    }

    // Rocks: brown
    SDL_SetRenderDrawColor(r, 160, 120, 60, 200);
    for (const auto& rock : rocks) {
        if (!rock.active) continue;
        float px = mmX + rock.pos.x * scaleX;
        float py = mmY + rock.pos.y * scaleY;
        SDL_FRect dot = {px - 2.f, py - 2.f, 4.f, 4.f};
        SDL_RenderFillRectF(r, &dot);
    }

    // Parts: yellow
    SDL_SetRenderDrawColor(r, 255, 220, 50, 240);
    for (const auto& pt : parts) {
        if (pt.collected) continue;
        float px = mmX + pt.pos.x * scaleX;
        float py = mmY + pt.pos.y * scaleY;
        SDL_FRect dot = {px - 3.f, py - 3.f, 6.f, 6.f};
        SDL_RenderFillRectF(r, &dot);
    }

    // Base entrance: blue
    {
        float px = mmX + baseEntrPos.x * scaleX;
        float py = mmY + baseEntrPos.y * scaleY;
        SDL_SetRenderDrawColor(r, 80, 140, 255, 220);
        SDL_FRect dot = {px - 3.f, py - 3.f, 6.f, 6.f};
        SDL_RenderFillRectF(r, &dot);
    }

    // Player: white
    {
        float px = mmX + playerPos.x * scaleX;
        float py = mmY + playerPos.y * scaleY;
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_FRect dot = {px - 3.f, py - 3.f, 6.f, 6.f};
        SDL_RenderFillRectF(r, &dot);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    if (m_font) {
        renderText(r, m_font, "MAP",
                   mmX + mmW * 0.5f, mmY + mmH + 2.f,
                   {100, 120, 160, 180}, true);
    }
}

void UI::renderEdgeGlow(SDL_Renderer* r, int sw, int sh, SDL_Color col, float alpha) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    Uint8 a = (Uint8)(255 * alpha);
    const float thickness = 24.f;
    // top
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, a);
    SDL_FRect top = {0, 0, (float)sw, thickness};
    SDL_RenderFillRectF(r, &top);
    // bottom
    SDL_FRect bot = {0, (float)sh - thickness, (float)sw, thickness};
    SDL_RenderFillRectF(r, &bot);
    // left
    SDL_FRect lft = {0, 0, thickness, (float)sh};
    SDL_RenderFillRectF(r, &lft);
    // right
    SDL_FRect rgt = {(float)sw - thickness, 0, thickness, (float)sh};
    SDL_RenderFillRectF(r, &rgt);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void UI::renderStellaBubble(SDL_Renderer* r, int sw, int sh,
                             const std::string& text, float alpha) {
    if (!m_font) return;
    float bx = (float)sw * 0.28f;
    float by = (float)sh * 0.72f;
    float bw = (float)sw * 0.50f;
    float bh = 72.f;
    Uint8 a = (Uint8)(255 * alpha);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(50 * alpha));
    SDL_FRect shadow = {bx + 4.f, by + 4.f, bw, bh};
    SDL_RenderFillRectF(r, &shadow);
    SDL_SetRenderDrawColor(r, 12, 16, 38, (Uint8)(210 * alpha));
    SDL_FRect bg = {bx, by, bw, bh};
    SDL_RenderFillRectF(r, &bg);
    SDL_SetRenderDrawColor(r, 90, 160, 255, (Uint8)(180 * alpha));
    SDL_RenderDrawRectF(r, &bg);
    // Tail (pointing left toward astronaut)
    float ty = by + bh * 0.5f;
    SDL_SetRenderDrawColor(r, 12, 16, 38, (Uint8)(210 * alpha));
    for (int i = 1; i <= 12; i++) {
        float hh = i * 0.38f;
        SDL_RenderDrawLineF(r, bx - (float)i, ty - hh, bx - (float)i, ty + hh);
    }
    SDL_SetRenderDrawColor(r, 90, 160, 255, (Uint8)(180 * alpha));
    SDL_RenderDrawLineF(r, bx, ty, bx - 12.f, ty - 4.f);
    SDL_RenderDrawLineF(r, bx, ty, bx - 12.f, ty + 4.f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    SDL_Color tc = {220, 235, 255, a};
    renderText(r, m_font, text, bx + bw * 0.5f, by + bh * 0.5f - 8.f, tc, true);
}

void UI::renderTitleScreen(SDL_Renderer* r, int sw, int sh, float timer) {
    if (m_fontBig) {
        renderText(r, m_fontBig, "STELLA TRAIL",
                   sw/2.f, sh/2.f - 90.f, {255, 230, 120, 255}, true);
        renderText(r, m_fontBig, "별의 여정",
                   sw/2.f, sh/2.f - 48.f, {180, 200, 255, 220}, true);
    }
    if (m_font) {
        float p = (std::sin(timer * 3.f) + 1.f) * 0.5f;
        SDL_Color ec = {200, 212, 255, (Uint8)(100 + 155 * p)};
        renderText(r, m_font, "PRESS  ENTER",
                   sw/2.f, sh/2.f - 6.f, ec, true);
        renderText(r, m_font, "WASD: 이동   바위를 압력판에 올려라   ESC: 뒤로",
                   sw/2.f, sh - 32.f, {90, 105, 130, 180}, true);
    }
}

void UI::renderPrologueLine(SDL_Renderer* r, const char* line, int lineIdx,
                             float cx, float cy, Uint8 alpha) {
    if (!line) return;
    SDL_Color col = {218, 222, 255, alpha};
    TTF_Font* f = m_font;
    if (!f) return;
    if (lineIdx == 0)             col = {178, 198, 255, alpha};
    else if (lineIdx == 4 || lineIdx == 5) col = {255, 198, 118, alpha};
    else if (lineIdx == 6)        col = {255, 255, 195, alpha};
    renderText(r, f, line, cx, cy, col, true);
}

void UI::renderSkipHint(SDL_Renderer* r, int sw, int sh, Uint8 alpha) {
    if (!m_font) return;
    renderText(r, m_font, "ENTER / 클릭 : 다음   ESC : 건너뛰기",
               sw/2.f, (float)sh - 28.f,
               {98, 112, 135, alpha}, true);
}

void UI::renderPlanetIntro(SDL_Renderer* r, int sw, int sh,
                            const PlanetPhysics& p, int idx, Uint8 alpha) {
    const char* knames[8] = {
        "수성","금성","지구","화성","목성","토성","천왕성","해왕성"
    };
    if (m_fontBig) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s  —  %s", knames[idx], p.name);
        SDL_Color nc = {p.ambientColor.r, p.ambientColor.g, p.ambientColor.b, alpha};
        renderText(r, m_fontBig, buf, sw/2.f, sh/2.f - 55.f, nc, true);
    }
    if (m_font) {
        renderText(r, m_font, Planets::STORY[idx],
                   sw/2.f, sh/2.f - 12.f, {220, 220, 240, alpha}, true);
        char phys[80];
        std::snprintf(phys, sizeof(phys),
                      "중력 %.2f m/s²   마찰 %.2f   충격량 %.1f×",
                      p.gravity, p.friction, p.impulseScale);
        renderText(r, m_font, phys, sw/2.f, sh/2.f + 18.f,
                   {160, 180, 200, alpha}, true);
        if (Planets::GIMMICK_DESC[idx][0]) {
            renderText(r, m_font, Planets::GIMMICK_DESC[idx],
                       sw/2.f, sh/2.f + 44.f, {255, 200, 80, alpha}, true);
        }
        float pulse = alpha > 200 ? 1.f : 0.5f;
        renderText(r, m_font, "SPACE 으로 계속",
                   sw/2.f, sh/2.f + 72.f,
                   {200, 200, 255, (Uint8)(120 * pulse)}, true);
    }
}

void UI::renderWarpActivation(SDL_Renderer* r, int sw, int sh,
                               const PlanetPhysics& p, float timer) {
    if (timer < 0.3f) return;
    float a = std::min((timer - 0.3f) / 0.4f, 1.f);
    Uint8 alpha = (Uint8)(255 * a);
    if (m_fontBig) {
        SDL_Color col = {p.ambientColor.r, p.ambientColor.g, p.ambientColor.b, alpha};
        renderText(r, m_fontBig, "워프 게이트가 열렸다!",
                   sw/2.f, sh/2.f - 30.f, col, true);
    }
    if (m_font && timer > 1.0f) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s 탐험 완료", p.name);
        renderText(r, m_font, buf, sw/2.f, sh/2.f + 15.f, {220, 220, 255, alpha}, true);
        if (timer > 1.8f) {
            renderText(r, m_font, "태양계 맵으로 귀환 중...",
                       sw/2.f, sh/2.f + 40.f,
                       {160, 160, 200, (Uint8)(alpha * std::min((timer-1.8f)/0.4f, 1.f))},
                       true);
        }
    }
}

void UI::renderEnding(SDL_Renderer* r, int sw, int sh, float timer) {
    float a = std::min(timer / 2.f, 1.f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(160 * a));
    SDL_FRect panel = {sw/2.f - 260.f, sh*0.72f, 520.f, 130.f};
    SDL_RenderFillRectF(r, &panel);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    if (m_fontBig && timer > 1.0f)
        renderText(r, m_fontBig, "집으로...",
                   sw/2.f, sh*0.74f, {255, 240, 180, (Uint8)(200 * a)}, true);
    if (m_font && timer > 1.5f) {
        renderText(r, m_font, "모든 행성의 워프 게이트를 수리했다!",
                   sw/2.f, sh*0.81f, {200, 220, 255, (Uint8)(200 * a)}, true);
        renderText(r, m_font, "스텔라는 드디어 집으로 돌아갈 수 있게 됐다.",
                   sw/2.f, sh*0.86f, {180, 200, 230, (Uint8)(180 * a)}, true);
    }
    if (m_font && timer > 2.5f)
        renderText(r, m_font, "SPACE / ENTER: 종료",
                   sw/2.f, sh*0.92f, {140, 150, 180, (Uint8)(160 * a)}, true);
}

void UI::renderText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                    float x, float y, SDL_Color color, bool centered) {
    if (!font) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(tex, color.a);
        SDL_FRect dst = {centered ? x - surf->w/2.f : x, y,
                         (float)surf->w, (float)surf->h};
        SDL_RenderCopyF(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}
