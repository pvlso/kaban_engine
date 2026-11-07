#if !defined(ENGINE_ENTITY_FILE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): Entities (map editor plan, phase 5). Everything in a game but its ground tiles is an entity, made from a
  definition: an entity file. A definition is only what any game needs to show and place a thing: its name (its
  file's), a GUID, a display name, its type, its clips, a footprint and a hit area. What a thing does is the game's
  code, by its type (and its name, when the game needs to tell two of a type apart).

  The project's data, data/txts/entity_types.txt:

    ; a comment
    type Tree
    type Crate
    clip Idle
    clip Burnt

  Types (the game's kinds of things; the editor groups definitions by them) and clips (an entity's named looks, the
  first is its default; none listed means one, Idle).

  A definition is a text file in the project's data/entities, its name the file's:

    ; An entity (Kaban engine). Format: engine_entity_file.h
    guid 9f3a1c227b4e4d10
    display "Oak"
    type Tree
    clip Idle bitmap "oak.png"
    clip Burnt spritesheet "oak burning.png" fps 6 once align 0.5 0.1
    footprint circle 0 4 6
    hitarea box 0 40 28 70

  Its GUID (16 hex digits) is made once when it's created and never changes: the asset file, maps and saves name it
  by it. A clip is a bitmap (a still) or a sprite sheet (frames, at fps, looping unless once), one of the project's
  stored assets by its file name, standing on its foot: "align x y", where the foot is on a frame from 0 to 1 from its
  bottom left (set in the entity editor), else the asset's alignment. The footprint blocks movement, the hit
  area is what spells and interaction hit: none, circle x y radius, or box x y width height, the centre from the foot
  in pixels, y up.
*/

#define ENTITY_NAME_LENGTH 32
#define ENTITY_DISPLAY_LENGTH 64
#define ENTITY_LOOK_NAME_LENGTH 256 // NOTE(pvlso): a stored asset's SourceFileName
#define ENTITY_MAX_TYPES 64
#define ENTITY_MAX_CLIPS 16

// NOTE(pvlso): entity_collision_shape is in engine_api.h, games use it too

// NOTE(pvlso): The project's data/txts/entity_types.txt
struct entity_project
{
    b32 Loaded;
    u32 TypeCount;
    char Types[ENTITY_MAX_TYPES][ENTITY_NAME_LENGTH];
    u32 ClipCount;
    char Clips[ENTITY_MAX_CLIPS][ENTITY_NAME_LENGTH];
};

enum entity_look_kind
{
    EntityLook_None,
    EntityLook_Bitmap,
    EntityLook_SpriteSheet,
};

struct entity_clip
{
    entity_look_kind Kind;
    char FileName[ENTITY_LOOK_NAME_LENGTH];
    r32 FramesPerSecond;
    b32 Loop;
    // NOTE(pvlso): Where the foot is on a frame, 0 to 1 from its bottom left; without it the asset's alignment
    b32 HasAlign;
    v2 Align;
};

struct entity_shape
{
    entity_collision_shape Shape;
    v2 Center; // NOTE(pvlso): from the foot, pixels, y up
    v2 Size;   // NOTE(pvlso): a box's width and height, a circle's radius in x
};

// NOTE(pvlso): A definition as its file has it, its type and clips by their place in the project's data
struct entity_file
{
    b32 Loaded;
    u32 ErrorCount;

    u64 GUID;
    char DisplayName[ENTITY_DISPLAY_LENGTH];
    u32 Type; // NOTE(pvlso): 0 for none, else the project's type + 1

    entity_clip Clips[ENTITY_MAX_CLIPS];
    entity_shape Footprint;
    entity_shape HitArea;
};

#define ENGINE_ENTITY_FILE_H
#endif
