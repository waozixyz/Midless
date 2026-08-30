/**
 * Inventory for Katalis: hotbar + storage grid with stacking, a cursor
 * stack for click-to-move, and save/load beside the world data.
 */

#ifndef G_INVENTORY_H
#define G_INVENTORY_H

#include "kryon.h"

#define INV_COLS 9
#define INV_ROWS 4
#define INV_SLOTS (INV_COLS * INV_ROWS)
#define INV_HOTBAR_SLOTS INV_COLS
#define INV_STACK_MAX 999

//Craftable tool items live above the block id space.
typedef enum ToolItem {
    ITEM_WOOD_PICKAXE = 256,
    ITEM_STONE_PICKAXE,
    ITEM_WOOD_AXE,
    ITEM_STONE_AXE,
    ITEM_WOOD_SHOVEL,
    ITEM_STONE_SHOVEL,
    ITEM_COUNT
} ToolItem;

bool Inventory_IsTool(int itemID);
const char *Inventory_ItemName(int itemID);

typedef struct InvItem {
    int blockID;
    int count;
} InvItem;

extern InvItem Inventory_slots[INV_SLOTS];
extern InvItem Inventory_held;
extern int Inventory_hotbar;
extern bool Inventory_open;

void Inventory_Reset(void);
bool Inventory_Add(int blockID, int count);
int Inventory_Count(int blockID);
bool Inventory_Consume(int blockID, int count);
int Inventory_SelectedBlock(void);

bool Inventory_Save(const char *path);
bool Inventory_Load(const char *path);

// Hotbar always; grid, crafting list and held stack when open.
void Inventory_Draw(Texture2D terrain, int screenWidth, int screenHeight);

#endif
