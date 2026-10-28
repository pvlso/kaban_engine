#if !defined(EDITOR_ENTITY_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The entity editor (map editor plan, phase 5b2), for the project chosen on the title screen. A definition
  (engine_entity_file.h) gets a GUID when it's made, then the panel fills it: display name, category, a look for each
  of the project's clips (picked from the project's stored bitmaps and sprite sheets, the ones the KEA is built from),
  footprint and hit area, the project's blocks it uses and their fields. Saved in data/entities. The canvas shows the
  clip picked in the panel standing on its foot, with the footprint and hit area.
*/

// NOTE(pvlso): A stored asset a clip can look like, with what the preview needs of it
struct entity_look_choice
{
    entity_look_kind Kind;
    char FileName[ENTITY_LOOK_NAME_LENGTH];
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
    entity_project *Project;
    u32 LookChoiceCount;
    entity_look_choice *LookChoices;
    char **LookLabels;

    // NOTE(pvlso): The definitions in data/entities and their GUIDs, to open one or to pick one for an entity field
    u32 EntityFileCount;
    char **EntityFiles;
    u64 *EntityGUIDs;
    u32 EntityFileIndex;

    // NOTE(pvlso): The definition being made, and how it was when last checked, to tell when the panel changed it
    b32 HasEntity;
    b32 Dirty;
    char FileName[64]; // NOTE(pvlso): in data/entities
    entity_file Entity;
    entity_file LastEntity;

    // NOTE(pvlso): The preview: the look of ShownClip, its picture loaded for that look
    u32 ShownClip;
    entity_clip PreviewClip;
    loaded_bitmap Preview;
    s32 PreviewChoice; // NOTE(pvlso): in LookChoices, -1 when the look isn't a stored asset
    r32 Zoom;
    r32 Time; // NOTE(pvlso): since the shown clip started
    b32 ShowFootprint;
    b32 ShowHitArea;

    // NOTE(pvlso): The panel's inputs and requests, done by the next update
    char NameInput[48];
    s32 NameInputLength;
    b32 NewRequested;
    b32 OpenRequested;
    b32 SaveRequested;
    b32 SaveAsRequested;
    b32 ReloadRequested;
};

internal void PlayEntityEditor(editor_state *EditorState, transient_state *TranState, char *ProjectName);

#define EDITOR_ENTITY_MODE_H
#endif
