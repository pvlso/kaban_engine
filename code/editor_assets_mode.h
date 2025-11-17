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

struct tag_map_list
{
    kea_tag_map Tag;
    tag_map_list *Next;
};

struct editor_mode_assets
{
    b32 AssetsInitialized;
    
    b32 Exit;
    assets_edit_mode LastEditMode;
    assets_edit_mode EditMode;

    memory_arena UtilityArena;
    memory_arena UtilityTempArena;
    
    b32 StoredAssetChanged;
    b32 EditStoredAsset;
    b32 RemoveStoredAsset;

    b32 ShowStoredAssets;
    u32 ShowStoredAssetIndex;
    u32 LastShowStoredAssetIndex;

    kesa_header StoredHeader;
    kesa_asset *StoredAssets;
    u64 *TagGUIDs;

    u32 AddAssetCount;
    kesa_asset AssetsToAdd[256];
    
    b32 WriteSSA;
    b32 WriteAssets;

    kesa_asset *CurrentAsset;
    
    b32 AddAsset;
    b32 RemoveTag;
    b32 AddTag;

    u32 LastTagID;
    u32 CurrentTagID;
    u32 CurrentTagValue;
    u32 CurrentTag;

    u32 TagMapListCount;
    tag_map_list *TagMapListHead;
    
    u32 FileIndex;
    u32 LastFileIndex;
    u32 SubFileIndex;
    u32 LastSubFileIndex;

    u32 SourceFileCounts[KESA_Count];
    char **SourceFiles[KESA_Count];
    u32 SolidTileFileCount;
    char **SolidTileFiles;
    
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

inline tag_map_list *
GetTagMapByIndex(editor_mode_assets *AssetsMode, u32 Index)
{
    tag_map_list *Result = AssetsMode->TagMapListHead;
    for(u32 I = 0;
        I < AssetsMode->TagMapListCount;
        ++I)
    {
        if(I == Index)
            break;
        Result = Result->Next;
    }

    return(Result);
}

internal void PlayAssetsMode(editor_state *EditorState, transient_state *TranState);

#define EDITOR_ASSETS_MODE_H
#endif
