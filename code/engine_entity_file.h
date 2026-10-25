#if !defined(ENGINE_ENTITY_FILE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): Entities (map editor plan, phase 5). Everything in a game but its ground tiles is an entity. The engine
  fixes what every entity has, the project lists its types and what each type has on top.

  The project's types, data/txts/entity_types.txt:

    ; a comment
    type Tree
    state Normal
    state Dry
    state Burnt
    field GrowStage int 0
    field Height number 1.5
    field Burns bool yes
    field Wood text "Oak"

  A type, then its states (the first is where an entity starts unless it says otherwise) and its own fields: a name,
  a kind (int, number, bool, text) and the value an entity of the type starts with. The lines after a type are its.

  An entity is a text file in the project's data/entities, its name the file's:

    ; An entity (Kaban engine). Format: engine_entity_file.h
    type Tree
    state Normal
    flip no
    collision circle 0 6 5
    look Normal bitmap oak.png
    look Burnt spritesheet "oak burning.png"
    field GrowStage 2

  What every entity has: its type, the state it starts in, flip, its collision (none, circle x y radius, or box x y
  width height: the shape's centre from the foot in pixels, y up), a look for each of its type's states (a bitmap, one
  static sprite, or a sprite sheet, an animation: one of the project's stored assets by its file name, its foot is
  the asset's alignment), and its type's fields. A field left out keeps the type's value.
*/

#define ENTITY_NAME_LENGTH 32
#define ENTITY_TEXT_LENGTH 128
#define ENTITY_LOOK_NAME_LENGTH 256 // NOTE(pvlso): a stored asset's SourceFileName
#define ENTITY_MAX_TYPES 64
#define ENTITY_MAX_STATES 16
#define ENTITY_MAX_FIELDS 32

enum entity_field_kind
{
    EntityField_Int,
    EntityField_Number,
    EntityField_Bool,
    EntityField_Text,

    EntityField_Count,
};

struct entity_field_value
{
    s32 Int;
    r32 Number;
    b32 Bool;
    char Text[ENTITY_TEXT_LENGTH];
};

struct entity_type_field
{
    char Name[ENTITY_NAME_LENGTH];
    entity_field_kind Kind;
    entity_field_value Default;
};

struct entity_type
{
    char Name[ENTITY_NAME_LENGTH];
    u32 StateCount;
    char States[ENTITY_MAX_STATES][ENTITY_NAME_LENGTH];
    u32 FieldCount;
    entity_type_field Fields[ENTITY_MAX_FIELDS];
};

struct entity_types
{
    b32 Loaded;
    u32 Count;
    entity_type Types[ENTITY_MAX_TYPES];
};

enum entity_collision_shape
{
    EntityCollision_None,
    EntityCollision_Circle,
    EntityCollision_Box,
};

enum entity_look_kind
{
    EntityLook_None,
    EntityLook_Bitmap,
    EntityLook_SpriteSheet,
};

struct entity_look
{
    entity_look_kind Kind;
    char FileName[ENTITY_LOOK_NAME_LENGTH];
};

// NOTE(pvlso): An entity as its file has it, its states and fields by their place in its type
struct entity_file
{
    b32 Loaded;
    u32 ErrorCount;

    s32 Type; // NOTE(pvlso): in entity_types, -1 when the file's type isn't one of them
    char TypeName[ENTITY_NAME_LENGTH];
    u32 State;
    b32 Flip;

    entity_collision_shape Collision;
    v2 CollisionCenter; // NOTE(pvlso): from the foot, pixels, y up
    v2 CollisionSize;   // NOTE(pvlso): a box's width and height, a circle's radius in x

    entity_look Looks[ENTITY_MAX_STATES];
    entity_field_value Fields[ENTITY_MAX_FIELDS];
};

#define ENGINE_ENTITY_FILE_H
#endif
