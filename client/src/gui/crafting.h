/**
 * Terraria-style crafting for Katalis: a flat recipe list; anything the
 * inventory can afford is craftable with one click.
 */

#ifndef G_CRAFTING_H
#define G_CRAFTING_H

typedef struct RecipeIngredient {
    int block;
    int count;
} RecipeIngredient;

typedef struct Recipe {
    int resultBlock;
    int resultCount;
    RecipeIngredient ingredients[3];
    int ingredientCount;
} Recipe;

int Crafting_Count(void);
Recipe Crafting_Get(int index);
bool Crafting_CanCraft(int index);
bool Crafting_Craft(int index);

#endif
