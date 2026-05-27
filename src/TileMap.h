#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include "Physics.h"

constexpr int TILE_SIZE = 32;

struct Tile {
    int id = 0;
    bool solid = false;
};

class TileMap {
public:
    bool load(const std::string& csvPath, SDL_Renderer* renderer,
              const std::string& tilesetPath);
    void render(SDL_Renderer* renderer, float camX, float camY);
    bool isSolid(int col, int row) const;
    std::vector<AABB> getSolidAABBs() const;
    int getWidth()  const { return m_cols; }
    int getHeight() const { return m_rows; }
    int getPixelWidth()  const { return m_cols * TILE_SIZE; }
    int getPixelHeight() const { return m_rows * TILE_SIZE; }

private:
    std::vector<std::vector<Tile>> m_tiles;
    SDL_Texture* m_tileset = nullptr;
    int m_cols = 0, m_rows = 0;
    int m_tilesetCols = 1;

    void drawTile(SDL_Renderer* r, int id, float dx, float dy);
    SDL_Color tileColor(int id); // fallback colored rect
};
