/**
 * Crafting recipes for Katalis. Consumes from and produces into the
 * inventory; no crafting stations for now - the whole list is always
 * available, Terraria-guide style.
 */

#include "kryon.h"
#include "inventory.h"
#include "crafting.h"
#include "block.h"

static Recipe recipes[] = {
    { 4, 4, { { 10, 1 } }, 1 },                      // wood x4 <- log x1
    { 18, 2, { { 4, 1 } }, 1 },                      // wood_slab x2 <- wood x1
    { 17, 2, { { 1, 1 } }, 1 },                      // stone_slab x2 <- stone x1
    { 14, 1, { { 6, 2 } }, 1 },                      // glass x1 <- sand x2
    { 15, 1, { { 8, 1 }, { 4, 1 } }, 2 },            // fire x1 <- coal_ore x1 + wood x1
};

int Crafting_Count(void) {
    return (int)(sizeof(recipes) / sizeof(recipes[0]));
}

Recipe Crafting_Get(int index) {
    if (index >= 0 && index < Crafting_Count())
        return recipes[index];
    Recipe empty = { 0 };
    return empty;
}

bool Crafting_CanCraft(int index) {
    Recipe recipe = Crafting_Get(index);
    if (recipe.resultBlock == 0) return false;
    for (int i = 0; i < recipe.ingredientCount; i++) {
        if (Inventory_Count(recipe.ingredients[i].block) < recipe.ingredients[i].count)
            return false;
    }
    return true;
}

bool Crafting_Craft(int index) {
    if (!Crafting_CanCraft(index)) return false;
    Recipe recipe = Crafting_Get(index);

    for (int i = 0; i < recipe.ingredientCount; i++) {
        if (!Inventory_Consume(recipe.ingredients[i].block, recipe.ingredients[i].count))
            return false;
    }

    if (!Inventory_Add(recipe.resultBlock, recipe.resultCount)) {
        // Undo on a full inventory (kept simple: refuse instead).
        for (int i = 0; i < recipe.ingredientCount; i++) {
            Inventory_Add(recipe.ingredients[i].block, recipe.ingredients[i].count);
        }
        return false;
    }
    return true;
}
