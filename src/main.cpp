#include "Game.h"
#include <SDL2/SDL.h>
#include <windows.h>

int main(int /*argc*/, char* /*argv*/[]) {
    // Always run relative to the exe's directory so "assets/" paths work
    // regardless of the working directory the launcher used.
    char* basePath = SDL_GetBasePath();
    if (basePath) {
        SetCurrentDirectoryA(basePath);
        SDL_free(basePath);
    }

    Game game;
    if (!game.init(1280, 720, "Stella Trail")) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Init Error", "Game initialization failed.", nullptr);
        return 1;
    }
    game.run();
    game.shutdown();
    return 0;
}
