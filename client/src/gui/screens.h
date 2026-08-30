/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef G_SCREEN_H
#define G_SCREEN_H

typedef enum GameScreen {
    SCREEN_GAME,
    SCREEN_PAUSE,
    SCREEN_LOADING,
    SCREEN_JOINING,
    SCREEN_LOGIN,
    SCREEN_OPTIONS
} GameScreen;

extern bool Screen_cursorEnabled;

void Screens_init(Texture2D terrain, bool *exit);
extern GameScreen Screen_Current;

void Screen_Switch(GameScreen screen);

void Screen_Make(void);

void Screen_MakeGame(void);
void Screen_MakePause(void);
void Screen_MakeOptions(void);
void Screen_MakeLoading(void);
void Screen_MakeJoining(void);
void Screen_MakeLogin(void);

#endif