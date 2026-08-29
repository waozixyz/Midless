/**
 * Luanti-style crafting for Katalis: recipes are 3x3 patterns; the grid
 * matcher supports translated shaped patterns and shapeless sets, and the
 * flat list doubles as a crafting guide with quick-craft.
 */

#ifndef G_CRAFTING_H
#define G_CRAFTING_H

#define CRAFT_GRID_CELLS 9

typedef struct Recipe {
    int resultBlock;
    int resultCount;
    int pattern[CRAFT_GRID_CELLS]; // block ids, 0 = empty
    int shapeless;                 // 1 = position-independent multiset
} Recipe;

int Crafting_Count(void);
Recipe Crafting_Get(int index);

// Guide/quick-craft: can the inventory afford the recipe right now?
bool Crafting_CanCraft(int index);
bool Crafting_Craft(int index);

// Grid matching: which recipe does the 3x3 cell contents produce?
// Returns the recipe index or -1. Exact consumption is one per filled cell.
int Crafting_MatchGrid(const int cells[CRAFT_GRID_CELLS]);

#endif
