#include "game_context.h"
#include <SDL3/SDL.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    ScrapHeap::GameContext ctx;

    if (!ScrapHeap::initializeContext(ctx)) {
        SDL_Log("Failed to initialize game");
        return 1;
    }

    Uint64 lastTime = SDL_GetTicks();
    const float maxDeltaTime = 0.1f;  // Cap to prevent physics explosion

    while (ctx.running) {
        // Calculate delta time
        Uint64 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        // Cap delta time
        if (dt > maxDeltaTime) {
            dt = maxDeltaTime;
        }

        // Run game loop
        ScrapHeap::gameLoop(ctx, dt);

        // Small delay to prevent CPU spinning
        SDL_Delay(1);
    }

    ScrapHeap::shutdownContext(ctx);

    return 0;
}
