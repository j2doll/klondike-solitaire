#include "Solitaire.h"

#include <iostream>
#include <raylib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Global game instance
Solitaire* game = nullptr;
// Global render texture for the game
RenderTexture2D gameTarget;
float gameScale = 1.0f;


void UpdateDrawFrame(void) {
    if (!game) return;

    gameScale = MIN( 
        float(GetScreenWidth()) / float(baseWindowWidth), 
        float(GetScreenHeight()) / float(baseWindowHeight) );
    game->update();
    
    // Begin rendering to the game target
    BeginTextureMode(gameTarget);
    ClearBackground(BLACK);
    game->draw();
    EndTextureMode();

    // Draw the game target to the screen
    BeginDrawing();
    ClearBackground(BLACK);

    DrawTexturePro(
		gameTarget.texture, // texture to draw
		Rectangle{ // source rectangle
            0.0f, 
            0.0f, 
            (float)gameTarget.texture.width, 
            (float)-gameTarget.texture.height },
        Rectangle{ // destination rectangle
            (GetScreenWidth() - ((float)baseWindowWidth * gameScale)) * 0.5f, 
            (GetScreenHeight() - ((float)baseWindowHeight * gameScale)) * 0.5f, 
            (float)baseWindowWidth * gameScale, 
            (float)baseWindowHeight * gameScale },
			Vector2{ 0, 0 }, // origin
			0.0f, // rotation
			WHITE); // color tint

    EndDrawing();
}

int main(void) {

    // Initialize window with base dimensions first
    InitWindow(baseWindowWidth, baseWindowHeight, "Solitaire");
#ifndef EMSCRIPTEN_BUILD
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    ToggleBorderlessWindowed();
#endif
    if (!IsWindowReady()) {
        return -1;
    }

    // Force a frame to ensure OpenGL context is properly initialized
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();

    // Set target FPS
    SetTargetFPS(60);

    SetExitKey(KEY_NULL);

    // Create game instance
    try {
        game = new Solitaire();
    } catch (const std::exception& e) {
        std::cerr << "Failed to create game: " << e.what() << std::endl;
        CloseWindow();
        return -1;
    }

    // Create render texture for the game
    gameTarget = LoadRenderTexture(baseWindowWidth, baseWindowHeight);
    SetTextureFilter(gameTarget.texture, TEXTURE_FILTER_BILINEAR);

#ifdef EMSCRIPTEN_BUILD
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose() && !game->shouldExit()) {
        UpdateDrawFrame();
    }
#endif

    // Cleanup
    if (game) {
        delete game;
        game = nullptr;
    }
    
    UnloadRenderTexture(gameTarget);
    CloseWindow();
    return 0;
}