/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if !defined(PLATFORM_WEB)
    #define __clang__ true
#endif
#define STB_DS_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include "kryon.h"
#include "inventory.h"
#include "stb_ds.h"
#include "kryon.h"
#include "world.h"
#include "worldgenerator.h"
#include "player.h"
#include "chunkmeshgeneration.h"
#include "screens.h"
#include "networkhandler.h"
#include "packet.h"
#include "entity.h"
#include "entitymodel.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

World world;

void World_Init(void) {
    world.mat = LoadMaterialDefault();
    world.loadChunks = false;
    world.drawDistance = 8;
    world.time = 0;

    world.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) world.entities[i].type = 0; //type 0 = none

    int seed = rand();

    //Create world directory
    struct stat st = {0};
    if (stat("./world", &st) == -1) {
        #if defined(PLATFORM_WEB) || defined(OS_LINUX)
            mkdir("./world", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        #else
            mkdir("./world");
        #endif
    }
    
    if (FileExists("./world/seed.dat")) {
        unsigned int bytesRead = 0;
        unsigned char *data = LoadFileData("./world/seed.dat", &bytesRead);
        seed = (int)(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]); 
        UnloadFileData(data);
    } else {
        char data[4] = {(char)(seed >> 24), (char)(seed >> 16), (char)(seed >> 8), (char)(seed)};
        SaveFileData("./world/seed.dat", data, 4);
    }

    Chunk_MeshGenerationInit();
    WorldGenerator_Init(seed);
}

void World_LoadMultiplayer(void) {
    player.position = (Vector3) { 0, 80, 0 };
    Screen_Switch(SCREEN_GAME);
    world.loadChunks = true;
}

void World_LoadSingleplayer(void) {

    //Prevent multiplayer chunks from being unloaded during a singleplayer game which causes them to be saved locally.
    if (hmlen(world.chunks) != 0) return;

    Inventory_Reset();
    Inventory_Load("./world/player.dat");

    player.position = (Vector3) { 0, 80, 0 };
    Network_connectedToServer = false;
    world.loadChunks = true;
    World_LoadChunks();

    //Chunks stream in through World_Update while the loading screen is
    //shown; the player spawns on the surface once the scene is ready.
}

int World_QueueRemaining(void) {
    return arrlen(world.generateChunksQueue);
}

bool World_SpawnReady(void) {
    return world.loadChunks && arrlen(world.generateChunksQueue) == 0;
}

//Stand the player on the first solid surface of the spawn column instead
//of a fixed height that can be mid-air or buried inside terrain.
void World_FindSpawnPosition(void) {
    for (int y = CHUNK_SIZE_Y * 8; y > 0; y--) {
        Block block = Block_GetDefinition(World_GetBlock((Vector3) { 0, y, 0 }));
        if (block.colliderType == BlockColliderType_Solid && Block_IsFullSize(&block)) {
            player.position = (Vector3) { 0, y + 1, 0 };
            player.velocity = (Vector3) { 0 };
            return;
        }
    }
    player.position = (Vector3) { 0, 80, 0 };
    player.velocity = (Vector3) { 0 };
}

clock_t updateClock;
void World_Update(void) { 
    
    clock_t newClock = clock();
    float time_spent = (float)(newClock - updateClock) / CLOCKS_PER_SEC;
    updateClock = newClock;

    world.time += time_spent;
    if (world.time >= WORLD_DAY_LENGTH_SECONDS) world.time = 0;

    // Generate/mesh up to 4 chunks per frame, but stop early once ~6 ms
    // of the frame budget is spent so chunk work cannot cause long frames.
    // While the loading screen is up there is no scene to keep smooth, so
    // spend a much larger slice to bring the world up quickly.
    double meshDeadline = GetTime() + (Screen_Current == SCREEN_LOADING ? 0.030 : 0.006);
    if (Screen_Current == SCREEN_LOADING) {
        while (arrlen(world.generateChunksQueue) != 0 && GetTime() < meshDeadline) {
            World_ReadChunksQueues();
        }
    } else {
        for (int i = 0; i < 4; i++) {
            if (i > 0 && GetTime() >= meshDeadline) break;
            World_ReadChunksQueues();
        }
    }
    
}

void World_ReadChunksQueues(void) {
        static unsigned long rcq_n = 0;
        if (Game_DebugLog() && (++rcq_n & 0xFFFF) == 0) fprintf(stderr, "WD rcq=%lu\n", rcq_n);

        if (world.loadChunks == true) {

            int index = World_GetClosestChunkIndex(world.generateChunksQueue, Player_GetChunkPosition());

            if (index != -1) {
                Chunk *chunk = world.generateChunksQueue[index];

                if(!chunk->isBuilt) {
                    for (int i = 0; i < 6; i++) {
                        if (chunk->neighbours[i] == NULL) continue;
                        World_QueueChunk(chunk->neighbours[i], false);
                    }
                }

                Chunk_Generate(chunk);
                Chunk_BuildMesh(chunk);

                arrdel(world.generateChunksQueue, index);

                chunk->isGenerating = false;
            }
            
        }  
}

void World_QueueChunk(Chunk *chunk, bool immediate) {

    if (chunk->isGenerating == false) {
        if(!immediate) {
            arrput(world.generateChunksQueue, chunk);
        } else {
            arrins(world.generateChunksQueue, 0, chunk);
        }
    }
    chunk->isGenerating = true;
    
}


Chunk* World_GetChunkAt(Vector3 position) {
    long int p = Chunk_GetPackedPos(position);
    int index = hmgeti(world.chunks, p);
    if (index >= 0) {
        return world.chunks[index].value;
    }
    
    return NULL;
}

int World_GetClosestChunkIndex(Chunk* *array, Vector3 pos) {
    int arrLength = arrlen(array);
    if (arrLength > 0) {
        Chunk* queuedChunk = array[0];
        int index = 0;
        for (int i = 0; i < arrLength; i++) {
            if (Vector3Distance(array[i]->position, pos) < Vector3Distance(queuedChunk->position, pos)) {
                queuedChunk = array[i];
                index = i;
            }
        }
        return index;
    }

    return -1;
}

void World_AddChunk(Vector3 position) {

    long int p = Chunk_GetPackedPos(position);
    int index = hmgeti(world.chunks, p);
    if (index == -1) {
        //Add chunk to list
        Chunk *newChunk = MemAlloc(sizeof(Chunk));
        hmput(world.chunks, p, newChunk);

        Chunk_Init(newChunk, position);

        World_QueueChunk(newChunk, false);
        
    }
}

void World_RemoveChunk(Chunk *curChunk) {

    if(curChunk->isGenerating == true) {
        for(int i = 0; i < arrlen(world.generateChunksQueue); i++) {
            if(world.generateChunksQueue[i] == curChunk) {
                arrdel(world.generateChunksQueue, i);
            }
        }
    }

    long int p = Chunk_GetPackedPos(curChunk->position);
    
    Chunk_Unload(curChunk);
    hmdel(world.chunks, p);
    return;
}

void World_LoadChunks(void) {

    if (!world.loadChunks || Network_connectedToServer) return;

    Vector3 pos = Player_GetChunkPosition();

    //Create chunks or prepare array of chunks to be sorted
    int loadingHeight = fmin(world.drawDistance, 4);
    for (int y = loadingHeight; y >= -loadingHeight; y--) {
        for (int x = -world.drawDistance ; x <= world.drawDistance; x++) {
            for (int z = -world.drawDistance ; z <= world.drawDistance; z++) {
                Vector3 chunkPos = (Vector3) {pos.x + x, pos.y + y, pos.z + z};

                if (Vector3Distance(chunkPos, pos) < world.drawDistance) {
                    World_AddChunk(chunkPos);
                }
            }
        }
    }
    
    //destroy far chunks
    for (int i = hmlen(world.chunks) - 1; i >= 0 ; i--) {
        Chunk *chunk = world.chunks[i].value;

        if (Vector3Distance(chunk->position, pos) >= world.drawDistance) {
            World_RemoveChunk(chunk);
        }
    }
    
}

void World_Reload(void) {
    if (!Network_connectedToServer) World_Unload();
    world.loadChunks = true;
}

void World_Unload(void) {
    world.loadChunks = false;

    //Keep the inventory with the world.
    Inventory_Save("./world/player.dat");

    arrfree(world.generateChunksQueue);
    world.generateChunksQueue = NULL;

    for (int i = hmlen(world.chunks) - 1; i >= 0; i--) {
        World_RemoveChunk(world.chunks[i].value);
    }

    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        World_RemoveEntity(i);
    }

    world.chunks = NULL;

}

void World_ApplyTexture(Texture2D texture) {
    SetMaterialTexture(&world.mat, MATERIAL_MAP_DIFFUSE, texture);
}

void World_ApplyShader(Shader shader) {
    world.mat.shader = shader;
}

int World_drawnChunks = 0;
int World_loadedChunks = 0;

void World_Draw(Vector3 camPosition) {

    ChunkMesh_PrepareDrawing(world.mat);

    int amountChunks = hmlen(world.chunks);
    float frustumAngle = DEG2RAD * player.camera.fovy + 0.3f;
    Vector3 dirVec = Player_GetForwardVector();
    
    Vector3 chunkLocalCenter = (Vector3){CHUNK_SIZE_X / 2, CHUNK_SIZE_Y / 2, CHUNK_SIZE_Z / 2};

    //Create the sorted chunk list
    struct { Chunk *chunk; float dist; } sortedChunks[amountChunks];

    World_drawnChunks = 0;
    World_loadedChunks = amountChunks;
    int sortedLength = 0;
    for (int i=0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;

        if (chunk->onlyAir) continue;

        Vector3 centerChunk = Vector3Add(chunk->blockPosition, chunkLocalCenter);
        float distFromCam = Vector3Distance(centerChunk, camPosition);

        //Don't draw chunks behind the player
        Vector3 toChunkVec = Vector3Normalize(Vector3Subtract(centerChunk, camPosition));
        int behindPlayer = distFromCam > CHUNK_SIZE_X &&
                           Vector3Distance(toChunkVec, dirVec) > frustumAngle;

        if (behindPlayer) continue;

        sortedChunks[sortedLength].dist = distFromCam;
        sortedChunks[sortedLength].chunk = chunk;
        sortedLength++;
    }
    
    //Sort chunks near to far (insertion sort; lists are a few hundred)
    for (int i = 1; i < sortedLength; i++) {
        int j = i;
        while (j > 0 && sortedChunks[j-1].dist <= sortedChunks[j].dist) {
            struct { Chunk *chunk; float dist; } tempC;
            tempC.chunk = sortedChunks[j].chunk;
            tempC.dist = sortedChunks[j].dist;

            sortedChunks[j] = sortedChunks[j - 1];
            sortedChunks[j - 1].chunk = tempC.chunk;
            sortedChunks[j - 1].dist = tempC.dist;
            j = j - 1;
        }
    }

    ChunkMesh_BeginFrame();

    //Opaque pass, front to back: near geometry fills depth first so far
    //geometry gets early-z rejected instead of overdrawn.
    for (int i = 0; i < sortedLength; i++) {
        Chunk *chunk = sortedChunks[i].chunk;

        Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                                   0, 1, 0, chunk->blockPosition.y,
                                   0, 0, 1, chunk->blockPosition.z,
                                   0, 0, 0, 1 };

        ChunkMesh_Draw(&chunk->mesh, world.mat, matrix);
        World_drawnChunks++;
    }

    ChunkMesh_PrepareDrawing(world.mat);

    //Transparent pass, far to near: blending order for water and leaves.
    for (int i = sortedLength - 1; i >= 0; i--) {
        Chunk *chunk = sortedChunks[i].chunk;
        if (!chunk->hasTransparency) continue;

        Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                                   0, 1, 0, chunk->blockPosition.y,
                                   0, 0, 1, chunk->blockPosition.z,
                                   0, 0, 0, 1 };

        rlDisableBackfaceCulling();
        ChunkMesh_Draw(&chunk->meshTransparent, world.mat, matrix);
        rlEnableBackfaceCulling();
        World_drawnChunks++;
    }

    ChunkMesh_FinishDrawing();

    //Draw entities
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        if (world.entities[i].type == 0) continue;
        Entity_Draw(&world.entities[i]);
    }

}

int World_GetBlock(Vector3 blockPos) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if (chunk == NULL) return 0;
    
    //Get Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunk->blockPosition.x,
                                floor(blockPos.y) - chunk->blockPosition.y, 
                                floor(blockPos.z) - chunk->blockPosition.z 
                               };

    return Chunk_GetBlock(chunk, blockPosInChunk);
}

void World_FastBlock(Vector3 blockPos, int blockID) {
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if (chunk == NULL) return;

    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunkPos.x * CHUNK_SIZE_X, 
                                floor(blockPos.y) - chunkPos.y * CHUNK_SIZE_Y, 
                                floor(blockPos.z) - chunkPos.z * CHUNK_SIZE_Z 
                               };
    
    if (Chunk_IsValidPos(blockPosInChunk)) {
        int index = Chunk_PosToIndex(blockPosInChunk);

        chunk->data[index] = blockID;
    }
}

void World_SetBlock(Vector3 blockPos, int blockID, bool immediate) {
    if (Game_DebugLog()) fprintf(stderr, "WD SetBlock enter\n");

    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if (chunk == NULL) return;
    if (chunk->isLightGenerated == false) return;

    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunkPos.x * CHUNK_SIZE_X, 
                                floor(blockPos.y) - chunkPos.y * CHUNK_SIZE_Y, 
                                floor(blockPos.z) - chunkPos.z * CHUNK_SIZE_Z 
                               };
    
    Chunk_SetBlock(chunk, blockPosInChunk, blockID);

    if (blockID == 0) {
        World_QueueChunk(chunk, immediate);
        for (int i = 0; i < 26; i++) {
            if (chunk->neighbours[i] == NULL) continue;
            World_QueueChunk(chunk->neighbours[i], immediate);
        }
    } else {
        for (int i = 0; i < 26; i++) {
            if (chunk->neighbours[i] == NULL) continue;
            World_QueueChunk(chunk->neighbours[i], immediate);
        }
        World_QueueChunk(chunk, immediate); 
    }

}

float World_GetSunlightStrength(void) {
    return fmax(abs((int)(world.time - WORLD_DAY_LENGTH_SECONDS / 2.0f)) / (WORLD_DAY_LENGTH_SECONDS / 2.0f), 2/16.0f);
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Entities-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void World_TeleportEntity(int ID, Vector3 position, Vector3 rotation) {
    Entity *entity = &world.entities[ID];
    entity->position = position;
    entity->rotation = (Vector3) { 0, rotation.y, 0 };
    
    for (int i = 0; i < entity->model.amountParts; i++) {
        if (entity->model.parts[i].type == PartType_Head) {
            entity->model.parts[i].rotation.x = rotation.x;
        }
    }
}

void World_AddEntity(int ID, int type, Vector3 position, Vector3 rotation) {
    world.entities[ID].type = type;
    world.entities[ID].position = position;
    world.entities[ID].rotation = rotation;
    
    EntityModel_Build(&world.entities[ID].model, entityModels[0]);
}

void World_RemoveEntity(int ID) {
    Entity_Remove(&world.entities[ID]);
}
