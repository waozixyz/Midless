/**
 * Inventory for Katalis: stacking slots, hotbar, cursor-stack mouse
 * interaction, and the Terraria-style crafting list beside the grid.
 */

#include <stdio.h>
#include <string.h>
#include "kryon.h"
#include "inventory.h"
#include "crafting.h"
#include "block.h"

InvItem Inventory_slots[INV_SLOTS];
InvItem Inventory_held = { 0 };
int Inventory_hotbar = 0;
bool Inventory_open = false;

void Inventory_Reset(void) {
    memset(Inventory_slots, 0, sizeof(Inventory_slots));
    Inventory_held = (InvItem) { 0 };
    Inventory_hotbar = 0;
}

bool Inventory_Add(int blockID, int count) {
    if (blockID <= 0 || count <= 0) return false;

    //Top up existing stacks first so the hotbar stays tidy.
    for (int i = 0; i < INV_SLOTS && count > 0; i++) {
        InvItem *slot = &Inventory_slots[i];
        if (slot->blockID == blockID && slot->count > 0 && slot->count < INV_STACK_MAX) {
            int room = INV_STACK_MAX - slot->count;
            int move = count < room ? count : room;
            slot->count += move;
            count -= move;
        }
    }
    for (int i = 0; i < INV_SLOTS && count > 0; i++) {
        InvItem *slot = &Inventory_slots[i];
        if (slot->count <= 0) {
            int move = count < INV_STACK_MAX ? count : INV_STACK_MAX;
            slot->blockID = blockID;
            slot->count = move;
            count -= move;
        }
    }
    return count == 0;
}

int Inventory_Count(int blockID) {
    int total = 0;
    for (int i = 0; i < INV_SLOTS; i++) {
        if (Inventory_slots[i].blockID == blockID)
            total += Inventory_slots[i].count;
    }
    return total;
}

bool Inventory_Consume(int blockID, int count) {
    if (Inventory_Count(blockID) < count) return false;

    for (int i = INV_SLOTS - 1; i >= 0 && count > 0; i--) {
        InvItem *slot = &Inventory_slots[i];
        if (slot->blockID != blockID) continue;
        int take = count < slot->count ? count : slot->count;
        slot->count -= take;
        count -= take;
        if (slot->count == 0) slot->blockID = 0;
    }
    return true;
}

int Inventory_SelectedBlock(void) {
    InvItem *slot = &Inventory_slots[Inventory_hotbar];
    return slot->count > 0 ? slot->blockID : 0;
}

bool Inventory_Save(const char *path) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) return false;
    fwrite(Inventory_slots, sizeof(InvItem), INV_SLOTS, file);
    fwrite(&Inventory_hotbar, sizeof(int), 1, file);
    fclose(file);
    return true;
}

bool Inventory_Load(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    size_t read = fread(Inventory_slots, sizeof(InvItem), INV_SLOTS, file);
    if (read != INV_SLOTS) {
        fclose(file);
        Inventory_Reset();
        return false;
    }
    fread(&Inventory_hotbar, sizeof(int), 1, file);
    if (Inventory_hotbar < 0 || Inventory_hotbar >= INV_HOTBAR_SLOTS)
        Inventory_hotbar = 0;
    fclose(file);
    return true;
}

static void DrawBlockIcon(Texture2D terrain, int blockID, int x, int y, int size) {
    Block def = Block_GetDefinition(blockID);
    int texI = def.textures[BlockFace_Front];
    int texX = texI % 16 * 16;
    int texY = texI / 16 * 16;

    float w = def.maxBB.x - def.minBB.x;
    float h = def.maxBB.y - def.minBB.y;
    if (w <= 0 || h <= 0) { w = 16; h = 16; }
    float scale = size / (w > h ? w : h);

    Rectangle src = { texX + 16 - def.maxBB.x, texY + 16 - def.maxBB.y, w, h };
    Rectangle dst = { x + (size - w * scale) / 2, y + (size - h * scale) / 2, w * scale, h * scale };
    DrawTexturePro(terrain, src, dst, (Vector2) { 0, 0 }, 0, WHITE);
}

static void DrawSlot(Texture2D terrain, InvItem *slot, int x, int y, int size, bool selected) {
    DrawRectangle(x, y, size, size, (Color) { 20, 20, 20, 140 });
    DrawRectangleLinesEx((Rectangle) { x, y, size, size }, selected ? 3 : 1,
                         selected ? WHITE : (Color) { 255, 255, 255, 90 });
    if (slot->count > 0) {
        DrawBlockIcon(terrain, slot->blockID, x + 4, y + 4, size - 8);
        const char *countText = TextFormat("%d", slot->count);
        DrawText(countText, x + size - 4 - MeasureText(countText, 14), y + size - 20, 14, BLACK);
        DrawText(countText, x + size - 5 - MeasureText(countText, 14), y + size - 21, 14, WHITE);
    }
}

static void HandleSlotClick(InvItem *slot) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (Inventory_held.count <= 0) {
            //Pick the stack up.
            Inventory_held = *slot;
            slot->count = 0;
            slot->blockID = 0;
        } else if (slot->count <= 0) {
            //Drop the whole stack.
            *slot = Inventory_held;
            Inventory_held = (InvItem) { 0 };
        } else if (slot->blockID == Inventory_held.blockID) {
            //Merge, keep the remainder on the cursor.
            int room = INV_STACK_MAX - slot->count;
            int move = Inventory_held.count < room ? Inventory_held.count : room;
            slot->count += move;
            Inventory_held.count -= move;
            if (Inventory_held.count == 0) Inventory_held.blockID = 0;
        } else {
            //Swap with the cursor.
            InvItem temp = *slot;
            *slot = Inventory_held;
            Inventory_held = temp;
        }
    }
}

static void DrawCraftingList(Texture2D terrain, int x, int y, int w) {
    int row = 0;
    int rowH = 44;

    DrawText("Crafting", x, y - 24, 18, WHITE);

    for (int i = 0; i < Crafting_Count(); i++) {
        Recipe recipe = Crafting_Get(i);
        bool can = Crafting_CanCraft(i);
        int rowY = y + row * rowH;

        DrawRectangle(x, rowY, w, rowH - 4, can ? (Color) { 30, 30, 30, 180 } : (Color) { 15, 15, 15, 100 });
        DrawRectangleLinesEx((Rectangle) { x, rowY, w, rowH - 4 }, 1,
                             can ? (Color) { 240, 240, 240, 200 } : (Color) { 120, 120, 120, 80 });

        Color tint = can ? WHITE : (Color) { 120, 120, 120, 160 };
        if (can) {
            DrawBlockIcon(terrain, recipe.resultBlock, x + 4, rowY + 4, rowH - 14);
        } else {
            DrawBlockIcon(terrain, recipe.resultBlock, x + 4, rowY + 4, rowH - 14);
            DrawRectangle(x + 4, rowY + 4, rowH - 14, rowH - 14, (Color) { 0, 0, 0, 120 });
        }

        Block resultDef = Block_GetDefinition(recipe.resultBlock);
        const char *title = TextFormat("%s x%d", resultDef.name, recipe.resultCount);
        DrawText(title, x + rowH - 4, rowY + 4, 14, tint);

        char needs[128] = "";
        for (int ing = 0; ing < recipe.ingredientCount; ing++) {
            Block ingDef = Block_GetDefinition(recipe.ingredients[ing].block);
            char part[48];
            snprintf(part, sizeof(part), "%s%s x%d",
                     ing > 0 ? ", " : "",
                     ingDef.name,
                     recipe.ingredients[ing].count);
            strncat(needs, part, sizeof(needs) - strlen(needs) - 1);
        }
        DrawText(needs, x + rowH - 4, rowY + 22, 12,
                 can ? (Color) { 200, 200, 200, 220 } : (Color) { 130, 130, 130, 140 });

        if (can && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, (Rectangle) { x, rowY, w, rowH - 4 })) {
                Crafting_Craft(i);
            }
        }
        row++;
    }
}

void Inventory_Draw(Texture2D terrain, int screenWidth, int screenHeight) {
    //Hotbar (always, Terraria-style top-left).
    int hotSize = 44;
    for (int i = 0; i < INV_HOTBAR_SLOTS; i++) {
        DrawSlot(terrain, &Inventory_slots[i], 8 + i * (hotSize + 4), 8, hotSize, i == Inventory_hotbar);
    }

    if (!Inventory_open) return;

    //Dim the world behind the panels.
    DrawRectangle(0, 0, screenWidth, screenHeight, (Color) { 0, 0, 0, 130 });

    int slotSize = 48;
    int gap = 6;
    int gridW = INV_COLS * (slotSize + gap) - gap;
    int gridX = screenWidth / 2 - gridW / 2;
    int gridY = screenHeight / 2 - 40;

    DrawText("Inventory", gridX, gridY - 26, 18, WHITE);
    for (int row = 0; row < INV_ROWS; row++) {
        for (int col = 0; col < INV_COLS; col++) {
            int index = row * INV_COLS + col;
            int x = gridX + col * (slotSize + gap);
            int y = gridY + row * (slotSize + gap);
            DrawSlot(terrain, &Inventory_slots[index], x, y, slotSize, false);

            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, (Rectangle) { x, y, slotSize, slotSize })) {
                HandleSlotClick(&Inventory_slots[index]);
            }
        }
    }

    DrawCraftingList(terrain, gridX - 300, gridY, 280);

    //Held stack follows the cursor.
    if (Inventory_held.count > 0) {
        Vector2 mouse = GetMousePosition();
        DrawBlockIcon(terrain, Inventory_held.blockID, (int)mouse.x + 4, (int)mouse.y + 4, 36);
        const char *countText = TextFormat("%d", Inventory_held.count);
        DrawText(countText, (int)mouse.x + 4, (int)mouse.y + 34, 14, WHITE);
    }
}
