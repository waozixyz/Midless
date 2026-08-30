/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include "kryon.h"
#include "player.h"
#include "inventory.h"
#include "world.h"
#include "resource.h"
#include "screens.h"
#include "block.h"
#include "networkhandler.h"
#include "chat.h"


void GameLoop(void);

//Hang watchdog: each frame refreshes a 3 s alarm. If a frame ever stops
//completing, the handler dumps the stuck call stack to stderr so the
// offending loop can be identified from the log without a debugger.
static void HangWatchdog(int sig) {
    (void)sig;
    void *frames[32];
    int n = backtrace(frames, 32);
    backtrace_symbols_fd(frames, n, 2);
    _exit(70);
}

static void HangWatchdogInit(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = HangWatchdog;
    sigaction(SIGALRM, &sa, NULL);
}


int main(void) {

    int screenWidth = 1280;
    int screenHeight = 720;

    // Initialization
    InitWindow(screenWidth, screenHeight, "Katalis");
    HangWatchdogInit();
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowState(FLAG_WINDOW_ALWAYS_RUN);
    SetExitKey(0);
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(60);
    InitUIDPI();

    // The kryon raylib backend runs OpenGL ES 2 on desktop, so the GLSL 100
    // shader pair is the only variant that compiles everywhere.
    char *chunkShaderVs = 
        #include "chunk/shaders/chunk_shader_gl100.vs"
    ;
    char *chunkShaderFs = 
        #include "chunk/shaders/chunk_shader_gl100.fs"
    ;

    Image midlessLogo = Resource_LoadImage("midless.png"); 


    SetWindowIcon(midlessLogo);

    EntityModel_DefineAll();
    Block_BuildDefinition();

    // World Initialization
    World_Init();

    
    Shader shader = LoadShaderFromMemory(chunkShaderVs, chunkShaderFs);
    Texture2D texture = Resource_LoadTexture("terrain.png");
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    
    World_ApplyTexture(texture);
    World_ApplyShader(shader);

    //Player Initialization
    Player_Init();
    
    bool exitProgram = false;
    Screens_init(texture, &exitProgram);

    // Boot straight into a local world behind the loading screen; the
    // player spawns and the scene shows only once the world has rendered.
    Screen_Switch(SCREEN_LOADING);


    #if defined(PLATFORM_WEB)
        emscripten_set_main_loop(GameLoop, 0, 1);
    #else
        while (!WindowShouldClose() && !exitProgram) {
            GameLoop();
        }
        
        Network_threadState = -1;

        UnloadShader(shader);
        UnloadTexture(texture);
        World_Unload();

        CloseWindow();
    #endif

    return 0;
}

void GameLoop(void) {
    alarm(3);
    static unsigned long dbgFrame = 0;
    static double dbgT0 = 0;
    double stageT = GetTime();
    if (dbgT0 == 0) dbgT0 = stageT;

    Network_ReadQueue();
    double tNet = GetTime();

    
    // Update
    Player_Update();
    double tPlayer = GetTime();
    World_Update();
    double tWorld = GetTime();
    
    Vector3 selectionBoxPos = (Vector3) { floor(player.rayResult.hitPos.x) + 0.5f, floor(player.rayResult.hitPos.y), floor(player.rayResult.hitPos.z) + 0.5f};
    
    // Draw
    BeginDrawing();

        float sunlightStrength = World_GetSunlightStrength();
        ClearBackground((Color) { 140 * sunlightStrength, 210 * sunlightStrength, 240 * sunlightStrength, 255});

        if (Screen_Current != SCREEN_LOADING) BeginMode3D(player.camera);
        if (Screen_Current != SCREEN_LOADING) {
            World_Draw(player.camera.position);
            if (player.rayResult.hitBlockID != -1) {
                Block block = Block_GetDefinition(player.rayResult.hitBlockID);
                Vector3 blockSize = Vector3Subtract(block.maxBB, block.minBB);
                blockSize = Vector3Scale(blockSize, 1.0f / 16);
                selectionBoxPos.y += blockSize.y / 2;
                DrawCube(selectionBoxPos, blockSize.x + 0.02f, blockSize.y + 0.02f, blockSize.z + 0.02f, (Color){255, 255, 255, 40});
            }
                
        }
        if (Screen_Current != SCREEN_LOADING) EndMode3D();
        
        double tUpdateAll = GetTime();
        int uiW = GetScreenWidth();
        int uiH = GetScreenHeight();
        UpdateUIDPI(uiW, uiH);
        BeginUIFrame(uiW, uiH, GetUIScale());
        Screen_Make();
        double tUi = GetTime();
        EndUIFrame();

    EndDrawing();

    dbgFrame++;
    if (GetTime() - dbgT0 > 2.0) {
        fprintf(stderr, "DBG frame=%lu net=%.4f player=%.4f world=%.4f preui=%.4f ui=%.4f total=%.4f hit=%d sel=%d camy=%.1f pgy=%.1f\n",
                dbgFrame, tNet - stageT, tPlayer - tNet, tWorld - tPlayer,
                tUpdateAll - tWorld, tUi - tUpdateAll, GetTime() - stageT,
                player.rayResult.hitBlockID, Inventory_SelectedBlock(),
                0.0f, player.position.y);
        dbgT0 = GetTime();
    }
}