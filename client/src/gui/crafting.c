/**
 * Crafting recipes and grid matching for Katalis.
 */

#include <string.h>
#include "kryon.h"
#include "inventory.h"
#include "crafting.h"
#include "block.h"

static Recipe recipes[] = {
    // wood x4 <- any single log (shapeless, like Luanti planks)
    { 4, 4, { 10, 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    // wood_slab x6 <- row of 3 wood
    { 18, 6, { 4, 4, 4, 0, 0, 0, 0, 0, 0 }, 0 },
    // stone_slab x6 <- row of 3 stone
    { 17, 6, { 1, 1, 1, 0, 0, 0, 0, 0, 0 }, 0 },
    // glass x1 <- 2 sand (shapeless smelt)
    { 14, 1, { 6, 6, 0, 0, 0, 0, 0, 0, 0 }, 1 },
    // fire x1 <- coal_ore + wood (torch)
    { 15, 1, { 8, 4, 0, 0, 0, 0, 0, 0, 0 }, 1 },
};

int Crafting_Count(void) {
    return (int)(sizeof(recipes) / sizeof(recipes[0]));
}

Recipe Crafting_Get(int index) {
    if (index >= 0 && index < Crafting_Count())
        return recipes[index];
    Recipe empty;
    memset(&empty, 0, sizeof(empty));
    return empty;
}

//Count one block id inside a pattern.
static int PatternCount(const Recipe *recipe, int block) {
    int total = 0;
    for (int i = 0; i < CRAFT_GRID_CELLS; i++) {
        if (recipe->pattern[i] == block) total++;
    }
    return total;
}

bool Crafting_CanCraft(int index) {
    Recipe recipe = Crafting_Get(index);
    if (recipe.resultBlock == 0) return false;
    for (int i = 0; i < CRAFT_GRID_CELLS; i++) {
        int need = recipe.pattern[i];
        if (need == 0) continue;
        if (Inventory_Count(need) < PatternCount(&recipe, need))
            return false;
    }
    return true;
}

bool Crafting_Craft(int index) {
    if (!Crafting_CanCraft(index)) return false;
    Recipe recipe = Crafting_Get(index);

    for (int i = 0; i < CRAFT_GRID_CELLS; i++) {
        int need = recipe.pattern[i];
        if (need == 0) continue;
        if (!Inventory_Consume(need, 1)) return false;
    }

    if (!Inventory_Add(recipe.resultBlock, recipe.resultCount)) {
        for (int i = 0; i < CRAFT_GRID_CELLS; i++) {
            if (recipe.pattern[i] != 0)
                Inventory_Add(recipe.pattern[i], 1);
        }
        return false;
    }
    return true;
}

//Bounding box of filled cells; returns 0 when the grid is empty.
static void GridBounds(const int cells[9], int *minR, int *minC, int *maxR, int *maxC) {
    *minR = 3; *minC = 3; *maxR = -1; *maxC = -1;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            if (cells[r * 3 + c] != 0) {
                if (r < *minR) *minR = r;
                if (c < *minC) *minC = c;
                if (r > *maxR) *maxR = r;
                if (c > *maxC) *maxC = c;
            }
        }
    }
}

int Crafting_MatchGrid(const int cells[CRAFT_GRID_CELLS]) {
    int minR, minC, maxR, maxC;
    GridBounds(cells, &minR, &minC, &maxR, &maxC);
    if (maxR < 0) return -1;

    for (int i = 0; i < Crafting_Count(); i++) {
        Recipe recipe = Crafting_Get(i);

        if (recipe.shapeless) {
            //Same multiset of ids, counts included.
            int used[9] = { 0 };
            int ok = 1;
            for (int cell = 0; cell < 9 && ok; cell++) {
                if (cells[cell] == 0) continue;
                int found = -1;
                for (int p = 0; p < 9; p++) {
                    if (used[p]) continue;
                    if (recipe.pattern[p] == cells[cell]) { found = p; break; }
                }
                if (found < 0) { ok = 0; break; }
                used[found] = 1;
            }
            //Recipe must not require ids absent from the grid.
            for (int p = 0; p < 9 && ok; p++) {
                if (recipe.pattern[p] == 0) continue;
                int have = 0;
                for (int cell = 0; cell < 9; cell++) {
                    if (cells[cell] == recipe.pattern[p]) have++;
                }
                if (have < PatternCount(&recipe, recipe.pattern[p])) ok = 0;
            }
            if (ok) return i;
        } else {
            //Shaped: translate the pattern to its bounding box origin
            //and compare cell by cell.
            int pMinR, pMinC, pMaxR, pMaxC;
            GridBounds(recipe.pattern, &pMinR, &pMinC, &pMaxR, &pMaxC);
            if (pMaxR - pMinR != maxR - minR || pMaxC - pMinC != maxC - minC)
                continue;

            int ok = 1;
            for (int r = 0; r <= maxR - minR && ok; r++) {
                for (int c = 0; c <= maxC - minC && ok; c++) {
                    int cellId = cells[(minR + r) * 3 + (minC + c)];
                    int patId = recipe.pattern[(pMinR + r) * 3 + (pMinC + c)];
                    if (cellId != patId) ok = 0;
                }
            }
            if (ok) return i;
        }
    }
    return -1;
}
