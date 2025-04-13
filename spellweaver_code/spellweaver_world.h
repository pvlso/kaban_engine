#if !defined(SPELLWEAVER_WORLD_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

// TODO(casey): Think about what the real safe margin is!
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

#define TILES_PER_CHUNK_DIM 4

#define WORLD_TILE_COUNT_PER_DIM 256 

#define WORLD_TILE_NODE_COUNT_PER_DIM 1024 

#define TILE_NODE_PER_TILE 4 

#define WORLD_WIDTH_CHUNK_COUNT WORLD_WIDTH_TILE_COUNT/TILES_PER_CHUNK
#define WORLD_HEIGHT_CHUNK_COUNT WORLD_HEIGHT_TILE_COUNT/TILES_PER_CHUNK

struct world_position
{
    s32 TileX;
    s32 TileY;

    v2 Offset;
};

#if 0
struct world_position
{
    // TODO(casey): It seems like we have to store ChunkX/Y/Z with each
    // entity because even though the sim region gather doesn't need it
    // at first, and we could get by without it, entity references pull
    // in entities WITHOUT going through their world_chunk, and thus
    // still need to know the ChunkX/Y/Z
    
    s32 ChunkX;
    s32 ChunkY;

    // NOTE(casey): These are the offsets from the chunk center
    v2 Offset_;
};
#endif

// TODO(casey): Could make this just tile_chunk and then allow multiple tile chunks per X/Y/Z
struct world_entity_block
{
    uint32 EntityCount;
    world_entity_block *Next;

    u32 EntityDataSize;
    u8 EntityData[1 << 16];
};

struct world_chunk
{
    int32 ChunkX;
    int32 ChunkY;

    // TODO(casey): Profile this and determine if a pointer would be better here!
    world_entity_block *FirstBlock;
    
    world_chunk *NextInHash;
};

struct decoration
{
    world_position P;

    u32 AssetTypeID;
    b32 IsSpriteSheet;
    u32 DecorationIndex;

    r32 Height;
    u32 TagCount;
    ssa_tag Tags[64];

    union
    {
        bitmap_id BitmapID;
        spritesheet_id SpriteSheetID;
    };
};

struct collision
{
    world_position P;
    rectangle2 Rect;
};

struct world_tile
{
    u32 TileID;
    bitmap_id TileBitmapID;
};

struct as_tile_node
{
    world_position TileP;

    s32 X;
    s32 Y;
    
    b32 Obstacle;
    b32 Visited;

    r32 GlobalGoal;
    r32 LocalGoal;

    as_tile_node *Neighbours[8];
    as_tile_node *Parent;
};

struct world
{
    memory_arena Arena;

    v2 ChunkDimInMeters;
    real32 TileSideInMeters;
    real32 TileDepthInMeters;

    u32 TileCount;
    world_tile *Tiles;
    decoration *Decorations;
    collision *Collisions;

    u32 TileNodeCount;
    as_tile_node *TileNodes;
    heap MinTileNodeHeap;
    
    world_entity_block *FirstFree;

    // TODO(casey): WorldChunkHash should probably switch to pointers IF
    // tile entity blocks continue to be stored en masse directly in the tile chunk!
    // NOTE(casey): A the moment, this must be a power of two!
    world_chunk *ChunkHash[4096];

    world_chunk *FirstFreeChunk;
    world_entity_block *FirstFreeBlock;
};

#define SPELLWEAVER_WORLD_H
#endif
