#if !defined(EDITOR_ENTITY_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The entity editor (map editor plan, phase 5b), for the project chosen on the title screen. An entity
  (engine_entity_file.h) is made of one of the project's types, its fields filled in the panel, its looks picked from
  the project's stored assets (the bitmaps and sprite sheets the KEA is built from), and saved in data/entities. The
  canvas shows the look of the state picked in the panel, standing on its foot, with its collision.
*/

// NOTE(pvlso): A stored asset an entity can look like, with what the preview needs of it
struct entity_look_choice
{
    entity_look Look;
    v2 Align;
    u32 SpriteCount;
    u32 SpriteWidth;
    u32 SpriteHeight;
};

struct editor_mode_entity
{
    char ProjectName[PROJECT_NAME_COUNT];
    b32 Exit;
    b32 ExitArmed; // NOTE(pvlso): Exit with unsaved changes asks for a second click

    memory_arena *Arena; // NOTE(pvlso): the mode arena
    entity_types *Types;
    u32 LookChoiceCount;
    entity_look_choice *LookChoices;
    char **LookLabels;

    u32 EntityFileCount;
    char **EntityFiles;
    u32 EntityFileIndex;

    // NOTE(pvlso): The entity being made, and how it was when last checked, to tell when the panel changed it
    b32 HasEntity;
    b32 Dirty;
    char FileName[64]; // NOTE(pvlso): in data/entities
    entity_file Entity;
    entity_file LastEntity;

    // NOTE(pvlso): The preview: the look of ShownState, its picture loaded for that look
    u32 ShownState;
    entity_look PreviewLook;
    loaded_bitmap Preview;
    s32 PreviewChoice; // NOTE(pvlso): in LookChoices, -1 when the look isn't a stored asset
    r32 Zoom;
    r32 Time;

    // NOTE(pvlso): The panel's inputs and requests, done by the next update
    char NameInput[48];
    s32 NameInputLength;
    u32 NewType;
    b32 NewRequested;
    b32 OpenRequested;
    b32 SaveRequested;
    b32 SaveAsRequested;
    b32 ReloadRequested;
};

internal void PlayEntityEditor(editor_state *EditorState, transient_state *TranState, char *ProjectName);

#define EDITOR_ENTITY_MODE_H
#endif
