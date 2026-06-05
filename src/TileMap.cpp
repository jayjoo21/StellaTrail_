#include "TileMap.h"
#include <fstream>
#include <sstream>
#include <SDL2/SDL_image.h>

// Tile IDs: 0=grass, 1=dirt, 2=stone(solid), 3=water(solid), 4=sand, 5=dark stone(floor)
static bool solidIds[] = {false, false, true, true, false, false};

bool TileMap::load(const std::string& csvPath, SDL_Renderer* renderer,
                   const std::string& tilesetPath) {
    // Try loading tileset texture
    if (!tilesetPath.empty()) {
        SDL_Surface* surf = IMG_Load(tilesetPath.c_str());
        if (surf) {
            m_tileset = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            int tw, th;
            SDL_QueryTexture(m_tileset, nullptr, nullptr, &tw, &th);
            m_tilesetCols = tw / TILE_SIZE;
        }
    }

    std::ifstream file(csvPath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<Tile> row;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ',')) {
            int id = std::stoi(cell);
            bool solid = (id >= 0 && id < (int)(sizeof(solidIds)/sizeof(bool)))
                         ? solidIds[id] : false;
            row.push_back({id, solid});
        }
        m_tiles.push_back(row);
    }

    m_rows = (int)m_tiles.size();
    m_cols = m_rows > 0 ? (int)m_tiles[0].size() : 0;
    return true;
}

void TileMap::render(SDL_Renderer* renderer, float camX, float camY) {
    for (int r = 0; r < m_rows; r++) {
        for (int c = 0; c < m_cols; c++) {
            float dx = c * TILE_SIZE - camX;
            float dy = r * TILE_SIZE - camY;
            // Cull off-screen tiles
            if (dx < -TILE_SIZE || dy < -TILE_SIZE || dx > 1280 || dy > 720) continue;
            int id = m_tiles[r][c].id;
            drawTile(renderer, id, dx, dy);
        }
    }
}

void TileMap::drawTile(SDL_Renderer* r, int id, float dx, float dy) {
    SDL_FRect dst = {dx, dy, (float)TILE_SIZE, (float)TILE_SIZE};
    if (m_tileset) {
        int srcCol = id % m_tilesetCols;
        int srcRow = id / m_tilesetCols;
        SDL_Rect src = {srcCol * TILE_SIZE, srcRow * TILE_SIZE, TILE_SIZE, TILE_SIZE};
        SDL_RenderCopyF(r, m_tileset, &src, &dst);
    } else {
        SDL_Color c = (id >= 0 && id < 8 && m_hasOverride[id])
                      ? m_tileOverride[id] : tileColor(id);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        SDL_RenderFillRectF(r, &dst);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 30);
        SDL_RenderDrawRectF(r, &dst);
    }
}

int TileMap::getTileId(int col, int row) const {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return -1;
    return m_tiles[row][col].id;
}

void TileMap::setColorOverride(int tileId, SDL_Color col) {
    if (tileId >= 0 && tileId < 8) { m_hasOverride[tileId] = true; m_tileOverride[tileId] = col; }
}

void TileMap::clearColorOverrides() {
    for (int i = 0; i < 8; i++) m_hasOverride[i] = false;
}

SDL_Color TileMap::tileColor(int id) {
    switch (id) {
        case 0: return {100, 160, 80, 255};   // grass
        case 1: return {160, 120, 70, 255};   // dirt
        case 2: return {100, 100, 110, 255};  // stone
        case 3: return {60,  100, 180, 255};  // water
        case 4: return {200, 180, 120, 255};  // sand
        case 5: return {60,  55,  65, 255};   // dark stone
        default: return {80, 80, 80, 255};
    }
}

bool TileMap::isSolid(int col, int row) const {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return true;
    return m_tiles[row][col].solid;
}

std::vector<AABB> TileMap::getSolidAABBs() const {
    std::vector<AABB> result;
    for (int r = 0; r < m_rows; r++)
        for (int c = 0; c < m_cols; c++)
            if (m_tiles[r][c].solid)
                result.push_back({(float)(c*TILE_SIZE), (float)(r*TILE_SIZE),
                                  (float)TILE_SIZE, (float)TILE_SIZE});
    return result;
}
