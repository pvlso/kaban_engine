/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_tileset_file.cpp"
#include "engine_entity_file.cpp"
#include "engine_map_file.cpp"
#include "engine_asset_list.cpp"
#include "engine_kea_builder.cpp"
#include "editor_tileset_edit.cpp"
#include "editor_cut_edit.cpp"
#include "engine_assets_mode_ui.cpp"

// NOTE(pvlso): The tags read by ReadTagFile, freed
internal void
FreeTagMaps(ket_header *Header, kea_tag_map *Tags, char **Keys)
{
    if(Keys)
    {
        Platform.DeallocateMemory(Keys);
    }

    if(Tags)
    {
        for(u32 I = 0; I < Header->TagCount; ++I)
        {
            kea_tag_map *TagMap = Tags + I;
            Platform.DeallocateMemory(TagMap->ValueGUIDs);
            for(u32 J = 0; J < TagMap->ValueCount; ++J)
            {
                Platform.DeallocateMemory(TagMap->Values[J]);
            }
            Platform.DeallocateMemory(TagMap->Values);
        }

        Platform.DeallocateMemory(Tags);
    }
}

inline void
DeallocateAssetsModeTags(editor_mode_assets *AssetsMode)
{
    FreeTagMaps(&AssetsMode->TagHeader, AssetsMode->Tags, AssetsMode->TagKeys);
    AssetsMode->Tags = 0;
    AssetsMode->TagKeys = 0;
}

internal void
TagsFileName(u32 TagFullVersion, char *Dest, umm DestSize)
{
    FormatString(DestSize, Dest, "asset_tags_%d.%d.%d.%d.ket",
                 (TagFullVersion >> 24) & 0xff, (TagFullVersion >> 16) & 0xff,
                 (TagFullVersion >>  8) & 0xff, (TagFullVersion >>  0) & 0xff);
}

// NOTE(pvlso): asset_tags_<Version>.ket read: its header, the tag maps and their keys (FreeTagMaps frees them). False
// without the file, *Tags and *Keys are 0 then.
internal b32
ReadTagFile(u32 TagFullVersion, ket_header *Header, kea_tag_map **TagsOut, char ***KeysOut, memory_arena *TempArena)
{
    b32 Result = false;
    *TagsOut = 0;
    *KeysOut = 0;

    char FileName[256];
    TagsFileName(TagFullVersion, FileName, sizeof(FileName));
    if(Platform.FileExists(FileName, PlatformFileType_KET))
    {
        Result = true;
        platform_file_handle KETHandle = Platform.OpenFile(FileName, PlatformFileType_KET, PlatformFileOp_Read);
        Platform.ReadDataFromFile(&KETHandle, 0, sizeof(ket_header), Header);

        Assert(Header->Version == TagFullVersion);
        Assert(Header->TagCount);

        u32 TagsSize = Header->TagCount*sizeof(ket_tag);

        temporary_memory TempMem = BeginTemporaryMemory(TempArena);
        ket_tag *RawTags = PushArray(TempMem.Arena, Header->TagCount, ket_tag);
        Platform.ReadDataFromFile(&KETHandle, Header->TagArrayOffset, TagsSize, RawTags);

        char **Keys = (char **)Platform.AllocateMemory(Header->TagCount*sizeof(char *));
        kea_tag_map *Tags = (kea_tag_map *)Platform.AllocateMemory(Header->TagCount*sizeof(kea_tag_map));
        *KeysOut = Keys;
        *TagsOut = Tags;
        for(u32 I = 0; I < Header->TagCount; ++I)
        {
            ket_tag *Tag = RawTags + I;
            kea_tag_map *Map = Tags + I;
            Map->Values = (char **)Platform.AllocateMemory(Tag->ValueCount*sizeof(char *));
            Map->ValueGUIDs = (u64 *)Platform.AllocateMemory(Tag->ValueCount*sizeof(u64));
            for(u32 J = 0; J < Tag->ValueCount; ++J)
            {
                Map->Values[J] = (char *)Platform.AllocateMemory(KET_TAG_KEY_LENGTH);
            }

            Map->GUID = Tag->GUID;
            Copy(KET_TAG_KEY_LENGTH, Tag->Key, Map->Key);
            Map->ValueCount = Tag->ValueCount;

            u32 SizeOfGUIDs = Map->ValueCount*sizeof(u64);
            Platform.ReadDataFromFile(&KETHandle, Tag->DataOffset, SizeOfGUIDs, Map->ValueGUIDs);
            
            u32 StringLength = Tag->ValueCount*KET_TAG_KEY_LENGTH;
            char *ValuesString = PushArray(TempMem.Arena, StringLength, char);
            Platform.ReadDataFromFile(&KETHandle, Tag->DataOffset + SizeOfGUIDs, StringLength, ValuesString);
            char *StringAt = ValuesString;

            for(u32 J = 0; J < Map->ValueCount; ++J)
            {
                Copy(KET_TAG_KEY_LENGTH, StringAt, Map->Values[J]);
                StringAt += KET_TAG_KEY_LENGTH;
            }

            Keys[I] = Map->Key;
        }

        EndTemporaryMemory(TempMem);

        Platform.CloseFile(&KETHandle);
    }

    return(Result);
}

internal b32
ReadTags(editor_mode_assets *AssetsMode)
{
    Assert(AssetsMode->StoredHeader.MagicValue);
    ket_header *Header = &AssetsMode->TagHeader;

    DeallocateAssetsModeTags(AssetsMode);

    u32 TagFullVersion = AssetsMode->StoredHeader.TagsVersion;
    char FileName[256];
    TagsFileName(TagFullVersion, FileName, sizeof(FileName));
    b32 Result = ReadTagFile(TagFullVersion, Header, &AssetsMode->Tags, &AssetsMode->TagKeys,
                             &AssetsMode->UtilityTempArena);
    if(Result)
    {
        LOG("Read %s: %u tags", FileName, Header->TagCount);
    }
    else
    {
        LOG_WARNING("No %s, writing one with only Tag_None", FileName);
        platform_file_handle KETHandle = Platform.OpenFile(FileName, PlatformFileType_KET, PlatformFileOp_Write);
        if(PlatformNoFileErrors(&KETHandle))
        {
            Header->MagicValue = KET_MAGIC_VALUE;
            Header->Version = TagFullVersion;
            Header->TagCount = 1;
            Header->TagArrayOffset = sizeof(ket_header);

            ket_tag NullTag = {};
            FormatString(ArrayCount(NullTag.Key), NullTag.Key, "Tag_None");
            NullTag.ValueCount = 1;
            NullTag.DataOffset = Header->TagArrayOffset + sizeof(ket_tag);

            
            char Value[KET_TAG_KEY_LENGTH] = {};
            FormatString(ArrayCount(Value), Value, "None");
            u64 ValueGUID = GUIDFromString(Value);

            Platform.WriteDataToFile(&KETHandle, 0, sizeof(ket_header), Header);
            Platform.WriteDataToFile(&KETHandle, Header->TagArrayOffset, sizeof(ket_tag), &NullTag);
            Platform.WriteDataToFile(&KETHandle, NullTag.DataOffset, sizeof(u64), &ValueGUID);
            Platform.WriteDataToFile(&KETHandle, NullTag.DataOffset + sizeof(u64), KET_TAG_KEY_LENGTH, Value);

            Platform.CloseFile(&KETHandle);
        }
        else
        {
            LOG_ERROR("Could not create %s", FileName);
        }
    }

    return(Result);
}

// NOTE(pvlso): stored_assets_<version>.kesa, the version from the data's editor_metadata.json. Only read once, to write
// data/assets.txt from it (ConvertStoredAssets).
internal void
StoredAssetsFileName(editor_meta *EditorMeta, char *Dest, u32 DestSize)
{
    FormatString(DestSize, Dest, "stored_assets_%d.%d.%d.%d.kesa",
                 EditorMeta->KESAVersion[0], EditorMeta->KESAVersion[1],
                 EditorMeta->KESAVersion[2], EditorMeta->KESAVersion[3]);
}

// NOTE(pvlso): The last stored assets file the project wrote (the first is the null asset), 0 without it;
// Platform.DeallocateMemory frees them
internal kesa_asset *
ReadLastStoredAssets(editor_meta *EditorMeta, kesa_header *Header)
{
    kesa_asset *Result = 0;
    char StoredFileName[256];
    StoredAssetsFileName(EditorMeta, StoredFileName, sizeof(StoredFileName));
    if(Platform.FileExists(StoredFileName, PlatformFileType_KESA))
    {
        platform_file_handle Handle = Platform.OpenFile(StoredFileName, PlatformFileType_KESA, PlatformFileOp_Read);
        Platform.ReadDataFromFile(&Handle, 0, sizeof(kesa_header), Header);
        if(PlatformNoFileErrors(&Handle) && (Header->SizeOfStoredAsset == sizeof(kesa_asset)) && Header->AssetCount)
        {
            u32 AssetsSize = Header->AssetCount*Header->SizeOfStoredAsset;
            Result = (kesa_asset *)Platform.AllocateMemory(AssetsSize);
            Platform.ReadDataFromFile(&Handle, Header->AssetsOffset, AssetsSize, Result);
        }
        Platform.CloseFile(&Handle);
    }
    if(!Result)
    {
        ZeroStruct(*Header);
    }

    return(Result);
}

// NOTE(pvlso): A project without data/assets.txt gets one, from its last stored assets file when it has one: what the
// stored assets knew that their files don't (tags, alignments, frame sizes) carries over
internal void
ConvertStoredAssets(editor_state *EditorState, editor_mode_assets *AssetsMode)
{
    if(!Platform.FileExists(ASSET_LIST_FILE_NAME, PlatformFileType_DataRoot))
    {
        kesa_header Header = {};
        kesa_asset *Stored = ReadLastStoredAssets(&EditorState->EditorMeta, &Header);
        kesa_asset Null = {};
        WriteAssetList(Stored ? Stored : &Null, Stored ? Header.AssetCount : 1, AssetsMode->Tags,
                       AssetsMode->TagHeader.TagCount, &AssetsMode->UtilityTempArena);
        if(Stored)
        {
            char StoredFileName[256];
            StoredAssetsFileName(&EditorState->EditorMeta, StoredFileName, sizeof(StoredFileName));
            LOG("Wrote %s from %s (%u assets); stored assets files aren't read any more", ASSET_LIST_FILE_NAME,
                StoredFileName, Header.AssetCount - 1);
            Platform.DeallocateMemory(Stored);
        }
    }
}

// NOTE(pvlso): The asset folders scanned into StoredAssets, each type's names for the pickers. The current asset is
// looked up again by its type and name (NoAsset when it's gone); the shown one stays in range.
internal void
ScanAssets(editor_mode_assets *AssetsMode)
{
    kesa_type CurrentType = KESA_None;
    char CurrentName[sizeof(AssetsMode->NoAsset.SourceFileName)] = {};
    if(AssetsMode->CurrentAsset)
    {
        CurrentType = AssetsMode->CurrentAsset->Type;
        FormatString(sizeof(CurrentName), CurrentName, "%s", AssetsMode->CurrentAsset->SourceFileName);
    }

    u32 Version = AssetsMode->StoredHeader.Version;
    if(AssetsMode->StoredAssets) {Platform.DeallocateMemory(AssetsMode->StoredAssets);}
    if(AssetsMode->AssetNames) {Platform.DeallocateMemory(AssetsMode->AssetNames);}
    AssetsMode->StoredAssets = ScanAssetList(&AssetsMode->StoredHeader, AssetsMode->Tags,
                                             AssetsMode->TagHeader.TagCount, &AssetsMode->UtilityTempArena);
    AssetsMode->StoredHeader.Version = Version;

    u32 Count = AssetsMode->StoredHeader.AssetCount;
    AssetsMode->AssetNames = (char **)Platform.AllocateMemory(Count*sizeof(char *));
    ZeroArray(KESA_Count, AssetsMode->SourceFileCounts);
    for(u32 Type = 0; Type < KESA_Count; ++Type) {AssetsMode->SourceFiles[Type] = AssetsMode->AssetNames;}
    for(u32 Index = 1; Index < Count; ++Index)
    {
        kesa_asset *Asset = AssetsMode->StoredAssets + Index;
        AssetsMode->AssetNames[Index] = Asset->SourceFileName;
        if(!AssetsMode->SourceFileCounts[Asset->Type]) {AssetsMode->SourceFiles[Asset->Type] = AssetsMode->AssetNames + Index;}
        ++AssetsMode->SourceFileCounts[Asset->Type];
    }

    AssetsMode->NoAsset = {};
    AssetsMode->NoAsset.Type = CurrentType;
    if(AssetsMode->CurrentAsset)
    {
        kesa_asset *Found = FindListedAsset(AssetsMode->StoredAssets, Count, CurrentType, CurrentName);
        AssetsMode->CurrentAsset = Found ? Found : &AssetsMode->NoAsset;
    }
    if(AssetsMode->ShowStoredAssetIndex >= Count) {AssetsMode->ShowStoredAssetIndex = Count - 1;}
    if(AssetsMode->LastShowStoredAssetIndex >= Count) {AssetsMode->LastShowStoredAssetIndex = Count - 1;}

    LOG("Scanned the asset folders: %u assets", Count - 1);
}

internal void
SaveAssetList(editor_mode_assets *AssetsMode)
{
    if(!WriteAssetList(AssetsMode->StoredAssets, AssetsMode->StoredHeader.AssetCount, AssetsMode->Tags,
                       AssetsMode->TagHeader.TagCount, &AssetsMode->UtilityTempArena))
    {
        LOG("%s: nothing changed", ASSET_LIST_FILE_NAME);
    }
}

// NOTE(pvlso): What was edited is saved first, a scan would lose it
internal void
RescanAssets(editor_mode_assets *AssetsMode)
{
    SaveAssetList(AssetsMode);
    ScanAssets(AssetsMode);
}

// NOTE(pvlso): Where an asset is in its type's picker
internal u32
SourceFileIndexOf(editor_mode_assets *AssetsMode, kesa_asset *Asset)
{
    u32 Result = 0;
    for(u32 Index = 0; Index < AssetsMode->SourceFileCounts[Asset->Type]; ++Index)
    {
        if(AssetsMode->SourceFiles[Asset->Type][Index] == Asset->SourceFileName) {Result = Index;}
    }

    return(Result);
}

// NOTE(pvlso): data/keas/last_build.txt: what the newest KEA was built from (AssetDataSignature), so a Run can tell
// whether the data changed since, even across editor sessions
#define LAST_BUILD_FILE_NAME "last_build.txt"

internal b32 ParseGameDataKEAVersion(char *Name, u32 *Version);

internal u64
ReadLastBuildSignature(memory_arena *Arena)
{
    u64 Result = 0;
    if(Platform.FileExists(LAST_BUILD_FILE_NAME, PlatformFileType_KEA))
    {
        temporary_memory TempMem = BeginTemporaryMemory(Arena);
        read_file_result File = Platform.ReadEntireFile(LAST_BUILD_FILE_NAME, PlatformFileType_KEA, TempMem.Arena, true);
        for(char *At = (char *)File.Contents; At && *At; ++At)
        {
            u32 Digit = IsDigit(*At) ? (u32)(*At - '0') :
                ((*At >= 'a') && (*At <= 'f')) ? (u32)(*At - 'a' + 10) : 16;
            if(Digit < 16) {Result = (Result << 4) | Digit;}
            else if(*At == '\n') {break;}
        }
        EndTemporaryMemory(TempMem);
    }

    return(Result);
}

// NOTE(pvlso): Whether data/keas has a game_data_<version>.kea
internal b32
HasGameDataKEA(memory_arena *Arena)
{
    b32 Result = false;
    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    u32 Count = Platform.ListFilesInDirectory(PlatformFileType_KEA, 0, 0);
    char **Names = PushArray(TempMem.Arena, Maximum(1, Count), char *);
    Count = Platform.ListFilesInDirectory(PlatformFileType_KEA, Names, TempMem.Arena);
    for(u32 Index = 0; Index < Count; ++Index)
    {
        u32 Version;
        if(ParseGameDataKEAVersion(Names[Index], &Version)) {Result = true;}
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}

// NOTE(pvlso): The newest game_data_<version>.kea in data/keas into Dest, false (and Dest empty) when there's none
internal b32
NewestGameDataKEA(char *Dest, umm DestCount, memory_arena *Arena)
{
    b32 Result = false;
    Dest[0] = 0;
    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    u32 Count = Platform.ListFilesInDirectory(PlatformFileType_KEA, 0, 0);
    char **Names = PushArray(TempMem.Arena, Maximum(1, Count), char *);
    Count = Platform.ListFilesInDirectory(PlatformFileType_KEA, Names, TempMem.Arena);
    u32 BestVersion = 0;
    for(u32 Index = 0; Index < Count; ++Index)
    {
        u32 Version;
        if(ParseGameDataKEAVersion(Names[Index], &Version) && (!Result || (Version > BestVersion)))
        {
            FormatString(DestCount, Dest, "%s", Names[Index]);
            BestVersion = Version;
            Result = true;
        }
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}

// NOTE(pvlso): Every game_data_<version>.kea but the newest and KeepOpen (the one a running game reads; Windows can't
// delete it anyway) is deleted: only the newest is ever loaded
internal void
DeleteOlderKEAs(u32 NewestVersion, char *KeepOpen, memory_arena *Arena)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    u32 Count = Platform.ListFilesInDirectory(PlatformFileType_KEA, 0, 0);
    char **Names = PushArray(TempMem.Arena, Maximum(1, Count), char *);
    Count = Platform.ListFilesInDirectory(PlatformFileType_KEA, Names, TempMem.Arena);
    for(u32 Index = 0; Index < Count; ++Index)
    {
        u32 Version;
        if(ParseGameDataKEAVersion(Names[Index], &Version) && (Version < NewestVersion) &&
           !(KeepOpen && StringsAreEqual(Names[Index], KeepOpen)))
        {
            if(!Platform.RemoveFile(Names[Index], PlatformFileType_KEA))
            {
                LOG_WARNING("Could not delete the old %s", Names[Index]);
            }
        }
    }
    EndTemporaryMemory(TempMem);
}

// NOTE(pvlso): The project's KEA built from its folders, without the assets mode: the newest KET's tags, a scan, the
// next version (editor_metadata.json). The data's signature is taken before the scan, so a file saved during a build
// makes the next check build again. The older KEAs go, but KeepOpen. Built gets the new file's name.
internal b32
BuildProjectKEA(editor_state *EditorState, memory_arena *TempArena, char *KeepOpen, char *Built, umm BuiltSize)
{
    TIMED_FUNCTION();

    f64 Start = Platform.GetSeconds();
    u64 Signature = AssetDataSignature(TempArena);

    temporary_memory TempMem = BeginTemporaryMemory(TempArena);
    ket_header TagHeader = {};
    kea_tag_map *Tags = 0;
    char **Keys = 0;
    if(!ReadTagFile(NewestTagsVersion(TempMem.Arena), &TagHeader, &Tags, &Keys, TempMem.Arena))
    {
        ZeroStruct(TagHeader);
        LOG_WARNING("No asset_tags_*.ket in data/kets: the KEA is built without tags");
    }

    // NOTE(pvlso): The newest KEA is the previous build, its unchanged bitmaps are copied (BuildKEA)
    char Previous[256];
    NewestGameDataKEA(Previous, sizeof(Previous), TempMem.Arena);

    kesa_header Header = {};
    kesa_asset *List = ScanAssetList(&Header, Tags, TagHeader.TagCount, TempMem.Arena);
    Header.Version = UpdateEditorVersionFile(EditorState);
    b32 Result = BuildKEA(&Header, List, &TagHeader, Tags, TempMem.Arena, Previous);
    Platform.DeallocateMemory(List);
    FreeTagMaps(&TagHeader, Tags, Keys);
    EndTemporaryMemory(TempMem);

    if(Built) {FormatString(BuiltSize, Built, "game_data_%u.kea", Header.Version);}
    if(Result)
    {
        char Text[64];
        FormatString(sizeof(Text), Text, "%016llx\n", Signature);
        Platform.WriteEntireFile(LAST_BUILD_FILE_NAME, PlatformFileType_KEA, (u8 *)Text, StringLength(Text));
        DeleteOlderKEAs(Header.Version, KeepOpen, TempArena);
        LOG("Built game_data_%u.kea from %u assets in %.2f s", Header.Version, Header.AssetCount - 1,
            Platform.GetSeconds() - Start);
    }
    else
    {
        LOG_ERROR("Building game_data_%u.kea failed", Header.Version);
    }

    return(Result);
}

// NOTE(pvlso): Whether the KEA has to be built: there's none, or the data changed since the last build
internal b32
ProjectKEAIsStale(memory_arena *TempArena)
{
    b32 Result = (!HasGameDataKEA(TempArena) || (AssetDataSignature(TempArena) != ReadLastBuildSignature(TempArena)));
    return(Result);
}

// NOTE(pvlso): The KEA build on a worker thread (the high priority queue), so the editor doesn't stop while it runs
// (kea_build_job). The data root mustn't change under it: UseDataOf waits for it first.
internal PLATFORM_WORK_QUEUE_CALLBACK(DoKEABuildWork)
{
    kea_build_job *Job = (kea_build_job *)Data;
    Job->Result = BuildProjectKEA(Job->EditorState, &Job->Arena, Job->KeepOpen[0] ? Job->KeepOpen : 0, Job->Built,
                                  sizeof(Job->Built));
    Clear(&Job->Arena);

    CompletePreviousWritesBeforeFutureWrites;
    Job->Running = false;
}

// NOTE(pvlso): False when a build is already running (its result comes first, then a new check)
internal b32
StartKEABuild(editor_state *EditorState, transient_state *TranState, char *KeepOpen)
{
    kea_build_job *Job = &EditorState->KEABuild;
    b32 Result = !Job->Running;
    if(Result)
    {
        Job->EditorState = EditorState;
        Job->Queue = TranState->HighPriorityQueue;
        Job->Result = false;
        Job->Built[0] = 0;
        FormatString(sizeof(Job->KeepOpen), Job->KeepOpen, "%s", KeepOpen ? KeepOpen : "");
        Job->Running = true;
        CompletePreviousWritesBeforeFutureWrites;
        Platform.AddEntry(Job->Queue, DoKEABuildWork, Job);
    }

    return(Result);
}

internal void
WaitForKEABuild(editor_state *EditorState)
{
    kea_build_job *Job = &EditorState->KEABuild;
    if(Job->Running)
    {
        LOG("Waiting for the KEA build to finish");
        Platform.CompleteAllWork(Job->Queue);
    }
}

internal void
PlayAssetsMode(editor_state *EditorState, transient_state *TranState, char *ProjectName)
{
    // NOTE(pvlso): ProjectName lives in the mode arena, which SetEditorMode frees
    char Name[PROJECT_NAME_COUNT];
    FormatString(sizeof(Name), Name, "%s", ProjectName);

    SetEditorMode(EditorState, TranState, EditorMode_AssetsMode);
    LOG("Assets Mode for %s", Name);

    // NOTE(pvlso): Every file below (source files, stored assets, tags, the KEA it writes) is the project's
    UseDataOf(EditorState, Name);

    editor_mode_assets *Result = PushStruct(&EditorState->ModeArena, editor_mode_assets);
    FormatString(sizeof(Result->ProjectName), Result->ProjectName, "%s", Name);
    Result->EditMode = EditMode_None;
    SubArena(&Result->UtilityTempArena, &EditorState->ModeArena, Megabytes(1));
    SubArena(&Result->UtilityArena, &EditorState->ModeArena, Megabytes(1));

    // NOTE(pvlso): Setting it to one because first stored asset is always zero; the folders are scanned on the first
    // update
    Result->ShowStoredAssetIndex = 1;

    EditorState->AssetsMode = Result;
}

internal void
CutSpriteSheet(platform_texture_op_queue *TextureOpQueue, spritesheet_mode *SpriteSheetMode,
               kesa_spritesheet *Asset)
{
    TIMED_FUNCTION();

    loaded_bitmap *SpriteSheet = &SpriteSheetMode->SpriteSheetBitmap;
    for(u32 SpriteIndex = 0;
        SpriteIndex < Asset->SpriteCount;
        ++SpriteIndex)
    {
        loaded_bitmap *Sprite = SpriteSheetMode->Sprites + SpriteIndex;
        Sprite->Width = Asset->SpriteWidth;
        Sprite->Height = Asset->SpriteHeight;
        Sprite->WidthOverHeight = (r32)Sprite->Width/(r32)Sprite->Height;
        Sprite->Pitch = Asset->SpriteWidth*BITMAP_BYTES_PER_PIXEL;
        Sprite->AlignPercentage = V2(0.5f, 0.5f);
        u32 MemorySize = Sprite->Pitch*Sprite->Height;
        Sprite->Memory = Platform.AllocateMemory(MemorySize);

        u8 *SourceRow = (u8 *)(SpriteSheet->Memory) + SpriteIndex*Sprite->Pitch;
        u8 *DestRow = (u8 *)(Sprite->Memory);
        for(s32 Y = 0;
            Y < Sprite->Height;
            ++Y)
        {
            u32 *Source = (u32 *)SourceRow;
            u32 *Dest = (u32 *)DestRow;
            for(s32 X = 0;
                X < Sprite->Width;
                ++X)
            {
                *Dest++ = *Source++;
            }

            SourceRow += SpriteSheet->Pitch;
            DestRow += Sprite->Pitch;
        }

        texture_op Op = {};
        Op.IsAllocate = true;
        Op.Allocate.Width = Sprite->Width;
        Op.Allocate.Height = Sprite->Height;
        Op.Allocate.Data = Sprite->Memory;
        Op.Allocate.ResultHandle = &Sprite->TextureHandle;
        AddOp(TextureOpQueue, &Op);
    }
}

internal void AllocateBitmap(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Bitmap);

// NOTE(pvlso): Reads a curated tile set and cuts each tile's first frame from its sheets, to show the set
internal void
LoadTilesetView(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode, kesa_asset *Asset)
{
    TIMED_FUNCTION();

    memory_arena *Arena = &TilesetMode->Arena;
    TilesetMode->FlagNames = ReadTileFlagNames(Arena);
    TilesetMode->File = ReadTilesetFile(Asset->SourceFileName, &TilesetMode->FlagNames, Arena);
    TilesetMode->HoverTile = -1;
    tileset_file *File = &TilesetMode->File;

    loaded_bitmap *Sheets = TilesetMode->Sheets;
    for(u32 SheetIndex = 0; SheetIndex < File->SheetCount; ++SheetIndex)
    {
        Sheets[SheetIndex] = LoadImageFile(File->Sheets[SheetIndex], PlatformFileType_DataRoot, Arena);
        if(!Sheets[SheetIndex].Memory)
        {
            LOG_ERROR("Tile set %s: can't read the sheet %s", Asset->SourceFileName, File->Sheets[SheetIndex]);
        }
    }

    for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
    {
        tileset_file_cell Cell = File->Tiles[TileIndex].Frames[0];
        loaded_bitmap *Tile = TilesetMode->Tiles + TileIndex;
        *Tile = CutTilesetCell(Sheets + Cell.Sheet, File->TileSize, Cell.X, Cell.Y, Arena);
        if(Tile->Memory)
        {
            AllocateBitmap(TextureOpQueue, Tile);
        }
    }

    Asset->Tileset.TileCount = File->TileCount;
    Asset->Tileset.GridWidth = File->GridWidth;
    Asset->Tileset.GridHeight = File->GridHeight;
    Asset->Tileset.TileSize = File->TileSize;
    TilesetMode->Loaded = File->Loaded;
}

// NOTE(pvlso): The tiles' pixels are in the view's arena, so only their textures are released one by one
internal void
ClearTilesetView(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode)
{
    for(u32 TileIndex = 0; TileIndex < ArrayCount(TilesetMode->Tiles); ++TileIndex)
    {
        loaded_bitmap *Tile = TilesetMode->Tiles + TileIndex;
        if(Tile->TextureHandle)
        {
            texture_op Op = {};
            Op.IsAllocate = false;
            Op.Deallocate.Handle = Tile->TextureHandle;
            AddOp(TextureOpQueue, &Op);
        }
    }
    ClearTilesetEditing(TextureOpQueue, TilesetMode);
    Clear(&TilesetMode->Arena);
}

internal void
AddTagToCurrentAsset(editor_mode_assets *AssetsMode, kesa_asset *CurrentAsset)
{
    kesa_tag *Tag = CurrentAsset->AssetTags + CurrentAsset->TagCount++;
    kea_tag_map *Map = AssetsMode->Tags + AssetsMode->CurrentTagIndex;

    Tag->TagGUID = Map->GUID;
    Tag->TagValueIndex = AssetsMode->CurrentTagValue;
}

inline void
TagInsertionSort(u32 Count, ket_tag *Nodes)
{
    for(u32 I = 1; I < Count; ++I)
    {
        ket_tag Node = Nodes[I];
        s32 J = I - 1;

        while((J >= 0) && (Node.GUID < Nodes[J].GUID))
        {
            Nodes[J + 1] = Nodes[J];
            J = J - 1;
        }

        Nodes[J + 1] = Node;
    }
}

enum update_tags_op
{
    UpdateTagsOp_AddNew,
    UpdateTagsOp_Edit,
    UpdateTagsOp_Remove,
};

internal void
UpdateEditorTags(editor_mode_assets *AssetsMode, update_tags_op Op)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);
    b32 Result = true;
    Assert(AssetsMode->StoredHeader.MagicValue);
    ket_header *Header = &AssetsMode->TagHeader;
   
    u32 TagFullVersion = ++AssetsMode->StoredHeader.TagsVersion;
    u8 TagVersion[4] =
        {
            (u8)((TagFullVersion >> 24) & 0xff),
            (u8)((TagFullVersion >> 16) & 0xff),
            (u8)((TagFullVersion >>  8) & 0xff),
            (u8)((TagFullVersion >>  0) & 0xff),
        };

    char FileName[256];
    FormatString(ArrayCount(FileName), FileName, "asset_tags_%d.%d.%d.%d.ket",
                 TagVersion[0], TagVersion[1],
                 TagVersion[2], TagVersion[3]);
    
    platform_file_handle KETHandle =
        Platform.OpenFile(FileName, PlatformFileType_KET, PlatformFileOp_Write);
    if(PlatformNoFileErrors(&KETHandle))
    {
        ket_tag *ExistingTags = 0;
        new_tag_map *NewTag = &AssetsMode->NewTag;

        char Buffer[KET_TAG_KEY_LENGTH*2];

        Header->TagCount += (Op == UpdateTagsOp_AddNew) ? 1 : 0;

        u32 AllocTagCount = (Op == UpdateTagsOp_Remove) ? (Header->TagCount - 1) : Header->TagCount;
        ExistingTags = PushArray(TempMem.Arena, AllocTagCount, ket_tag);
        u64 DataOffset = Header->TagArrayOffset + AllocTagCount*sizeof(ket_tag);

        u32 *TagStringSizes = PushArray(TempMem.Arena, AllocTagCount, u32);
        char **TagStrings = PushArray(TempMem.Arena, AllocTagCount, char *);
        u32 IndexDecrement = 0;
        for(u32 I = 0; I < Header->TagCount; ++I)
        {
            u32 Index = I - IndexDecrement;

            kea_tag_map *TagMap = AssetsMode->Tags + I;
            ket_tag *Tag = ExistingTags + Index;

            if(StringsAreEqual(TagMap->Key, "Remove"))
            {
                IndexDecrement = 1;
                continue;
            }

            Tag->DataOffset = DataOffset;
            if(((Op == UpdateTagsOp_Edit) && (TagMap->GUID == NewTag->Tag.GUID)) ||
               ((Op == UpdateTagsOp_AddNew) && (I == (Header->TagCount - 1))))
            {
                Assert(I == Index);

                NewTag->Tag.GUID = GUIDFromString(NewTag->Tag.Key);
                TagStringSizes[Index] = NewTag->Tag.ValueCount*KET_TAG_KEY_LENGTH;
                TagStrings[Index] = PushArray(TempMem.Arena, TagStringSizes[Index], char);
                NewTag->Tag.ValueGUIDs = PushArray(TempMem.Arena, NewTag->Tag.ValueCount, u64);

                
                new_tag_map_value *Prev = 0;
                new_tag_map_value *Cur = NewTag->ValuesHead.Next;
                while(Cur != 0)
                {
                    new_tag_map_value *Next = Cur->Next;
                    Cur->Next = Prev;
                    Prev = Cur;
                    Cur = Next;
                        
                }
                NewTag->ValuesHead.Next = Prev;
                
                char *TagStringAt = TagStrings[Index];
                for(u32 ValueIndex = 0;
                    ValueIndex < NewTag->Tag.ValueCount;
                    ++ValueIndex)
                {
                    new_tag_map_value *Remove = NewTag->ValuesHead.Next;
                    NewTag->ValuesHead.Next = Remove->Next;                        

                    Remove->Next = NewTag->Free;
                    NewTag->Free = Remove;

                    Copy(KET_TAG_KEY_LENGTH, Remove->Value, TagStringAt);
                    TagStringAt += KET_TAG_KEY_LENGTH;

                    FormatString(ArrayCount(Buffer), Buffer, "%s%s", NewTag->Tag.Key, Remove->Value);
                    NewTag->Tag.ValueGUIDs[ValueIndex] = GUIDFromString(Buffer);

                    ZeroStruct(*Remove);
                }

                Tag->GUID = NewTag->Tag.GUID;
                Copy(KET_TAG_KEY_LENGTH, NewTag->Tag.Key, Tag->Key);
                Tag->ValueCount = NewTag->Tag.ValueCount;

                u32 GUIDsSize = Tag->ValueCount*sizeof(u64);
                Platform.WriteDataToFile(&KETHandle, Tag->DataOffset, GUIDsSize, NewTag->Tag.ValueGUIDs);
                DataOffset += GUIDsSize;

                ZeroStruct(NewTag->Tag);
            }
            else
            {
                TagStringSizes[Index] = TagMap->ValueCount*KET_TAG_KEY_LENGTH;
                TagStrings[Index] = PushArray(TempMem.Arena, TagStringSizes[Index], char);

                char *TagStringAt = TagStrings[Index];
                for(u32 ValueIndex = 0;
                    ValueIndex < TagMap->ValueCount;
                    ++ValueIndex)
                {
                    Copy(KET_TAG_KEY_LENGTH, TagMap->Values[ValueIndex], TagStringAt);
                    TagStringAt += KET_TAG_KEY_LENGTH;
                }

                Tag->GUID = TagMap->GUID;
                Copy(KET_TAG_KEY_LENGTH, TagMap->Key, Tag->Key);
                Tag->ValueCount = TagMap->ValueCount;

                u32 GUIDsSize = Tag->ValueCount*sizeof(u64);
                Platform.WriteDataToFile(&KETHandle, Tag->DataOffset, GUIDsSize, TagMap->ValueGUIDs);
                DataOffset += GUIDsSize;
            }

            Platform.WriteDataToFile(&KETHandle, DataOffset, TagStringSizes[Index], TagStrings[Index]);
            DataOffset += TagStringSizes[Index];
        }

        Header->TagCount -= IndexDecrement;
        
        TagInsertionSort(Header->TagCount, ExistingTags);
            
        Header->Version = TagFullVersion;
        Platform.WriteDataToFile(&KETHandle, 0, sizeof(ket_header), Header);
        Platform.WriteDataToFile(&KETHandle, sizeof(ket_header), sizeof(ket_tag)*Header->TagCount, ExistingTags);

        Platform.CloseFile(&KETHandle);
    }
    else
    {
        // TODO(pvlso): Logging
    }
    
    EndTemporaryMemory(TempMem);

    // NOTE(pvlso): The newest KET is the one read, there's no stored assets file to point at it any more
    char *OpNames[] = {"added", "edited", "removed"};
    LOG("Tag %s, tags version is now %u", (Op < ArrayCount(OpNames)) ? OpNames[Op] : "?", TagFullVersion);

    AssetsMode->CurrentTagIndex = 0;
    
    //NOTE(pvlso): Update tags
    ReadTags(AssetsMode);
}

internal void
RemoveCurrentAssetTag(editor_mode_assets *AssetsMode, kesa_asset *CurrentAsset)
{
    if(CurrentAsset->TagCount)
    {
        for(u32 TagIndex = AssetsMode->CurrentStoredTagIndex;
            TagIndex < (ArrayCount(CurrentAsset->AssetTags) - 1);
            ++TagIndex)
        {
            CurrentAsset->AssetTags[TagIndex] = CurrentAsset->AssetTags[TagIndex + 1];        
        }

        kesa_tag *Tag = CurrentAsset->AssetTags + (ArrayCount(CurrentAsset->AssetTags) - 1);
        *Tag = {};

        --CurrentAsset->TagCount;
    }
}

internal void
DeallocateBitmap(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Bitmap)
{
    if(Bitmap)
    {
        if(Bitmap->TextureHandle)
        {
            texture_op Op = {};
            Op.IsAllocate = false;
            Op.Deallocate.Handle = Bitmap->TextureHandle;
            AddOp(TextureOpQueue, &Op);
        }

        Platform.FreeFileMemory(Bitmap->Memory);
    }
}

internal void
AllocateBitmap(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Bitmap)
{
    texture_op Op = {};
    Op.IsAllocate = true;
    Op.Allocate.Width = Bitmap->Width;
    Op.Allocate.Height = Bitmap->Height;
    Op.Allocate.Data = Bitmap->Memory;
    Op.Allocate.ResultHandle = &Bitmap->TextureHandle;
    AddOp(TextureOpQueue, &Op);
}

inline void
DrawTileGrid(render_group *RenderGroup, object_transform *Transform, r32 HalfCanvasSize,
             r32 TileSize, r32 Z, v4 Color)
{
    for(r32 Y = HalfCanvasSize;
        Y > -HalfCanvasSize;
        Y -= TileSize)
    {
        PushLine(RenderGroup, Transform, V3(-HalfCanvasSize, Y, Z), V3(HalfCanvasSize, Y, Z), Color);
    }

    for(r32 X = HalfCanvasSize;
        X > -HalfCanvasSize;
        X -= TileSize)
    {
        PushLine(RenderGroup, Transform, V3(X, HalfCanvasSize, Z), V3(X, -HalfCanvasSize, Z), Color);
    }
}

inline v2
CalculateAlingment(v2 Point, u32 BitmapWidth, u32 BitmapHeight)
{
    v2 Result = V2(Point.x * (1.0f / (r32)(BitmapWidth - 1)),
                   Point.y * (1.0f / (r32)(BitmapHeight - 1)));

    return(Result);
}

inline void
DrawCanvasOutline(render_group *RenderGroup, object_transform *Transform, v2 CanvasDim, v2 Offset)
{
    PushRect(RenderGroup, Transform, V3(Offset, 4.0f), CanvasDim + V2(4.0f, 4.0f),
             V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1.0f));

    PushRect(RenderGroup, Transform, V3(Offset, 3.0f), CanvasDim + V2(8.0f, 8.0f),
             V4(0.403921568627f, 0.254901960784f, 0.172549019608f, 1.0f));

    PushRect(RenderGroup, Transform, V3(Offset, 2.0f), CanvasDim + V2(12.0f, 12.0f),
             V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1.0f));

    PushRect(RenderGroup, Transform, V3(Offset, 1.0f), CanvasDim + V2(16.0f, 16.0f),
             V4(0.725490196078f, 0.478431372549f, 0.341176470588f, 1.0f));

    PushRect(RenderGroup, Transform, V3(Offset, 0.0f), CanvasDim + V2(20.0f, 20.0f),
             V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1.0f));
}

inline void
ClearSpriteSheetSprites(platform_texture_op_queue *TextureOpQueue, spritesheet_mode *SpriteSheetMode)
{
    for(u32 SpriteIndex = 0;
        SpriteIndex < ArrayCount(SpriteSheetMode->Sprites);
        ++SpriteIndex)
    {
        loaded_bitmap *Sprite = SpriteSheetMode->Sprites + SpriteIndex;
        DeallocateBitmap(TextureOpQueue, Sprite);
    }
                                
    ZeroArray(ArrayCount(SpriteSheetMode->Sprites), SpriteSheetMode->Sprites);
}

internal void
ClearStoredAssetData(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue,
                     kesa_type Type, audio_state *AudioState = 0)
{
    switch(Type)
    {
        case KESA_None:
        {
        } break;

        case KESA_Bitmap:
        {
            // NOTE(paul): Clear Bitmap Mode data
            bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
            DeallocateBitmap(TextureOpQueue, &BitmapMode->Bitmap);
        } break;

        case KESA_SpriteSheet:
        {
            // NOTE(paul): Clear SpriteSheet Mode data
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;

            DeallocateBitmap(TextureOpQueue, &SpriteSheetMode->SpriteSheetBitmap);
            ClearSpriteSheetSprites(TextureOpQueue, SpriteSheetMode);
        } break;

        case KESA_Tileset:
        {
            ClearTilesetView(TextureOpQueue, &AssetsMode->TilesetMode);
        } break;

        case KESA_Sound:
        {
            // NOTE(paul): Clear Sound Mode data 
            sound_mode *SoundMode = &AssetsMode->SoundMode;
            Assert(AudioState);
            MuteAndTerminateSound(AudioState, SoundMode->PlayingSound, 0.0f);
            Platform.DeallocateMemory(SoundMode->FreeSound);
        } break;
        
        case KESA_Text:
        {
            // NOTE(paul): Clear Text Mode Data
            text_mode *TextMode = &AssetsMode->TextMode;
            Platform.DeallocateMemory(TextMode->Text.String);
        } break;
        
        case KESA_File:
        {
            // NOTE(paul): Clear File Mode Data
            binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
            Platform.DeallocateMemory(FileMode->FileData);
        } break;
        
        case KESA_SSWM:
        {
            // NOTE(paul): Clear File Mode Data
            sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
            Platform.DeallocateMemory(SSWMMode->FileData);
        } break;

        case KESA_Map:
        {
            Clear(&AssetsMode->MapMode.Arena);
        } break;
    }

    u32 ModeSizes[] =
        {
            sizeof(bitmap_mode), sizeof(spritesheet_mode), sizeof(tileset_mode),
            sizeof(sound_mode), sizeof(text_mode), sizeof(binary_file_mode),
            sizeof(sswm_mode), sizeof(map_mode), sizeof(cut_mode),
        };

    u32 LargestModeSize = 0;
    for(u32 ModeIndex = 0;
        ModeIndex < ArrayCount(ModeSizes);
        ++ModeIndex)
    {
        if(ModeSizes[ModeIndex] > LargestModeSize)
        {
            LargestModeSize = ModeSizes[ModeIndex];
        }
    }

    ZeroSize(LargestModeSize, &AssetsMode->BitmapMode);
}

inline void
ClearEditModeData(editor_state *EditorState, editor_mode_assets *AssetsMode,
                  platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    kesa_type Type = Asset->Type;

    RemoveAction(AssetsMode, AM_RemoveAssetTag); 
    RemoveAction(AssetsMode, AM_AddAssetTag); 

    AssetsMode->LastTagIndex = 0;
    AssetsMode->CurrentTagIndex = 0;
    AssetsMode->CurrentTagValue = 0;
    AssetsMode->CurrentStoredTagIndex = 0;

    AssetsMode->FileIndex = 0;
    AssetsMode->LastFileIndex = 0;
    AssetsMode->SubFileIndex = 0;
    AssetsMode->LastSubFileIndex = 0;
    AssetsMode->PixelPosition = {};
    AssetsMode->Time = 0.0f;

    ClearStoredAssetData(AssetsMode, TextureOpQueue, Type, &EditorState->AudioState);
}

inline void
LoadStoredAssetData(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *StoredAsset,
                    b32 RememberEditMode = false)
{
    TIMED_FUNCTION();

    if(RememberEditMode)
    {
        AssetsMode->EditMode = AssetsEditModeFromStoredType(StoredAsset->Type);
    }

    switch(StoredAsset->Type)
    {
        case KESA_None:
        {
        } break;
        
        case KESA_Bitmap:
        {
            bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
            kesa_bitmap *StoredBitmap = &StoredAsset->Bitmap;
    
            BitmapMode->Bitmap = LoadImageFile(StoredAsset->SourceFileName, PlatformFileType_BMP, 0);
            if(BitmapMode->Bitmap.Memory)
            {
                loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
                AllocateBitmap(TextureOpQueue, Bitmap);
            }
        } break;

        case KESA_SpriteSheet:
        {
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
            kesa_spritesheet *SpriteSheet = &StoredAsset->SpriteSheet;

            SpriteSheetMode->SpriteSheetBitmap = LoadImageFile(StoredAsset->SourceFileName, PlatformFileType_SSBMP, 0);

            loaded_bitmap *Bitmap = &SpriteSheetMode->SpriteSheetBitmap;
            AllocateBitmap(TextureOpQueue, Bitmap);
            CutSpriteSheet(TextureOpQueue, SpriteSheetMode, SpriteSheet);
        } break;

        case KESA_Tileset:
        {
            LoadTilesetView(TextureOpQueue, &AssetsMode->TilesetMode, StoredAsset);
        } break;

        case KESA_Sound:
        {
            sound_mode *SoundMode = &AssetsMode->SoundMode;
            kesa_sound *StoredSound = &StoredAsset->Sound;
            SoundMode->Sound = LoadWAV(StoredAsset->SourceFileName, StoredSound->FirstSampleIndex, 0,
                                       &SoundMode->FreeSound, 0);
        } break;

        case KESA_Text:
        {
            text_mode *TextMode = &AssetsMode->TextMode;
            TextMode->Text = LoadText(StoredAsset->SourceFileName, 0);
        } break;

        case KESA_File:
        {
            binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
            kesa_binary_file *StoredFile = &StoredAsset->File;
            read_file_result ReadResult =
                Platform.ReadEntireFile(StoredAsset->SourceFileName, PlatformFileType_BIN, 0, 0);    
            FileMode->FileData = (u8 *)ReadResult.Contents;
            StoredFile->FileSize = ReadResult.Size;
        } break;

        case KESA_SSWM:
        {
            sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
            kesa_sswm_file *StoredFile = &StoredAsset->SSWM;
            read_file_result ReadResult =
                Platform.ReadEntireFile(StoredAsset->SourceFileName, PlatformFileType_KEWM, 0, 0);    
            SSWMMode->FileData = (u8 *)ReadResult.Contents;
            StoredFile->FileSize = ReadResult.Size;
        } break;

        case KESA_Map:
        {
            map_mode *MapMode = &AssetsMode->MapMode;
            MapMode->File = ReadMapFile(StoredAsset->SourceFileName, &MapMode->Arena);
            StoredAsset->Map.Width = MapMode->File.Width;
            StoredAsset->Map.Height = MapMode->File.Height;
            StoredAsset->Map.LayerCount = MapMode->File.LayerCount;
            StoredAsset->Map.SetCount = MapMode->File.SetCount;
        } break;

        InvalidDefaultCase;
    }
}

// NOTE(pvlso): The edit mode's picked file (FileIndex) becomes the current asset and is loaded. Every file is an
// asset already; an empty folder gives NoAsset, which loads nothing.
internal kesa_asset *
SelectEditedAsset(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue)
{
    kesa_type Type = KESAFromEditMode(AssetsMode->EditMode);
    kesa_asset *Asset = 0;
    if(AssetsMode->FileIndex < AssetsMode->SourceFileCounts[Type])
    {
        Asset = FindListedAsset(AssetsMode->StoredAssets, AssetsMode->StoredHeader.AssetCount, Type,
                                AssetsMode->SourceFiles[Type][AssetsMode->FileIndex]);
    }
    if(!Asset)
    {
        AssetsMode->NoAsset = {};
        AssetsMode->NoAsset.Type = Type;
        Asset = &AssetsMode->NoAsset;
    }
    AssetsMode->CurrentAsset = Asset;

    if(Asset->SourceFileName[0])
    {
        LoadStoredAssetData(AssetsMode, TextureOpQueue, Asset);
        if(Type == KESA_Text)
        {
            text_mode *TextMode = &AssetsMode->TextMode;
            TextMode->EditBufferMaxLength = sizeof(TextMode->EditBuffer);
            TextMode->EditBufferLength = TextMode->Text.String ? StringLength(TextMode->Text.String) : 0;
            if(TextMode->EditBufferLength >= (s32)TextMode->EditBufferMaxLength)
            {
                TextMode->EditBufferLength = TextMode->EditBufferMaxLength - 1;
            }
            Copy(TextMode->EditBufferLength, TextMode->Text.String, TextMode->EditBuffer);
        }
    }

    return(Asset);
}

inline void
UpdateAlignmentCursor(render_group *RenderGroup, object_transform *Flat, engine_input *Input,
                      v2 MouseP, rectangle2 CanvasRect, rectangle2 BitmapRect,
                      u32 BitmapPixelWidth, u32 BitmapPixelHeight, r32 Scale, v2 *Result)
{
    if(IsInRectangle(CanvasRect, MouseP))
    {
        v2 P = (MouseP - BitmapRect.Min - 0.5f*V2(Scale, Scale)) * (1.0f / Scale);
        s32 X = RoundReal32ToInt32(P.x);
        s32 Y = RoundReal32ToInt32(P.y);

        v2 CanvasCenter = GetCenter(BitmapRect);
        v2 PointP = BitmapRect.Min + Scale*V2(X, Y);
        PushRect(RenderGroup, Flat, V3(PointP + 0.5f*V2(Scale, Scale), 8.0f), V2(Scale, Scale));

        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
        {
            *Result = CalculateAlingment(V2(X, Y), BitmapPixelWidth, BitmapPixelHeight);
        }
    }
}

internal void
UpdateAndRenderBitmapEditMode(editor_mode_assets *AssetsMode, engine_input *Input, platform_texture_op_queue *TextureOpQueue,
                              render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                              rectangle2 CanvasRect, r32 TileDim, kesa_asset *Asset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    v2 CanvasCenter = GetCenter(CanvasRect);
    r32 CanvasSize = CanvasDim.x;
    r32 HalfCanvasSize = 0.5f*CanvasSize;
    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, CanvasCenter);

    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Bitmap);
        Asset = SelectEditedAsset(AssetsMode, TextureOpQueue);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
    kesa_bitmap *StoredBitmap = &Asset->Bitmap;

    loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
    PushRect(RenderGroup, Flat, V3(0, 0, 5.0f), CanvasDim, V4(0.25f, 0.25f, 0.25f, 1.0f));
                
    r32 Scale = CalculateBitmapScaleForSquareCanvas(CanvasSize, Bitmap->Width, Bitmap->Height);
    r32 TileSize = Scale*TileDim;
    DrawTileGrid(RenderGroup, Flat, HalfCanvasSize, TileSize, 6.0f, V4(0, 1, 0, 1));

    r32 BitmapHeight = Scale*Bitmap->Height;
    r32 BitmapWidth = Bitmap->WidthOverHeight*BitmapHeight;
    rectangle2 BitmapRect = RectCenterDim(V2(0, 0), V2(BitmapWidth, BitmapHeight));

    UpdateAlignmentCursor(RenderGroup, Flat, Input, MouseP, CanvasRect, BitmapRect,
                          Bitmap->Width, Bitmap->Height, Scale, &StoredBitmap->AlignPercentage);

    PushBitmap(RenderGroup, Flat, Bitmap, BitmapHeight, V3(0, 0, 7.0f));
    PushRectOutline(RenderGroup, Flat, BitmapRect, 7.0f, V4(0, 0, 1, 1), 1.0f);
}

inline void
DrawSpriteOutlines(render_group *RenderGroup, object_transform *Flat, r32 Scale,
                   r32 SpriteWidthOverHeight, u32 SpritePixelHeight, u32 SpriteCount,
                   r32 SpriteSheetWidth)
{
    r32 SpriteHeight = Scale*SpritePixelHeight;
    r32 SpriteWidth = SpriteHeight*SpriteWidthOverHeight;
    v2 Dim = V2(SpriteWidth, SpriteHeight);
    v2 Offset = V2(-0.5f*SpriteSheetWidth + 0.5f*Dim.x, 0.0f);
    for(u32 SpriteIndex = 0;
        SpriteIndex < SpriteCount;
        ++SpriteIndex)
    {
        PushRectOutline(RenderGroup, Flat, V3(Offset, 10.0f), Dim, V4(1, 0, 0, 1), 1.0f);
        Offset.x += Dim.x;
    }
}

internal void
UpdateAndRenderSpriteSheetEditMode(editor_mode_assets *AssetsMode, engine_input *Input, platform_texture_op_queue *TextureOpQueue,
                                   render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                                   rectangle2 CanvasRect, r32 TileDim, kesa_asset *CurrentAsset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    v2 CanvasCenter = GetCenter(CanvasRect);
    r32 CanvasSize = CanvasDim.x;
    r32 HalfCanvasSize = 0.5f*CanvasSize;
    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, CanvasCenter);

    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_SpriteSheet);
        CurrentAsset = SelectEditedAsset(AssetsMode, TextureOpQueue);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
        SpriteSheetMode->ShowAnimated = false;
    }
    kesa_spritesheet *SpriteSheet = &CurrentAsset->SpriteSheet;

    loaded_bitmap *SpriteSheetBitmap = &SpriteSheetMode->SpriteSheetBitmap;
    if(SpriteSheetMode->CutSpriteSheet)
    {
        if(SpriteSheet->SpriteWidth >= 16)
        {
            if((SpriteSheetBitmap->Width % SpriteSheet->SpriteWidth) == 0)
            {
                ClearSpriteSheetSprites(TextureOpQueue, SpriteSheetMode);

                SpriteSheet->SpriteCount = (SpriteSheetBitmap->Width / SpriteSheet->SpriteWidth);
                CutSpriteSheet(TextureOpQueue, SpriteSheetMode, SpriteSheet);
            }
        }

        SpriteSheetMode->CutSpriteSheet = false;
    }
                
    PushRect(RenderGroup, Flat, V3(0, 0, 5.0f), CanvasDim, V4(0.25f, 0.25f, 0.25f, 1.0f));

    if(SpriteSheetMode->ShowAnimated)
    {
        if(SpriteSheet->SpriteCount)
        {
            u32 SpriteIndex = (FloorReal32ToInt32(AssetsMode->Time*SpriteSheet->SpriteCount) %
                               SpriteSheet->SpriteCount);

            loaded_bitmap *SpriteBitmap = SpriteSheetMode->Sprites + SpriteIndex;
                
            r32 Scale = CalculateBitmapScaleForSquareCanvas(CanvasSize, SpriteBitmap->Width, SpriteBitmap->Height);
            r32 TileSize = Scale*TileDim;
            DrawTileGrid(RenderGroup, Flat, HalfCanvasSize, TileSize, 6.0f, V4(0, 1, 0, 1));

            if(SpriteBitmap)
            {
                r32 SpriteHeight = Scale*SpriteBitmap->Height;
                r32 SpriteWidth = SpriteBitmap->WidthOverHeight*SpriteHeight;
                rectangle2 SpriteBitmapRect = RectCenterDim(V2(0, 0), V2(SpriteWidth, SpriteHeight));
                    
                PushBitmap(RenderGroup, Flat, SpriteBitmap, SpriteHeight, V3(0, 0, 7.0f));
                PushRectOutline(RenderGroup, Flat, SpriteBitmapRect, 7.0f, V4(0, 0, 1, 1), 1.0f);

                UpdateAlignmentCursor(RenderGroup, Flat, Input, MouseP, CanvasRect, SpriteBitmapRect,
                                      SpriteBitmap->Width, SpriteBitmap->Height, Scale,
                                      &SpriteSheet->SpriteAlignPercentage);
            }
        }
    }
    else
    {
        r32 Scale = CalculateBitmapScaleForSquareCanvas(CanvasSize, SpriteSheetBitmap->Width,
                                             SpriteSheetBitmap->Height);
        r32 TileSize = Scale*TileDim;
        DrawTileGrid(RenderGroup, Flat, HalfCanvasSize, TileSize, 6.0f, V4(0, 1, 0, 1));

        r32 Height = Scale*SpriteSheetBitmap->Height;
        r32 Width = SpriteSheetBitmap->WidthOverHeight*Height;
        rectangle2 BitmapRect = RectCenterDim(V2(0, 0), V2(Width, Height));
                    
        PushBitmap(RenderGroup, Flat, SpriteSheetBitmap, Height, V3(0, 0, 7.0f));
        PushRectOutline(RenderGroup, Flat, BitmapRect, 7.0f, V4(0, 0, 1, 1), 1.0f);

        DrawSpriteOutlines(RenderGroup, Flat, Scale, SpriteSheetMode->Sprites[0].WidthOverHeight,
                           SpriteSheet->SpriteHeight, SpriteSheet->SpriteCount, Width);
    }
}

// NOTE(pvlso): A new set's file: the name kept to letters, digits, '_' and '-'. The tile set files are listed again
// and the new one picked, which loads it.
// NOTE(pvlso): Name with only the characters a file name can safely have (letters, digits, _ and -), its length
internal u32
CleanFileName(char *Name, char *Dest, u32 DestSize)
{
    u32 Length = 0;
    for(char *C = Name; *C && (Length + 1 < DestSize); ++C)
    {
        if(((*C >= 'a') && (*C <= 'z')) || ((*C >= 'A') && (*C <= 'Z')) || IsDigit(*C) || (*C == '_') || (*C == '-'))
        {
            Dest[Length++] = *C;
        }
    }
    Dest[Length] = 0;

    return(Length);
}

internal void
CreateTilesetFile(editor_mode_assets *AssetsMode, char *Name)
{
    char Clean[60];
    u32 Length = CleanFileName(Name, Clean, sizeof(Clean));
    char FileName[80];
    FormatString(sizeof(FileName), FileName, "%s.txt", Clean);
    if(!Length)
    {
        LOG_ERROR("A new tile set needs a name");
    }
    else if(Platform.FileExists(FileName, PlatformFileType_TSBMP))
    {
        LOG_ERROR("Tile set %s already exists", FileName);
    }
    else
    {
        char *Text = "; A curated tile set (Kaban engine). Format: engine_tileset_file.h\nsize 32\n";
        Platform.WriteEntireFile(FileName, PlatformFileType_TSBMP, (u8 *)Text, StringLength(Text));

        RescanAssets(AssetsMode);
        for(u32 Index = 0; Index < AssetsMode->SourceFileCounts[KESA_Tileset]; ++Index)
        {
            if(StringsAreEqual(AssetsMode->SourceFiles[KESA_Tileset][Index], FileName)) {AssetsMode->FileIndex = Index;}
        }
        AssetsMode->LastFileIndex = U32Maximum;
        LOG("Made the tile set %s", FileName);
    }
}

// NOTE(pvlso): Editing a curated tile set (editor_tileset_edit.cpp): the sheet on the left half of the canvas, the
// set's grid on the right, the panel's requests, undo and redo
internal void
UpdateAndRenderTilesetEditMode(editor_mode_assets *AssetsMode, engine_input *Input, platform_texture_op_queue *TextureOpQueue,
                               render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                               rectangle2 CanvasRect, r32 TileDim, kesa_asset *Asset, nk_context *Nk)
{
    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
    if(TilesetMode->CreateRequested)
    {
        TilesetMode->CreateRequested = false;
        CreateTilesetFile(AssetsMode, TilesetMode->NewSetName);
    }

    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Tileset);
        Asset = SelectEditedAsset(AssetsMode, TextureOpQueue);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }

    tileset_file *File = &TilesetMode->File;
    if(!TilesetMode->Editing && TilesetMode->Loaded)
    {
        TilesetMode->Editing = true;
        TilesetMode->SheetZoom = 24.0f;
        TilesetMode->SetZoom = 32.0f;
        TilesetMode->SelectedTile = -1;
        TilesetMode->ImageCount = Platform.ListImagesUnderData(0, 0);
        TilesetMode->Images = PushArray(&TilesetMode->Arena, Maximum(1, TilesetMode->ImageCount), char *);
        Platform.ListImagesUnderData(TilesetMode->Images, &TilesetMode->Arena);
        TilesetMode->LastImageIndex = U32Maximum;
        for(u32 Index = 0; File->SheetCount && (Index < TilesetMode->ImageCount); ++Index)
        {
            // NOTE(pvlso): The set's first sheet opens with it
            if(StringsAreEqual(TilesetMode->Images[Index], File->Sheets[0])) {TilesetMode->ImageIndex = Index;}
        }
        RecordTilesetState(TilesetMode);
    }
    if(!TilesetMode->Editing)
    {
        return;
    }

    if(TilesetMode->ImageCount && (TilesetMode->ImageIndex != TilesetMode->LastImageIndex))
    {
        OpenSheetImage(TextureOpQueue, TilesetMode, TilesetMode->Images[TilesetMode->ImageIndex]);
        TilesetMode->LastImageIndex = TilesetMode->ImageIndex;
    }

    b32 Changed = false;
    if(TilesetMode->FramesRequested)
    {
        TilesetMode->FramesRequested = false;
        if(TilesetMode->HasSelection && TilesetMode->SheetBitmap.Memory && (TilesetMode->SelectedTile >= 0))
        {
            SetFramesFromSelection(TextureOpQueue, TilesetMode);
            Changed = true;
        }
    }
    if(TilesetMode->RemoveRequested)
    {
        TilesetMode->RemoveRequested = false;
        if(TilesetMode->SelectedTile >= 0)
        {
            RemoveTile(TextureOpQueue, TilesetMode, TilesetMode->SelectedTile);
            RecountTilesetGrid(File);
            Changed = true;
        }
    }
    if(TilesetMode->TileChanged)
    {
        TilesetMode->TileChanged = false;
        Changed = true;
    }

    if(!TilesetMode->TypingName)
    {
        if(nk_input_is_key_pressed(&Nk->input, NK_KEY_TEXT_UNDO) && (TilesetMode->HistoryAt > 0))
        {
            RestoreTilesetState(TextureOpQueue, TilesetMode, TilesetMode->HistoryAt - 1);
        }
        else if(nk_input_is_key_pressed(&Nk->input, NK_KEY_TEXT_REDO) &&
                (TilesetMode->HistoryAt + 1 < TilesetMode->HistoryCount))
        {
            RestoreTilesetState(TextureOpQueue, TilesetMode, TilesetMode->HistoryAt + 1);
        }
    }

    r32 Wheel = Nk->input.mouse.scroll_delta.y;
    v2 Half = 0.5f*GetDim(CanvasRect);
    rectangle2 SheetRect = RectMinMax(V2(-Half.x, -Half.y), V2(-4.0f, Half.y));
    rectangle2 SetRect = RectMinMax(V2(4.0f, -Half.y), V2(Half.x, Half.y));
    UpdateAndRenderSheetView(TilesetMode, Input, RenderGroup, Flat, MouseP, SheetRect, Wheel);
    Changed |= UpdateAndRenderSetView(TextureOpQueue, TilesetMode, Input, RenderGroup, Flat, MouseP, SetRect, Wheel);

    if(Changed)
    {
        RecordTilesetState(TilesetMode);
        TilesetMode->Dirty = true;
    }

    if(TilesetMode->SaveRequested)
    {
        TilesetMode->SaveRequested = false;
        if(WriteTilesetFile(Asset->SourceFileName, File, &TilesetMode->FlagNames, &TilesetMode->Arena))
        {
            TilesetMode->Dirty = false;
            Asset->Tileset.TileCount = File->TileCount;
            Asset->Tileset.GridWidth = File->GridWidth;
            Asset->Tileset.GridHeight = File->GridHeight;
            LOG("Saved the tile set %s: %u tiles", Asset->SourceFileName, File->TileCount);
        }
    }
}

internal void
UpdateAndRenderSoundEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue,
                             audio_state *AudioState, kesa_asset *Asset)
{
    sound_mode *SoundMode = &AssetsMode->SoundMode;
    kesa_sound *StoredSound = &Asset->Sound;
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Sound, AudioState);
        Asset = SelectEditedAsset(AssetsMode, TextureOpQueue);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }

    if(SoundMode->PlaySound)
    {
        MuteAndTerminateSound(AudioState, SoundMode->PlayingSound, 0.0f);

        sound_id NullID = {};
        SoundMode->PlayingSound = PlaySound(AudioState, NullID, &SoundMode->Sound);

        SoundMode->PlaySound = false;
    }

    if(SoundMode->StopSound)
    {
        MuteAndTerminateSound(AudioState, SoundMode->PlayingSound, 0.0f);
        SoundMode->StopSound = false;
    }
}

internal void
UpdateAndRenderTextEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    text_mode *TextMode = &AssetsMode->TextMode;
    kesa_text *StoredText = &Asset->Text;
                
    if((AssetsMode->FileIndex != AssetsMode->LastFileIndex) || TextMode->Reload)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Text);
        Asset = SelectEditedAsset(AssetsMode, TextureOpQueue);

        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
        TextMode->Reload = false;
    }

    if(TextMode->EditTextFile && AssetsMode->SourceFileCounts[KESA_Text])
    {
#if EDITOR_INTERNAL
        char *FileName = AssetsMode->SourceFiles[KESA_Text][AssetsMode->FileIndex];
        char CommandLine[512];
        FormatString(ArrayCount(CommandLine), CommandLine,
                     "openwithnotepad.bat txts/%s", FileName);
        Platform.DEBUGExecuteSystemCommand(0, 0, CommandLine);
#endif

        TextMode->EditTextFile = false;
    }
}

internal void
UpdateAndRenderFileEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    kesa_binary_file *StoredFile = &Asset->File;
    binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
    
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_File);
        Asset = SelectEditedAsset(AssetsMode, TextureOpQueue);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
UpdateAndRenderSSWMEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    kesa_sswm_file *StoredFile = &Asset->SSWM;
    sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
    
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_SSWM);
        Asset = SelectEditedAsset(AssetsMode, TextureOpQueue);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
UpdateAndRenderMapAssetEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Map);
        Asset = SelectEditedAsset(AssetsMode, TextureOpQueue);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal b32
UpdateAndRenderAssetsMode(editor_state *EditorState, transient_state *TranState, engine_input *Input,
                          platform_texture_op_queue *TextureOpQueue)
{
    TIMED_FUNCTION();

    editor_mode_assets *AssetsMode = EditorState->AssetsMode;
    ui_state *UIState = &EditorState->UIState;

    b32 Result = false;//CheckForMetaInput(EditorState, TranState, Input);
    if(!Result)
    {
        render_group *RenderGroup = &UIState->RenderGroup;
        object_transform Flat = DefaultFlatTransform();
        v2 MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;
        if(!AssetsMode->AssetsInitialized)
        {
            // NOTE(pvlso): The tags first, assets.txt names them; the newest KET is the project's
            AssetsMode->StoredHeader.MagicValue = KESA_MAGIC_VALUE;
            AssetsMode->StoredHeader.TagsVersion = NewestTagsVersion(&AssetsMode->UtilityTempArena);
            if(!ReadTags(AssetsMode))
                Assert(ReadTags(AssetsMode));

            ConvertStoredAssets(EditorState, AssetsMode);
            editor_meta *EditorMeta = &EditorState->EditorMeta;
            AssetsMode->StoredHeader.Version = (((u32)EditorMeta->KESAVersion[0] << 24) |
                                                ((u32)EditorMeta->KESAVersion[1] << 16) |
                                                ((u32)EditorMeta->KESAVersion[2] <<  8) |
                                                ((u32)EditorMeta->KESAVersion[3]));
            ScanAssets(AssetsMode);
            AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
            AssetsMode->AssetsInitialized = true;
        }

        if(!IsAction(AssetsMode, AM_Exit))
        {
            if(AssetsMode->LastEditMode != AssetsMode->EditMode)
            {
                // NOTE(pvlso): The cut tool's data isn't a stored asset's, ClearEditModeData would only zero it
                if(AssetsMode->LastEditMode == EditMode_Cut)
                    ClearCutMode(TextureOpQueue, &AssetsMode->CutMode);
                if(AssetsMode->CurrentAsset)
                    ClearEditModeData(EditorState, AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                RemoveAction(AssetsMode, AM_EditStoredAsset);

                if(AssetsMode->EditMode == EditMode_Cut)
                {
                    AssetsMode->NoAsset = {};
                    AssetsMode->CurrentAsset = &AssetsMode->NoAsset;
                }
                else if(AssetsMode->EditMode != EditMode_None)
                {
                    SelectEditedAsset(AssetsMode, TextureOpQueue);
                }
                else
{
                    AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
                    LoadStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                }

                AssetsMode->LastEditMode = AssetsMode->EditMode;            
            }

            if(CheckRemoveAction(AssetsMode, AM_RemoveAssetTag))
            {
                RemoveCurrentAssetTag(AssetsMode, AssetsMode->CurrentAsset);
                LOG("Removed a tag from %s", AssetsMode->CurrentAsset->SourceFileName);
            }
        
            if(CheckRemoveAction(AssetsMode, AM_AddAssetTag))
            {
                AddTagToCurrentAsset(AssetsMode, AssetsMode->CurrentAsset);
                LOG("Added a tag to %s", AssetsMode->CurrentAsset->SourceFileName);
            }

            if(CheckRemoveAction(AssetsMode, AM_AddNewTag))
                UpdateEditorTags(AssetsMode, UpdateTagsOp_AddNew);
            else if(CheckRemoveAction(AssetsMode, AM_AddEditedTag))
                UpdateEditorTags(AssetsMode, UpdateTagsOp_Edit);
            else if(CheckRemoveAction(AssetsMode, AM_RemoveEditedTag))
                UpdateEditorTags(AssetsMode, UpdateTagsOp_Remove);

            if(CheckRemoveAction(AssetsMode, AM_WriteAssets))
            {
                SaveAssetList(AssetsMode);
            }

            r32 TileDim = 32.0f;
            r32 CanvasSize = 960.0f;
            r32 HalfCanvasSize = 0.5f*CanvasSize;
            v2 CanvasDim = V2(CanvasSize, CanvasSize);
            v2 CanvasHalfDim = 0.5f*V2(CanvasSize, CanvasSize);
            rectangle2 CanvasRect = RectCenterDim(V2(0, 0), CanvasDim);

            switch(AssetsMode->EditMode)
            {
                case EditMode_None:
                {
                    if(IsAction(AssetsMode, AM_EditStoredAsset))
                    {
                        AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;

                        ClearStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset->Type,
                                             &EditorState->AudioState);
                        LoadStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset, true);

                        // NOTE(pvlso): The edit mode's picker shows it, picking another file there edits that one
                        AssetsMode->FileIndex = SourceFileIndexOf(AssetsMode, AssetsMode->CurrentAsset);
                        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
                        AssetsMode->LastEditMode = AssetsMode->EditMode;
                        AssetsMode->LastShowStoredAssetIndex = AssetsMode->ShowStoredAssetIndex;
}
                    else if(AssetsMode->LastShowStoredAssetIndex != AssetsMode->ShowStoredAssetIndex)
                    {
                        kesa_asset *PreviousAsset = AssetsMode->StoredAssets + AssetsMode->LastShowStoredAssetIndex;
                        AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;

                        ClearStoredAssetData(AssetsMode, TextureOpQueue, PreviousAsset->Type,
                                             &EditorState->AudioState);
                        LoadStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);

                        AssetsMode->LastShowStoredAssetIndex = AssetsMode->ShowStoredAssetIndex;
                    }
                    
                    // NOTE(pvlso): A build scans first, so files added since are in it
                    b32 Build = CheckRemoveAction(AssetsMode, AM_WriteKEA);
                    if(CheckRemoveAction(AssetsMode, AM_Rescan) || Build)
                    {
                        ClearStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset->Type,
                                             &EditorState->AudioState);
                        RescanAssets(AssetsMode);
                        AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
                        LoadStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                        AssetsMode->LastShowStoredAssetIndex = AssetsMode->ShowStoredAssetIndex;
                    }

                    // NOTE(pvlso): The same build a Run or a changed folder starts (BuildProjectKEA)
                    if(Build)
                    {
                        WaitForKEABuild(EditorState);
                        BuildProjectKEA(EditorState, &TranState->TranArena, 0, 0, 0);
                        editor_meta *EditorMeta = &EditorState->EditorMeta;
                        AssetsMode->StoredHeader.Version = (((u32)EditorMeta->KESAVersion[0] << 24) |
                                                            ((u32)EditorMeta->KESAVersion[1] << 16) |
                                                            ((u32)EditorMeta->KESAVersion[2] <<  8) |
                                                            ((u32)EditorMeta->KESAVersion[3]));
                    }

                } break;

                case EditMode_Bitmap:
                {
                    UpdateAndRenderBitmapEditMode(AssetsMode, Input, TextureOpQueue, RenderGroup, &Flat, MouseP,
                                                  CanvasRect, TileDim, AssetsMode->CurrentAsset);
                } break;

                case EditMode_SpriteSheet:
                {
                    UpdateAndRenderSpriteSheetEditMode(AssetsMode, Input, TextureOpQueue, RenderGroup, &Flat, MouseP,
                                                       CanvasRect, TileDim, AssetsMode->CurrentAsset);
                } break;

                case EditMode_Tileset:
                {
                    UpdateAndRenderTilesetEditMode(AssetsMode, Input, TextureOpQueue, RenderGroup, &Flat, MouseP,
                                                   CanvasRect, TileDim, AssetsMode->CurrentAsset, UIState->Nk);
                } break;

                case EditMode_Sound:
                {
                    UpdateAndRenderSoundEditMode(AssetsMode, TextureOpQueue, &EditorState->AudioState,
                                                 AssetsMode->CurrentAsset);
                } break;

                case EditMode_Text:
                {
                    UpdateAndRenderTextEditMode(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                } break;

                case EditMode_File:
                {
                    UpdateAndRenderFileEditMode(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                } break;

                case EditMode_SSWM:
                {
                    UpdateAndRenderSSWMEditMode(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                } break;

                case EditMode_Map:
                {
                    UpdateAndRenderMapAssetEditMode(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                } break;

                case EditMode_Cut:
                {
                    UpdateAndRenderCutEditMode(AssetsMode, Input, TextureOpQueue, RenderGroup, &Flat, MouseP,
                                               CanvasRect, UIState->Nk);
                } break;
            
                InvalidDefaultCase;
            }

            DrawAssetsModeUI(AssetsMode, UIState);

            AssetsMode->Time += Input->dtForFrame;
        }
        else
        {
            ClearStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset->Type,
                                 &EditorState->AudioState);
            SaveAssetList(AssetsMode);
            Platform.DeallocateMemory(AssetsMode->StoredAssets);
            Platform.DeallocateMemory(AssetsMode->AssetNames);
            DeallocateAssetsModeTags(AssetsMode);

            PlayTitleScreen(EditorState, TranState);
        }
    }

    return(Result);
}
