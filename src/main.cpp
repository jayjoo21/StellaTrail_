#include "Game.h"
#include <SDL2/SDL.h>

int main(int /*argc*/, char* /*argv*/[]) {
    Game game;
    if (!game.init(1280, 720, "Stella Trail - 별의 여정")) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Init Error", "Game initialization failed.", nullptr);
        return 1;
    }
    game.run();
    game.shutdown();
    return 0;
}
