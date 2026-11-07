/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): How a project's files become its assets. Nothing is added by hand: every file in the asset folders is
  an asset, found by scanning them, and the KEA is built from what the scan finds. data/assets.txt holds only what a
  file can't tell by itself, and only for the files that need it:

    ; a comment
    bmps/tree_small.png align 0.5 0.37
    spritesheets/fire.png frame 64 96 align 0.5 0.1
    wavs/rain.wav sound 0 0
    txts/world_grade.glsl tag Tag_Shader WorldGrade
    maps/hearthmoor.txt tag Tag_Map Hearthmoor

  align: where the foot is on a bitmap or on a sheet's frames, 0 to 1 from the bottom left (default the middle).
  frame: a sprite sheet's frame width and height in pixels; the frames stand side by side, as many as fit in its width
  (default square frames as high as the image). sound: the first sample and the chain (default 0 0). tag: a key and one
  of its values, both from the project's KET; any number of them. A name with spaces goes in quotes.

  The folders, in this order: bmps, spritesheets, tilesets, maps, wavs, txts, bins (entities are packed by the builder
  from data/entities on their own). An asset's GUID is made of its file name alone, so two files of one name in two
  folders would be one asset: the first folder's is kept, the other is left out with a warning.

  In memory the list is the old stored assets' layout (kesa_header, kesa_asset, the first one the null asset), which
  the builder and the editor modes already read. The KESA file itself retired: its last one is read once, to write
  assets.txt, when a project has no assets.txt yet.
*/

#define ASSET_LIST_FILE_NAME "data/assets.txt"

struct asset_list_folder
{
    kesa_type Type;
    platform_file_type FileType;
    char *Name;
};

global_variable asset_list_folder AssetListFolders[] =
{
    {KESA_Bitmap,      PlatformFileType_BMP,   "bmps"},
    {KESA_SpriteSheet, PlatformFileType_SSBMP, "spritesheets"},
    {KESA_Tileset,     PlatformFileType_TSBMP, "tilesets"},
    {KESA_Map,         PlatformFileType_Map,   "maps"},
    {KESA_Sound,       PlatformFileType_WAV,   "wavs"},
    {KESA_Text,        PlatformFileType_TXT,   "txts"},
    {KESA_File,        PlatformFileType_BIN,   "bins"},
};

internal char *
AssetListFolderName(kesa_type Type)
{
    char *Result = 0;
    for(u32 Index = 0; Index < ArrayCount(AssetListFolders); ++Index)
    {
        if(AssetListFolders[Index].Type == Type) {Result = AssetListFolders[Index].Name;}
    }

    return(Result);
}

// NOTE(pvlso): An image's size from its header, without reading its pixels: a PNG's IHDR or a BMP's info header
internal b32
ReadImageSize(char *FileName, platform_file_type Type, u32 *Width, u32 *Height)
{
    b32 Result = false;
    *Width = *Height = 0;

    u8 Head[26] = {};
    platform_file_handle Handle = Platform.OpenFile(FileName, Type, PlatformFileOp_Read);
    if(PlatformNoFileErrors(&Handle))
    {
        Platform.ReadDataFromFile(&Handle, 0, sizeof(Head), Head);
        if(PlatformNoFileErrors(&Handle))
        {
            if((Head[0] == 0x89) && (Head[1] == 'P') && (Head[2] == 'N') && (Head[3] == 'G'))
            {
                *Width = ((u32)Head[16] << 24) | ((u32)Head[17] << 16) | ((u32)Head[18] << 8) | (u32)Head[19];
                *Height = ((u32)Head[20] << 24) | ((u32)Head[21] << 16) | ((u32)Head[22] << 8) | (u32)Head[23];
                Result = true;
            }
            else if((Head[0] == 'B') && (Head[1] == 'M'))
            {
                s32 W = *(s32 *)(Head + 18);
                s32 H = *(s32 *)(Head + 22);
                *Width = (u32)((W < 0) ? -W : W);
                *Height = (u32)((H < 0) ? -H : H);
                Result = true;
            }
        }
        Platform.CloseFile(&Handle);
    }

    return(Result);
}

// NOTE(pvlso): A sheet's frames from its frame width: as many as fit in the image's width
internal void
SetSpriteSheetFrame(kesa_spritesheet *Sheet, u32 ImageWidth, u32 FrameWidth, u32 FrameHeight)
{
    Sheet->SpriteWidth = FrameWidth;
    Sheet->SpriteHeight = FrameHeight;
    Sheet->SpriteCount = FrameWidth ? (ImageWidth / FrameWidth) : 0;
}

// NOTE(pvlso): asset_tags_<a>.<b>.<c>.<d>.ket in data/kets, the highest version (0 when there's none)
internal u32
NewestTagsVersion(memory_arena *Arena)
{
    u32 Result = 0;
    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    u32 Count = Platform.ListFilesInDirectory(PlatformFileType_KET, 0, 0);
    char **Names = PushArray(TempMem.Arena, Maximum(1, Count), char *);
    Count = Platform.ListFilesInDirectory(PlatformFileType_KET, Names, TempMem.Arena);
    char *Prefix = "asset_tags_";
    umm PrefixLength = StringLength(Prefix);
    for(u32 Index = 0; Index < Count; ++Index)
    {
        char *At = Names[Index];
        if(!StringsAreEqual(PrefixLength, At, Prefix)) {continue;}
        At += PrefixLength;

        u32 Version = 0;
        b32 Valid = true;
        for(u32 Part = 0; Valid && (Part < 4); ++Part)
        {
            u32 Value = 0;
            Valid = IsDigit(*At);
            while(IsDigit(*At)) {Value = 10*Value + (*At++ - '0');}
            Valid = Valid && (Value <= 255) && (*At == '.'); // NOTE(pvlso): the last number is followed by .ket
            ++At;
            Version = (Version << 8) | Value;
        }
        if(Valid && (Version > Result)) {Result = Version;}
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}

struct asset_list_read
{
    kesa_asset *Assets;
    u32 *ImageWidths;
    u32 AssetCount; // NOTE(pvlso): with the null asset
    kea_tag_map *Tags;
    u32 TagCount;
};

internal kesa_asset *
FindListedAsset(kesa_asset *Assets, u32 AssetCount, kesa_type Type, char *FileName)
{
    kesa_asset *Result = 0;
    for(u32 Index = 1; !Result && (Index < AssetCount); ++Index)
    {
        if((Assets[Index].Type == Type) && StringsAreEqual(Assets[Index].SourceFileName, FileName))
        {
            Result = Assets + Index;
        }
    }

    return(Result);
}

internal
ENTITY_LINE_CALLBACK(ReadAssetListLine)
{
    asset_list_read *Read = (asset_list_read *)Context;
    char *Path = Tokens->Tokens[0];

    kesa_type Type = KESA_None;
    char *Name = 0;
    for(u32 Index = 0; Index < ArrayCount(AssetListFolders); ++Index)
    {
        char *Folder = AssetListFolders[Index].Name;
        umm Length = StringLength(Folder);
        if(StringsAreEqual(Length, Path, Folder) && (Path[Length] == '/'))
        {
            Type = AssetListFolders[Index].Type;
            Name = Path + Length + 1;
        }
    }

    kesa_asset *Asset = Name ? FindListedAsset(Read->Assets, Read->AssetCount, Type, Name) : 0;
    if(!Name)
    {
        LOG_ERROR("%s line %u: %s isn't in an asset folder (bmps/, spritesheets/, tilesets/, maps/, wavs/, txts/, "
                  "bins/)", FileName, LineNumber, Path);
    }
    else if(!Asset)
    {
        LOG_WARNING("%s line %u: there's no %s any more, the line is dropped at the next save", FileName, LineNumber,
                    Path);
    }
    else
    {
        u32 ImageWidth = Read->ImageWidths[Asset - Read->Assets];
        for(u32 At = 1; At < Tokens->Count;)
        {
            char *Key = Tokens->Tokens[At];
            u32 Left = Tokens->Count - At - 1;
            if(StringsAreEqual(Key, "align") && (Left >= 2) &&
               ((Type == KESA_Bitmap) || (Type == KESA_SpriteSheet)))
            {
                v2 Align = V2(R32FromTilesetToken(Tokens->Tokens[At + 1]), R32FromTilesetToken(Tokens->Tokens[At + 2]));
                if(Type == KESA_Bitmap) {Asset->Bitmap.AlignPercentage = Align;}
                else {Asset->SpriteSheet.SpriteAlignPercentage = Align;}
                At += 3;
            }
            else if(StringsAreEqual(Key, "frame") && (Left >= 2) && (Type == KESA_SpriteSheet))
            {
                u32 FrameWidth = (u32)R32FromTilesetToken(Tokens->Tokens[At + 1]);
                u32 FrameHeight = (u32)R32FromTilesetToken(Tokens->Tokens[At + 2]);
                SetSpriteSheetFrame(&Asset->SpriteSheet, ImageWidth, FrameWidth, FrameHeight);
                At += 3;
            }
            else if(StringsAreEqual(Key, "sound") && (Left >= 2) && (Type == KESA_Sound))
            {
                Asset->Sound.FirstSampleIndex = (u32)R32FromTilesetToken(Tokens->Tokens[At + 1]);
                Asset->Sound.Chain = (u32)R32FromTilesetToken(Tokens->Tokens[At + 2]);
                At += 3;
            }
            else if(StringsAreEqual(Key, "tag") && (Left >= 2))
            {
                char *TagKey = Tokens->Tokens[At + 1];
                char *TagValue = Tokens->Tokens[At + 2];
                At += 3;

                // NOTE(pvlso): Without the KET (the modes that only look at pictures) the tags are skipped
                if(!Read->Tags) {continue;}

                kea_tag_map *Tag = 0;
                for(u32 Index = 0; !Tag && (Index < Read->TagCount); ++Index)
                {
                    if(StringsAreEqual(Read->Tags[Index].Key, TagKey)) {Tag = Read->Tags + Index;}
                }
                s32 ValueIndex = -1;
                for(u32 Index = 0; Tag && (ValueIndex < 0) && (Index < Tag->ValueCount); ++Index)
                {
                    if(StringsAreEqual(Tag->Values[Index], TagValue)) {ValueIndex = (s32)Index;}
                }

                if(!Tag)
                {
                    LOG_ERROR("%s line %u: there's no tag %s in the KET, %s is left without it", FileName, LineNumber,
                              TagKey, Path);
                }
                else if(ValueIndex < 0)
                {
                    LOG_ERROR("%s line %u: the tag %s has no value %s, %s is left without it", FileName, LineNumber,
                              TagKey, TagValue, Path);
                }
                else if(Asset->TagCount >= MAX_NUMBER_OF_TAGS)
                {
                    LOG_ERROR("%s line %u: %s has more than %d tags", FileName, LineNumber, Path, MAX_NUMBER_OF_TAGS);
                }
                else
                {
                    kesa_tag *Dest = Asset->AssetTags + Asset->TagCount++;
                    Dest->TagGUID = Tag->GUID;
                    Dest->TagValueIndex = (u32)ValueIndex;
                }
            }
            else
            {
                LOG_ERROR("%s line %u: can't read \"%s\" for %s, the rest of the line is skipped", FileName,
                          LineNumber, Key, Path);
                break;
            }
        }
    }

    return(true);
}

// NOTE(pvlso): Every file in the asset folders as an asset, with what data/assets.txt says about it. Tags are read
// only with the KET's tags (Tags), the modes that only show pictures pass 0. The first asset is the null asset;
// Platform.DeallocateMemory frees the list.
internal kesa_asset *
ScanAssetList(kesa_header *Header, kea_tag_map *Tags, u32 TagCount, memory_arena *Arena)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    u32 FolderCounts[ArrayCount(AssetListFolders)] = {};
    char **FolderFiles[ArrayCount(AssetListFolders)] = {};
    u32 FileCount = 0;
    for(u32 Folder = 0; Folder < ArrayCount(AssetListFolders); ++Folder)
    {
        platform_file_type Type = AssetListFolders[Folder].FileType;
        u32 Count = Platform.ListFilesInDirectory(Type, 0, 0);
        FolderFiles[Folder] = PushArray(TempMem.Arena, Maximum(1, Count), char *);
        FolderCounts[Folder] = Platform.ListFilesInDirectory(Type, FolderFiles[Folder], TempMem.Arena);
        FileCount += FolderCounts[Folder];
    }

    umm Size = (FileCount + 1)*sizeof(kesa_asset);
    kesa_asset *Assets = (kesa_asset *)Platform.AllocateMemory(Size);
    ZeroSize(Size, Assets);
    u32 *ImageWidths = PushArray(TempMem.Arena, FileCount + 1, u32);
    u32 AssetCount = 1;
    for(u32 Folder = 0; Folder < ArrayCount(AssetListFolders); ++Folder)
    {
        asset_list_folder *Info = AssetListFolders + Folder;
        for(u32 FileIndex = 0; FileIndex < FolderCounts[Folder]; ++FileIndex)
        {
            char *Name = FolderFiles[Folder][FileIndex];
            if(StringLength(Name) >= sizeof(Assets->SourceFileName))
            {
                LOG_ERROR("%s/%s: the name is too long for an asset, it's left out", Info->Name, Name);
                continue;
            }

            u64 GUID = GUIDFromString(Name);
            kesa_asset *Same = 0;
            for(u32 Index = 1; !Same && (Index < AssetCount); ++Index)
            {
                if(Assets[Index].GUID == GUID) {Same = Assets + Index;}
            }
            if(Same)
            {
                LOG_WARNING("%s/%s has the name of %s/%s: two assets can't share a name, it's left out", Info->Name,
                            Name, AssetListFolderName(Same->Type), Same->SourceFileName);
                continue;
            }

            kesa_asset *Asset = Assets + AssetCount;
            Asset->GUID = GUID;
            Asset->Type = Info->Type;
            FormatString(sizeof(Asset->SourceFileName), Asset->SourceFileName, "%s", Name);
            if(Info->Type == KESA_Bitmap)
            {
                Asset->Bitmap.AlignPercentage = V2(0.5f, 0.5f);
            }
            else if(Info->Type == KESA_SpriteSheet)
            {
                u32 Width, Height;
                if(!ReadImageSize(Name, Info->FileType, &Width, &Height))
                {
                    LOG_ERROR("spritesheets/%s: can't read its size", Name);
                }
                ImageWidths[AssetCount] = Width;
                SetSpriteSheetFrame(&Asset->SpriteSheet, Width, Height, Height);
                Asset->SpriteSheet.SpriteAlignPercentage = V2(0.5f, 0.5f);
            }
            ++AssetCount;
        }
    }

    if(Platform.FileExists(ASSET_LIST_FILE_NAME, PlatformFileType_DataRoot))
    {
        asset_list_read Read = {Assets, ImageWidths, AssetCount, Tags, TagCount};
        ReadEntityLines(ASSET_LIST_FILE_NAME, PlatformFileType_DataRoot, TempMem.Arena, ReadAssetListLine, &Read);
    }
    EndTemporaryMemory(TempMem);

    ZeroStruct(*Header);
    Header->MagicValue = KESA_MAGIC_VALUE;
    Header->SizeOfStoredAsset = sizeof(kesa_asset);
    Header->AssetCount = AssetCount;
    Header->TagsVersion = NewestTagsVersion(Arena);

    return(Assets);
}

// NOTE(pvlso): A name as one word of a line, in quotes when it has a space
internal umm
FormatAssetListName(char *Dest, umm DestSize, char *Folder, char *Name)
{
    b32 Space = false;
    for(char *C = Name; *C; ++C) {if(IsWhitespace(*C)) {Space = true;}}
    char *Format = Space ? (char *)"\"%s/%s\"" : (char *)"%s/%s";
    FormatString(DestSize, Dest, Format, Folder, Name);

    return(StringLength(Dest));
}

// NOTE(pvlso): data/assets.txt from the list: a line for each asset that isn't all defaults. Written only when it
// changes; true when it was.
internal b32
WriteAssetList(kesa_asset *Assets, u32 AssetCount, kea_tag_map *Tags, u32 TagCount, memory_arena *Arena)
{
    b32 Result = false;
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    umm Capacity = 4096 + (umm)AssetCount*(1024 + MAX_NUMBER_OF_TAGS*2*KET_TAG_KEY_LENGTH);
    char *Text = (char *)Platform.AllocateMemory(Capacity);
    umm At = 0;
    FormatString(Capacity, Text, "; The project's assets (Kaban engine): every file in bmps/, spritesheets/, "
                 "tilesets/, maps/, wavs/,\n; txts/ and bins/ is packed into the KEA. A file has a line here only "
                 "for what isn't its default.\n; Format: engine_asset_list.cpp\n");
    At = StringLength(Text);

    u32 LineCount = 0;
    for(u32 Index = 1; Index < AssetCount; ++Index)
    {
        kesa_asset *Asset = Assets + Index;
        char *Folder = AssetListFolderName(Asset->Type);
        if(!Folder) {continue;}

        char Line[4096];
        umm LineAt = FormatAssetListName(Line, sizeof(Line), Folder, Asset->SourceFileName);
        umm NameEnd = LineAt;
        if(Asset->Type == KESA_Bitmap)
        {
            v2 Align = Asset->Bitmap.AlignPercentage;
            if((Align.x != 0.5f) || (Align.y != 0.5f))
            {
                FormatString(sizeof(Line) - LineAt, Line + LineAt, " align %.3f %.3f", Align.x, Align.y);
                LineAt += StringLength(Line + LineAt);
            }
        }
        else if(Asset->Type == KESA_SpriteSheet)
        {
            kesa_spritesheet *Sheet = &Asset->SpriteSheet;
            if(Sheet->SpriteWidth != Sheet->SpriteHeight)
            {
                FormatString(sizeof(Line) - LineAt, Line + LineAt, " frame %u %u", Sheet->SpriteWidth,
                             Sheet->SpriteHeight);
                LineAt += StringLength(Line + LineAt);
            }
            v2 Align = Sheet->SpriteAlignPercentage;
            if((Align.x != 0.5f) || (Align.y != 0.5f))
            {
                FormatString(sizeof(Line) - LineAt, Line + LineAt, " align %.3f %.3f", Align.x, Align.y);
                LineAt += StringLength(Line + LineAt);
            }
        }
        else if(Asset->Type == KESA_Sound)
        {
            if(Asset->Sound.FirstSampleIndex || Asset->Sound.Chain)
            {
                FormatString(sizeof(Line) - LineAt, Line + LineAt, " sound %u %u", Asset->Sound.FirstSampleIndex,
                             Asset->Sound.Chain);
                LineAt += StringLength(Line + LineAt);
            }
        }

        for(u32 TagIndex = 0; TagIndex < Asset->TagCount; ++TagIndex)
        {
            kesa_tag *Tag = Asset->AssetTags + TagIndex;
            kea_tag_map *Map = 0;
            for(u32 MapIndex = 0; !Map && (MapIndex < TagCount); ++MapIndex)
            {
                if(Tags[MapIndex].GUID == Tag->TagGUID) {Map = Tags + MapIndex;}
            }
            if(Map && (Tag->TagValueIndex < Map->ValueCount))
            {
                FormatString(sizeof(Line) - LineAt, Line + LineAt, " tag %s %s", Map->Key,
                             Map->Values[Tag->TagValueIndex]);
                LineAt += StringLength(Line + LineAt);
            }
            else
            {
                LOG_WARNING("%s/%s: a tag that isn't in the KET any more is dropped", Folder, Asset->SourceFileName);
            }
        }

        if((LineAt > NameEnd) && (At + LineAt + 2 < Capacity))
        {
            FormatString(Capacity - At, Text + At, "%s\n", Line);
            At += LineAt + 1;
            ++LineCount;
        }
    }

    read_file_result Old = {};
    if(Platform.FileExists(ASSET_LIST_FILE_NAME, PlatformFileType_DataRoot))
    {
        Old = Platform.ReadEntireFile(ASSET_LIST_FILE_NAME, PlatformFileType_DataRoot, TempMem.Arena, true);
    }
    if(!Old.Contents || !StringsAreEqual((char *)Old.Contents, Text))
    {
        if(Platform.WriteEntireFile(ASSET_LIST_FILE_NAME, PlatformFileType_DataRoot, (u8 *)Text, (u32)At))
        {
            LOG("Wrote %s: %u lines", ASSET_LIST_FILE_NAME, LineCount);
            Result = true;
        }
        else
        {
            LOG_ERROR("Could not write %s", ASSET_LIST_FILE_NAME);
        }
    }
    EndTemporaryMemory(TempMem);
    Platform.DeallocateMemory(Text);

    return(Result);
}
