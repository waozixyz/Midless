/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 *
 * Menu screens rebuilt on the kryon UI toolkit (theme, buttons, sliders,
 * toggles, text fields). The in-game HUD stays hand-drawn like the
 * original.
 */

#include <math.h>
#include <stdlib.h>
#include "kryon.h"
#include "screens.h"
#include "chat.h"
#include "player.h"
#include "world.h"
#include "block.h"
#include "networkhandler.h"
#include "packet.h"
#include "worldgenerator.h"
#include "inventory.h"

GameScreen Screen_Current = SCREEN_LOGIN;
bool Screen_cursorEnabled = false;
bool Screen_showDebug = false;
int Screen_debugInit = 0;
int screenHeight;
int screenWidth;
bool *exitGame;
Color uiColBg;
int maxFPSChoice = 0;
const char* maxFPS = "60";

Texture2D mapTerrain;

void Screens_init(Texture2D terrain, bool *exit) {
    mapTerrain = terrain;
    exitGame = exit;

    SetThemeSource(THEME_SOURCE_APP);
    SetCurrentTheme(THEME_MONO, 1);

    // KATALIS_DEBUG_INPUT turns on the debug overlay too, so scratch
    // X-server runs get FPS and chunk-draw numbers without clicking.
    if (Screen_debugInit == 0) {
        Screen_debugInit = 1;
        if (getenv("KATALIS_DEBUG_INPUT") != NULL)
            Screen_showDebug = true;
    }
}

void Screen_MakeGame(void) {

    //Draw debug infos
    if (Screen_showDebug) {
        const char* coordText = TextFormat("X: %i Y: %i Z: %i", (int)player.position.x, (int)player.position.y, (int)player.position.z);
        const char* debugText;

        if (Network_connectedToServer) {
            debugText = TextFormat("%2i FPS %2i PING", GetFPS(), Network_ping);
        } else {
            debugText = TextFormat("%2i FPS", GetFPS());
        }

        const char* drawText = TextFormat("CHUNK DRAWS: %i/%i", World_drawnChunks, World_loadedChunks);

        const char* versionText = "Katalis Pre-Alpha";
        DrawText(versionText, 9, 9, 20, BLACK);
        DrawText(versionText, 8, 8, 20, WHITE);

        DrawText(debugText, 9, 29, 20, BLACK);
        DrawText(coordText, 9, 49, 20, BLACK);
        DrawText(drawText, 9, 69, 20, BLACK);
        DrawText(debugText, 8, 28, 20, WHITE);
        DrawText(coordText, 8, 48, 20, WHITE);
        DrawText(drawText, 8, 68, 20, WHITE);
    }

    //Draw crosshair
    DrawRectangle(screenWidth / 2 - 8, screenHeight / 2 - 2, 16, 4, uiColBg);
    DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 + 2,  4, 6, uiColBg);
    DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 - 8,  4, 6, uiColBg);

    //Mining progress bar while holding the left button on a block.
    if (player.breaking && player.breakProgress > 0.0f && player.rayResult.hitBlockID > 0) {
        int barW = 48;
        int barX = screenWidth / 2 - barW / 2;
        int barY = screenHeight / 2 + 26;
        DrawRectangle(barX - 2, barY - 2, barW + 4, 10, (Color) { 0, 0, 0, 140 });
        DrawRectangle(barX, barY, (int)(barW * player.breakProgress), 6, WHITE);
    }

    //Hotbar + inventory/crafting overlay
    Inventory_Draw(mapTerrain, screenWidth, screenHeight);

    //Draw Chat
    Chat_Draw((Vector2){16, screenHeight - 52}, uiColBg);
}

void Screen_MakePause(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, uiColBg);

    int offsetY = screenHeight / 2 - 75;
    int offsetX = screenWidth / 2 - 100;

    int index = 0;

    //Continue Button
    if (StyledButton(offsetX , offsetY + (index++ * 35), 200, 30, "Continue", ButtonStylePrimary, 0, NULL)) {
        Screen_Switch(SCREEN_GAME);
        Screen_cursorEnabled = false;
        Game_SetCursorCaptured(true);
    }

    //Options Button
    if (StyledButton(offsetX, offsetY + (index++ * 35), 200, 30, "Options", ButtonStyleSecondary, 0, NULL)) {
        Screen_Switch(SCREEN_OPTIONS);
    }

    //Main Menu Button
    if (StyledButton(offsetX, offsetY + (index++ * 35), 200, 30, "Main Menu", ButtonStyleSecondary, 0, NULL)) {
        if (Network_connectedToServer) {
            Network_Disconnect();
        } else {
            Screen_Switch(SCREEN_LOGIN);
             Screen_cursorEnabled = false;
            World_Unload();
        }
    }

    //Quit Button
    if (StyledButton(offsetX, offsetY + (index++ * 35), 200, 30, "Quit", ButtonStyleDanger, 0, NULL)) {
        *exitGame = true;
    }
}

void Screen_MakeOptions(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, uiColBg);

    int offsetY = screenHeight / 2 - 75;
    int offsetX = screenWidth / 2 - 100;

    //Draw distance
    int newDrawDistance = world.drawDistance;
    if (Slider(1, offsetX, offsetY, 200, "Draw Distance", 2, 16, &newDrawDistance, "", NULL)) {
        if (newDrawDistance > world.drawDistance) {
            world.drawDistance = newDrawDistance;
            World_LoadChunks();
        } else {
            world.drawDistance = newDrawDistance;
            World_Reload();
        }

        if (Network_connectedToServer) {
            Network_Send(Packet_SetDrawDistance(world.drawDistance));
        }
    }

    offsetY += 56;

    //Show Debug toggle
    int showDebug = Screen_showDebug;
    Toggle(2, offsetX, offsetY, 200, 30, &showDebug, "Show Debug: OFF", "Show Debug: ON");
    Screen_showDebug = showDebug;

    offsetY += 35;

    //Max FPS button
    const char* maxFPSTxt = TextFormat("Max FPS: %s", maxFPS);
    if (StyledButton(offsetX, offsetY, 200, 30, maxFPSTxt, ButtonStyleSecondary, 0, NULL)) {
        maxFPSChoice++;
        if (maxFPSChoice == 3) maxFPSChoice = 0;
        if (maxFPSChoice == 0) {
            maxFPS = "60";
            SetTargetFPS(60);
        } else if (maxFPSChoice == 1) {
            maxFPS = "120";
            SetTargetFPS(120);
        } else if (maxFPSChoice == 2) {
            maxFPS = "Unlimited";
            SetTargetFPS(0);
        }
    }

    offsetY += 35;

    //Back Button
    if (StyledButton(offsetX, offsetY, 200, 30, "Back", ButtonStylePrimary, 0, NULL)) {
        Screen_Switch(SCREEN_PAUSE);
    }

}

void Screen_MakeJoining(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);
    DrawText("Joining Server...", screenWidth / 2 - 80, screenHeight / 2 - 30, 20, WHITE);
}

char name_input[16] = "Player";
int name_cursor = 0;
int name_focused = 0;

void Screen_MakeLogin(void) {
    if(IsCursorHidden()) EnableCursor();
    DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);

    const char *title = "KATALIS";
    int offsetY = screenHeight / 2;
    int offsetX = screenWidth / 2;

    DrawText(title, offsetX - (MeasureText(title, 80) / 2), offsetY - 100, 80, WHITE);

    //Name Input
    TextFieldProps nameField = {0};
    nameField.bounds = (Rectangle) { offsetX - 80, offsetY - 15, 160, 30 };
    nameField.text = name_input;
    nameField.text_size = sizeof(name_input);
    nameField.cursor_position = &name_cursor;
    nameField.focused = &name_focused;
    nameField.max_codepoints = 15;
    nameField.font = GetUIFontSize();
    TextField(nameField);

    Network_name = name_input;

    //Play button (local world)
    if (StyledButton(offsetX - 80, offsetY + 55, 160, 30, "Play", ButtonStylePrimary, 0, NULL)) {
        Screen_Switch(SCREEN_LOADING);
        Game_SetCursorCaptured(true);
    }

}

bool loadingNextFrame = false;
void Screen_MakeLoading(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);
    DrawText("Loading World", screenWidth / 2 - 80, screenHeight / 2, 20, WHITE);
    if(loadingNextFrame) {
        World_LoadSingleplayer();
        loadingNextFrame = false;
    }
    loadingNextFrame = true;
}

void Screen_Make(void) {
    screenHeight = GetScreenHeight();
    screenWidth = GetScreenWidth();

    uiColBg = (Color){ 0, 0, 0, 80 };

    if (Screen_Current == SCREEN_GAME)
        Screen_MakeGame();
    else if (Screen_Current == SCREEN_PAUSE)
        Screen_MakePause();
    else if(Screen_Current == SCREEN_LOADING)
        Screen_MakeLoading();
    else if (Screen_Current == SCREEN_JOINING)
        Screen_MakeJoining();
    else if (Screen_Current == SCREEN_LOGIN)
        Screen_MakeLogin();
    else if (Screen_Current == SCREEN_OPTIONS)
        Screen_MakeOptions();
}

void Screen_Switch(GameScreen screen) {
    Screen_Current = screen;
}
