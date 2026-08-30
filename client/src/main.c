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

//Kryon screenshot front-end helper (implemented in the kryon back end).
extern int kry_write_png_file(const char *path, const unsigned char *rgba, int w, int h);
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

static void RenderFrame(void) {
    Vector3 selectionBoxPos = (Vector3) { floor(player.rayResult.hitPos.x) + 0.5f, floor(player.rayResult.hitPos.y), floor(player.rayResult.hitPos.z) + 0.5f};

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

    int uiW = GetScreenWidth();
    int uiH = GetScreenHeight();
    UpdateUIDPI(uiW, uiH);
    BeginUIFrame(uiW, uiH, GetUIScale());
    Screen_Make();
    EndUIFrame();
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
    
    // Draw
    double tUpdateAll = GetTime();
    BeginDrawing();
    RenderFrame();
    double tUi = GetTime();
    EndDrawing();

    dbgFrame++;

    //Headless bring-up aid: KATALIS_SHOT=f1,f2,... re-renders those frames
    //into an FBO and dumps it to PNG (works on surfaceless GPU contexts
    //where the backbuffer is unreadable).
    {
        static int shotFrames[16];
        static int shotCount = -1;
        static int shotIdx = 0;
        static RenderTexture2D shotRT = { 0 };
        if (shotCount < 0) {
            shotCount = 0;
            const char *spec = getenv("KATALIS_SHOT");
            if (spec != NULL) {
                char buf[128];
                snprintf(buf, sizeof(buf), "%s", spec);
                char *tok = strtok(buf, ",");
                while (tok != NULL && shotCount < 16) {
                    shotFrames[shotCount++] = TextToInteger(tok);
                    tok = strtok(NULL, ",");
                }
            }
        }
        if (shotIdx < shotCount && dbgFrame >= (unsigned long)shotFrames[shotIdx]) {
            if (shotRT.id == 0) shotRT = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
            BeginTextureMode(shotRT);
            RenderFrame();
            EndTextureMode();
            Image img = LoadImageFromTexture(shotRT.texture);
            if (img.data != NULL) {
                char path[128];
                snprintf(path, sizeof(path), "shot_%05lu.png", dbgFrame);
                int rc = kry_write_png_file(path, (const unsigned char *)img.data, img.width, img.height);
                fprintf(stderr, "DBG wrote %s (%dx%d) rc=%d\n", path, img.width, img.height, rc);
                UnloadImage(img);
            } else {
                fprintf(stderr, "DBG fbo shot readback failed\n");
            }
            shotIdx++;
        }
    }

    if (GetTime() - dbgT0 > 2.0) {
        fprintf(stderr, "DBG frame=%lu net=%.4f player=%.4f world=%.4f preui=%.4f ui=%.4f total=%.4f hit=%d sel=%d camy=%.1f pgy=%.1f\n",
                dbgFrame, tNet - stageT, tPlayer - tNet, tWorld - tPlayer,
                tUpdateAll - tWorld, tUi - tUpdateAll, GetTime() - stageT,
                player.rayResult.hitBlockID, Inventory_SelectedBlock(),
                0.0f, player.position.y);
        fprintf(stderr, "DBG screen=%d queue=%d\n", (int)Screen_Current, World_QueueRemaining());
        dbgT0 = GetTime();
    }
}