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
  definition: an entity file. Each state of a thing is a definition of its own (an oak, a burning oak, a burnt oak),
  reactions in the project's data say which one a hit turns it into. The engine fixes the core every definition has,
  the project declares blocks of fields on top; a definition ticks the blocks it uses.

  The project's data, data/txts/entity_blocks.txt:

    ; a comment
    category Tree
    category Creature
    clip Idle
    clip Walk
    block Reactive
    field Material choice Wood Plant Stone
    block Damageable
    field MaxHealth number 10
    field OnDeathBecomes entity
    field Boss bool no
    field Drops int 2
    field Line text "Ouch"

  Categories (the editor groups definitions by them), clips (an entity's named animations, the first is its default;
  none listed means one, Idle), and blocks, each followed by its fields: a name, a kind and the value a definition
  starts with. A choice lists its words (the first is the start), an entity field names another definition (none to
  start with).

  A definition is a text file in the project's data/entities, its name the file's:

    ; An entity (Kaban engine). Format: engine_entity_file.h
    guid 9f3a1c227b4e4d10
    display "Oak"
    category Tree
    clip Idle bitmap "oak.png"
    clip Walk spritesheet "oak sway.png" fps 6 once
    footprint circle 0 4 6
    hitarea box 0 40 28 70
    block Damageable
    field MaxHealth 20
    field OnDeathBecomes 2b7e01c4a9f05d33

  Its GUID (16 hex digits) is made once when it's created and never changes: the asset file, maps and saves name it
  by it. A clip is a bitmap (a still) or a sprite sheet (frames, at fps, looping unless once), one of the project's
  stored assets by its file name, standing on the asset's alignment (the foot). The footprint blocks movement, the hit
  area is what spells and interaction hit: none, circle x y radius, or box x y width height, the centre from the foot
  in pixels, y up. Then the blocks it uses with their fields (a field left out keeps the block's value); an entity
  field holds a definition's GUID, 0 for none.
*/

#define ENTITY_NAME_LENGTH 32
#define ENTITY_DISPLAY_LENGTH 64
#define ENTITY_TEXT_LENGTH 128
#define ENTITY_LOOK_NAME_LENGTH 256 // NOTE(pvlso): a stored asset's SourceFileName
#define ENTITY_MAX_CATEGORIES 64
#define ENTITY_MAX_CLIPS 16
#define ENTITY_MAX_BLOCKS 16
#define ENTITY_MAX_BLOCK_FIELDS 8
#define ENTITY_MAX_CHOICES 16

// NOTE(pvlso): entity_field_kind and entity_collision_shape are in engine_api.h, games use them too

struct entity_field_value
{
    s32 Int; // NOTE(pvlso): a choice's word, by its place in the field's list
    r32 Number;
    b32 Bool;
    u64 Entity; // NOTE(pvlso): a definition's GUID, 0 for none
    char Text[ENTITY_TEXT_LENGTH];
};

struct entity_field_def
{
    char Name[ENTITY_NAME_LENGTH];
    entity_field_kind Kind;
    entity_field_value Default;
    u32 ChoiceCount;
    char Choices[ENTITY_MAX_CHOICES][ENTITY_NAME_LENGTH];
};

struct entity_block_def
{
    char Name[ENTITY_NAME_LENGTH];
    u32 FieldCount;
    entity_field_def Fields[ENTITY_MAX_BLOCK_FIELDS];
};

// NOTE(pvlso): The project's data/txts/entity_blocks.txt
struct entity_project
{
    b32 Loaded;
    u32 CategoryCount;
    char Categories[ENTITY_MAX_CATEGORIES][ENTITY_NAME_LENGTH];
    u32 ClipCount;
    char Clips[ENTITY_MAX_CLIPS][ENTITY_NAME_LENGTH];
    u32 BlockCount;
    entity_block_def Blocks[ENTITY_MAX_BLOCKS];
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
};

struct entity_shape
{
    entity_collision_shape Shape;
    v2 Center; // NOTE(pvlso): from the foot, pixels, y up
    v2 Size;   // NOTE(pvlso): a box's width and height, a circle's radius in x
};

// NOTE(pvlso): A definition as its file has it, its clips, blocks and fields by their place in the project's data
struct entity_file
{
    b32 Loaded;
    u32 ErrorCount;

    u64 GUID;
    char DisplayName[ENTITY_DISPLAY_LENGTH];
    u32 Category; // NOTE(pvlso): 0 for none, else the project's category + 1

    entity_clip Clips[ENTITY_MAX_CLIPS];
    entity_shape Footprint;
    entity_shape HitArea;

    u32 Blocks; // NOTE(pvlso): bit n: it uses the project's block n
    entity_field_value Fields[ENTITY_MAX_BLOCKS][ENTITY_MAX_BLOCK_FIELDS];
};

#define ENGINE_ENTITY_FILE_H
#endif
