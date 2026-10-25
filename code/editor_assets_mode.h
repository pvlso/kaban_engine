#if !defined(EDITOR_ASSETS_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#include "engine_tileset_file.h"
#include "engine_map_file.h"
#include "engine_entity_file.h"

enum assets_edit_mode
{
    EditMode_None,
    EditMode_Bitmap,
    EditMode_SpriteSheet,
    EditMode_Tileset,
    EditMode_Sound,
    EditMode_Text,
    EditMode_File,
    EditMode_SSWM,
    EditMode_Map,
    EditMode_Cut, // NOTE(pvlso): not a stored asset's, the cut tool only writes image files
};

// NOTE(paul): Bitmaps
struct bitmap_mode
{
    loaded_bitmap Bitmap;
};

// NOTE(paul): SpriteSheets
struct spritesheet_mode
{
    b32 ShowAnimated;
    b32 CutSpriteSheet;
    b32 SpriteSheetLoaded;
    loaded_bitmap SpriteSheetBitmap;
    loaded_bitmap Sprites[256];
};

// NOTE(pvlso): Curated tile sets (engine_tileset_file.h): the set read from its text file and its tiles cut from
// their sheets, shown in the set's own grid. Everything but the textures lives in Arena, cleared with the view.
struct tileset_mode
{
    memory_arena Arena;
    b32 Loaded;
    tile_flag_names FlagNames;
    tileset_file File;
    s32 HoverTile; // NOTE(pvlso): -1 when the mouse isn't on a tile

    loaded_bitmap Tiles[TILESET_MAX_TILES]; // NOTE(pvlso): each tile's first frame

    // NOTE(pvlso): Editing (editor_tileset_edit.cpp). The sheet open on the left, any image under data/; what's
    // selected on it in cells; the set's grid on the right; the tile picked for the panel.
    b32 Editing;
    b32 Dirty;
    u32 ImageCount;
    char **Images;
    u32 ImageIndex;
    u32 LastImageIndex;
    loaded_bitmap Sheets[TILESET_MAX_SHEETS]; // NOTE(pvlso): the set's sheets, in Arena, to cut tiles from
    b32 TypingName; // NOTE(pvlso): the panel's name field has the keyboard, Ctrl+Z is the field's
    char OpenSheet[TILESET_SHEET_PATH_LENGTH];
    loaded_bitmap SheetBitmap;
    r32 SheetZoom;  // NOTE(pvlso): screen pixels per cell
    v2 SheetPan;    // NOTE(pvlso): the sheet's top left from the view's, in screen pixels, y down
    r32 SetZoom;
    v2 SetPan;
    b32 Panning;
    v2 PanFrom;
    b32 Selecting;
    b32 HasSelection;
    s32 SelectionX0, SelectionY0, SelectionX1, SelectionY1;
    s32 SelectedTile; // NOTE(pvlso): -1 for none

    // NOTE(pvlso): The panel asks, the next update does it
    b32 SaveRequested;
    b32 FramesRequested;
    b32 RemoveRequested;
    b32 TileChanged; // NOTE(pvlso): the panel edited the selected tile's name, flags or speed
    b32 CreateRequested;
    char NewSetName[64];
    s32 NewSetNameLength;

    // NOTE(pvlso): Undo: every state after a change, History[HistoryAt] is the current one
    u32 HistoryCount;
    u32 HistoryAt;
    struct tileset_snapshot *History[100];
};

// NOTE(pvlso): Maps (engine_map_file.h): the map read from its file, to show what it holds
struct map_mode
{
    memory_arena Arena;
    map_file File;
};

// NOTE(pvlso): The cut tool (editor_cut_edit.cpp): sprites cut out of any image under data/ into PNGs, a rectangle
// into bmps/ (a static sprite) or frames side by side into spritesheets/ (an animation). It only cuts: the files are
// added as stored assets through Edit Bitmaps and Edit SpriteSheets, where their alignment is set.
#define CUT_MAX_FRAMES 64

struct cut_rect
{
    s32 X, Y, W, H; // NOTE(pvlso): pixels, from the image's top left, y down
};

struct cut_mode
{
    memory_arena Arena;
    b32 Started;
    u32 ImageCount;
    char **Images;
    u32 ImageIndex;
    u32 LastImageIndex;
    loaded_bitmap Sheet; // NOTE(pvlso): to show it: premultiplied, bottom row first
    u8 *Pixels;          // NOTE(pvlso): to cut it: as in the file, RGBA, top row first, alpha not premultiplied
    s32 PixelsWidth, PixelsHeight;

    r32 Zoom; // NOTE(pvlso): screen pixels per image pixel
    v2 Pan;   // NOTE(pvlso): the image's top left from the view's, in screen pixels, y down
    b32 Panning;
    v2 PanFrom;
    s32 Snap; // NOTE(pvlso): a drag selects whole cells of this many pixels, 0 for single pixels

    b32 Selecting;
    s32 AnchorX, AnchorY;
    b32 HasSelection;
    cut_rect Selection;
    u32 FrameCount;
    cut_rect Frames[CUT_MAX_FRAMES];
    r32 FramesPerSecond; // NOTE(pvlso): only for the preview, a sprite sheet asset has its own
    s32 SplitCount;      // NOTE(pvlso): how many frames Split cuts the selection into

    char Name[64];
    s32 NameLength;
    b32 Overwrite;
    b32 TypingName; // NOTE(pvlso): the name field has the keyboard, the arrows are the field's

    // NOTE(pvlso): The panel asks, the next update does it
    b32 TrimRequested;
    b32 AddFrameRequested;
    b32 SplitRequested;
    b32 RemoveFrameRequested;
    b32 ClearFramesRequested;
    b32 SaveBitmapRequested;
    b32 SaveSheetRequested;
};

// NOTE(paul): Sounds
struct sound_mode
{
    b32 PlaySound;
    b32 StopSound;
    loaded_sound Sound;
    void *FreeSound;

    playing_sound *PlayingSound;
};

// NOTE(paul): Texts
struct text_mode
{
    b32 EditTextFile;
    b32 Reload;
    b32 Save;

    s32 EditBufferLength;
    u32 EditBufferMaxLength;
    char EditBuffer[4096];

    loaded_text Text;
};

// NOTE(paul): Files
struct binary_file_mode
{
    u8 *FileData;
};

// NOTE(paul): SSWMs
struct sswm_mode
{
    u8 *FileData;
};

static platform_file_type StoredToSourceTypeMap[KESA_Count] =
{
    PlatformFileType_None, PlatformFileType_BMP,
    PlatformFileType_SSBMP, PlatformFileType_TSBMP,
    PlatformFileType_WAV, PlatformFileType_TXT,
    PlatformFileType_BIN, PlatformFileType_KEWM,
    PlatformFileType_Map
};

enum assets_mode_action
{
    AM_Exit              = (1 << 0),
    AM_EditStoredAsset   = (1 << 1),
    AM_RemoveStoredAsset = (1 << 2),

    AM_ShowStoredAssets  = (1 << 3),
    AM_WriteKEA          = (1 << 4),
    AM_WriteAssets       = (1 << 5),
    
    AM_AddAsset          = (1 << 6),
    AM_RemoveAssetTag    = (1 << 7),
    AM_AddAssetTag       = (1 << 8),

    AM_AddNewTag         = (1 << 9),
    AM_AddEditedTag      = (1 << 10),
    AM_RemoveEditedTag   = (1 << 11),
};

struct new_tag_map_value
{
    char Value[KET_TAG_KEY_LENGTH];
    u64 GUID;
    
    new_tag_map_value *Next;
};

struct new_tag_map
{
    kea_tag_map Tag;

    new_tag_map_value ValuesHead;    
    new_tag_map_value *Free;
};

struct editor_mode_assets
{
    // NOTE(pvlso): The project whose data/ this edits
    char ProjectName[PROJECT_NAME_COUNT];

    b32 AssetsInitialized;
    
    u32 Actions;
    assets_edit_mode LastEditMode;
    assets_edit_mode EditMode;

    memory_arena UtilityArena;
    memory_arena UtilityTempArena;
    
    b32 StoredAssetChanged;

    u32 ShowStoredAssetIndex;
    u32 LastShowStoredAssetIndex;

    kesa_header StoredHeader;
    kesa_asset *StoredAssets;

    ket_header TagHeader;
    char **TagKeys;
    // NOTE(pvlso): Sorted by GUID
    kea_tag_map *Tags;

    u32 AddAssetCount;
    kesa_asset AssetsToAdd[256];

    kesa_asset *CurrentAsset;

    u32 LastTagIndex;
    u32 CurrentTagIndex;
    u32 CurrentTagValue;
    u32 CurrentStoredTagIndex;
    
    u32 FileIndex;
    u32 LastFileIndex;
    u32 SubFileIndex;
    u32 LastSubFileIndex;

    s32 SearchLen;
    char Search[128];
    
    u32 SourceFileCounts[KESA_Count];
    char **SourceFiles[KESA_Count];

    b32 CreatingNewTag;
    b32 EditingTag;
    new_tag_map NewTag;
    
    v2 PixelPosition;
    r32 Time;

    union
    {
        bitmap_mode BitmapMode;
        spritesheet_mode SpriteSheetMode;
        tileset_mode TilesetMode;
        sound_mode SoundMode;
        text_mode TextMode;
        binary_file_mode BinaryFileMode;
        sswm_mode SSWMMode;
        map_mode MapMode;
        cut_mode CutMode;
    };
};

inline void
AddAction(editor_mode_assets *AssetsMode, u32 Action)
{
    AssetsMode->Actions |= Action;
}

inline void
RemoveAction(editor_mode_assets *AssetsMode, u32 Action)
{
    AssetsMode->Actions &= ~Action;
}

inline b32
IsAction(editor_mode_assets *AssetsMode, u32 Action)
{
    b32 Result = (AssetsMode->Actions & Action);
    return(Result);
}

inline b32
CheckRemoveAction(editor_mode_assets *AssetsMode, u32 Action)
{
    b32 Result = IsAction(AssetsMode, Action);
    RemoveAction(AssetsMode, Action);
    return(Result);
}

inline assets_edit_mode
AssetsEditModeFromStoredType(u32 StoredType)
{
    assets_edit_mode Result = EditMode_None;
    switch(StoredType)
    {
        case KESA_None:        {}                               break;
        case KESA_Bitmap:      {Result = EditMode_Bitmap;}      break;
        case KESA_SpriteSheet: {Result = EditMode_SpriteSheet;} break;
        case KESA_Tileset:     {Result = EditMode_Tileset;}     break;
        case KESA_Sound:       {Result = EditMode_Sound;}       break;
        case KESA_Text:        {Result = EditMode_Text;}        break;
        case KESA_File:        {Result = EditMode_File;}        break;
        case KESA_SSWM:        {Result = EditMode_SSWM;}        break;
        case KESA_Map:         {Result = EditMode_Map;}         break;
        InvalidDefaultCase;
    }

    return(Result);
}

inline kesa_type
KESAFromEditMode(u32 EditMode)
{
    kesa_type Result = KESA_None;
    switch(EditMode)
    {
        case EditMode_None:        {}                                      break;
        case EditMode_Bitmap:      {Result = KESA_Bitmap;}      break;
        case EditMode_SpriteSheet: {Result = KESA_SpriteSheet;} break;
        case EditMode_Tileset:     {Result = KESA_Tileset;}     break;
        case EditMode_Sound:       {Result = KESA_Sound;}       break;
        case EditMode_Text:        {Result = KESA_Text;}        break;
        case EditMode_File:        {Result = KESA_File;}        break;
        case EditMode_SSWM:        {Result = KESA_SSWM;}        break;
        case EditMode_Map:         {Result = KESA_Map;}         break;
        case EditMode_Cut:         {}                           break;
        InvalidDefaultCase;
    }

    return(Result);
}

inline kea_tag_map *
GetTag(editor_mode_assets *AssetsMode, u64 GUID)
{
    kea_tag_map *Result = 0;
    // TODO(pvlso): Eventually replace this with binary search
    for(u32 I = 0;
        I < AssetsMode->TagHeader.TagCount;
        ++I)
    {
        Result = AssetsMode->Tags + I;
        if(Result->GUID == GUID)
            break;
    }

    return(Result);
}

internal void PlayAssetsMode(editor_state *EditorState, transient_state *TranState, char *ProjectName);

#define EDITOR_ASSETS_MODE_H
#endif
