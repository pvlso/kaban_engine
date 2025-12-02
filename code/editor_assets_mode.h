#if !defined(EDITOR_ASSETS_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

enum assets_edit_mode
{
    EditMode_None,
    EditMode_Bitmap,
    EditMode_SpriteSheet,
    EditMode_Tileset,
    EditMode_Sound,
    EditMode_Text,
    EditMode_Font,
    EditMode_File,
    EditMode_SSWM,
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

// NOTE(paul): Tilesets
struct tileset_mode
{
    b32 CutTileset;
    b32 CutWithMergeTileset;
    b32 ShowTiles;
    u32 CurrentTileIndex;

    loaded_bitmap MergeTileBitmap;
    loaded_bitmap TilesetBitmap;
    loaded_bitmap Tiles[512];
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
    loaded_text Text;
};

// NOTE(paul): Fonts
struct font_mode
{
    builder_loaded_font Font;
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
    PlatformFileType_TTF, PlatformFileType_BIN,
    PlatformFileType_KEWM
};

enum assets_mode_action
{
    AM_Exit              = (1 << 0),
    AM_EditStoredAsset   = (1 << 1),
    AM_RemoveStoredAsset = (1 << 2),

    AM_ShowStoredAssets  = (1 << 3),
    AM_WriteSSA          = (1 << 4),
    AM_WriteAssets       = (1 << 5),
    
    AM_AddAsset          = (1 << 6),
    AM_RemoveTag         = (1 << 7),
    AM_AddTag            = (1 << 8),
    AM_AddNewTag         = (1 << 9),
};

struct editor_mode_assets
{
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

    u32 SourceFileCounts[KESA_Count];
    char **SourceFiles[KESA_Count];
    u32 SolidTileFileCount;
    char **SolidTileFiles;

    b32 CreatingNewTag;
    kea_tag_map NewTag;
    
    v2 PixelPosition;
    r32 Time;

    union
    {
        bitmap_mode BitmapMode;
        spritesheet_mode SpriteSheetMode;
        tileset_mode TilesetMode;
        sound_mode SoundMode;
        text_mode TextMode;
        font_mode FontMode;
        binary_file_mode BinaryFileMode;
        sswm_mode SSWMMode;
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
        case KESA_Font:        {Result = EditMode_Font;}        break;
        case KESA_File:        {Result = EditMode_File;}        break;
        case KESA_SSWM:        {Result = EditMode_SSWM;}        break;
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
        case EditMode_Font:        {Result = KESA_Font;}        break;
        case EditMode_File:        {Result = KESA_File;}        break;
        case EditMode_SSWM:        {Result = KESA_SSWM;}        break;
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

internal void PlayAssetsMode(editor_state *EditorState, transient_state *TranState);

#define EDITOR_ASSETS_MODE_H
#endif
