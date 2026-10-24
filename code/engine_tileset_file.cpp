/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): Curated tile sets, the text file format: see engine_tileset_file.h

// NOTE(pvlso): One line's words; a word in quotes can hold spaces
struct tileset_line_tokens
{
    u32 Count;
    char Tokens[64][TILESET_SHEET_PATH_LENGTH];
};

internal void
TokenizeTilesetLine(char *Line, char *End, tileset_line_tokens *Tokens)
{
    Tokens->Count = 0;
    char *At = Line;
    while((At < End) && (Tokens->Count < ArrayCount(Tokens->Tokens)))
    {
        while((At < End) && IsWhitespace(*At)) {++At;}
        if((At >= End) || (*At == ';'))
        {
            break;
        }

        char *Dest = Tokens->Tokens[Tokens->Count];
        u32 Length = 0;
        if(*At == '"')
        {
            ++At;
            while((At < End) && (*At != '"') && (Length < TILESET_SHEET_PATH_LENGTH - 1)) {Dest[Length++] = *At++;}
            if(At < End) {++At;}
        }
        else
        {
            while((At < End) && !IsWhitespace(*At) && (Length < TILESET_SHEET_PATH_LENGTH - 1)) {Dest[Length++] = *At++;}
        }
        Dest[Length] = 0;
        ++Tokens->Count;
    }
}

internal b32
IsTilesetNumber(char *Token)
{
    b32 Result = (*Token != 0);
    for(char *C = Token; *C; ++C)
    {
        if(!IsDigit(*C) && (*C != '.') && !((C == Token) && (*C == '-'))) {Result = false;}
    }

    return(Result);
}

internal r32
R32FromTilesetToken(char *Token)
{
    r32 Result = 0.0f;
    r32 Sign = 1.0f;
    char *At = Token;
    if(*At == '-') {Sign = -1.0f; ++At;}
    while(IsDigit(*At)) {Result = 10.0f*Result + (r32)(*At++ - '0');}
    if(*At == '.')
    {
        ++At;
        r32 Scale = 0.1f;
        while(IsDigit(*At)) {Result += Scale*(r32)(*At++ - '0'); Scale *= 0.1f;}
    }

    return(Sign*Result);
}

internal b32
IsTilesetKeyword(char *Token)
{
    b32 Result = (StringsAreEqual(Token, "name") || StringsAreEqual(Token, "flags") ||
                  StringsAreEqual(Token, "frames") || StringsAreEqual(Token, "fps"));
    return(Result);
}

// NOTE(pvlso): data/txts/tile_flags.txt: one flag name per line (';' starts a comment), at most 32; the first is bit 0.
// A project without the file has no flags.
internal tile_flag_names
ReadTileFlagNames(memory_arena *Arena)
{
    tile_flag_names Result = {};
    if(Platform.FileExists("tile_flags.txt", PlatformFileType_TXT))
    {
        temporary_memory TempMem = BeginTemporaryMemory(Arena);
        read_file_result File = Platform.ReadEntireFile("tile_flags.txt", PlatformFileType_TXT, Arena, true);
        char *At = (char *)File.Contents;
        char *FileEnd = At ? At + StringLength(At) : 0; // NOTE(pvlso): a text file's Size counts its closing zero
        tileset_line_tokens *Tokens = PushStruct(Arena, tileset_line_tokens);
        while(At && (At < FileEnd))
        {
            char *LineEnd = At;
            while((LineEnd < FileEnd) && !IsEndOfLine(*LineEnd)) {++LineEnd;}
            TokenizeTilesetLine(At, LineEnd, Tokens);
            if(Tokens->Count)
            {
                if(Result.Count < TILESET_MAX_FLAGS)
                {
                    FormatString(TILESET_FLAG_NAME_LENGTH, Result.Names[Result.Count++], "%s", Tokens->Tokens[0]);
                }
                else
                {
                    LOG_ERROR("tile_flags.txt: more than %d flags, %s and the rest are left out", TILESET_MAX_FLAGS,
                              Tokens->Tokens[0]);
                }
            }
            At = LineEnd;
            while((At < FileEnd) && IsEndOfLine(*At)) {++At;}
        }
        EndTemporaryMemory(TempMem);
    }

    return(Result);
}

internal u32
TileFlagBit(tile_flag_names *FlagNames, char *Name)
{
    u32 Result = 0;
    for(u32 Index = 0; Index < FlagNames->Count; ++Index)
    {
        if(StringsAreEqual(FlagNames->Names[Index], Name)) {Result = (1u << Index);}
    }

    return(Result);
}

// NOTE(pvlso): Reads data/tilesets/<FileName>. Tiles goes in Arena; mistakes are logged with their line and skipped.
internal tileset_file
ReadTilesetFile(char *FileName, tile_flag_names *FlagNames, memory_arena *Arena)
{
    tileset_file Result = {};
    Result.TileSize = 32;
    Result.Tiles = PushArray(Arena, TILESET_MAX_TILES, tileset_file_tile);

    read_file_result File = Platform.ReadEntireFile(FileName, PlatformFileType_TSBMP, Arena, true);
    if(!File.Contents)
    {
        LOG_ERROR("Tile set %s: can't read the file", FileName);
        ++Result.ErrorCount;
        return(Result);
    }
    Result.Loaded = true;

    tileset_line_tokens *Tokens = PushStruct(Arena, tileset_line_tokens);
    char *At = (char *)File.Contents;
    char *FileEnd = At + StringLength(At); // NOTE(pvlso): a text file's Size counts its closing zero
    u32 LineNumber = 0;
    while(At < FileEnd)
    {
        char *LineEnd = At;
        while((LineEnd < FileEnd) && (*LineEnd != '\n')) {++LineEnd;}
        ++LineNumber;
        TokenizeTilesetLine(At, LineEnd, Tokens);
        At = (LineEnd < FileEnd) ? LineEnd + 1 : LineEnd;
        if(!Tokens->Count)
        {
            continue;
        }

        char *Key = Tokens->Tokens[0];
        if(StringsAreEqual(Key, "size") && (Tokens->Count >= 2))
        {
            Result.TileSize = S32FromZ(Tokens->Tokens[1]);
        }
        else if(StringsAreEqual(Key, "sheet") && (Tokens->Count >= 2))
        {
            if(Result.SheetCount < TILESET_MAX_SHEETS)
            {
                FormatString(TILESET_SHEET_PATH_LENGTH, Result.Sheets[Result.SheetCount++], "%s", Tokens->Tokens[1]);
            }
            else
            {
                LOG_ERROR("Tile set %s, line %d: more than %d sheets", FileName, LineNumber, TILESET_MAX_SHEETS);
                ++Result.ErrorCount;
            }
        }
        else if(StringsAreEqual(Key, "tile"))
        {
            b32 Valid = (Tokens->Count >= 6);
            for(u32 Index = 1; Valid && (Index < 6); ++Index)
            {
                Valid = IsTilesetNumber(Tokens->Tokens[Index]);
            }
            u32 Sheet = Valid ? S32FromZ(Tokens->Tokens[3]) : 0;
            if(!Valid || (Sheet >= Result.SheetCount))
            {
                LOG_ERROR("Tile set %s, line %d: a tile is \"tile <grid x> <grid y> <sheet> <cell x> <cell y> ...\" "
                          "with one of the sheets listed above it", FileName, LineNumber);
                ++Result.ErrorCount;
                continue;
            }
            if(Result.TileCount >= TILESET_MAX_TILES)
            {
                LOG_ERROR("Tile set %s, line %d: more than %d tiles", FileName, LineNumber, TILESET_MAX_TILES);
                ++Result.ErrorCount;
                continue;
            }

            tileset_file_tile *Tile = Result.Tiles + Result.TileCount++;
            *Tile = {};
            Tile->GridX = (u16)S32FromZ(Tokens->Tokens[1]);
            Tile->GridY = (u16)S32FromZ(Tokens->Tokens[2]);
            Tile->Frames[0].Sheet = (u16)Sheet;
            Tile->Frames[0].X = (u16)S32FromZ(Tokens->Tokens[4]);
            Tile->Frames[0].Y = (u16)S32FromZ(Tokens->Tokens[5]);
            Tile->FrameCount = 1;
            Result.GridWidth = Maximum(Result.GridWidth, (u32)Tile->GridX + 1);
            Result.GridHeight = Maximum(Result.GridHeight, (u32)Tile->GridY + 1);

            u32 Index = 6;
            while(Index < Tokens->Count)
            {
                char *Part = Tokens->Tokens[Index++];
                if(StringsAreEqual(Part, "name") && (Index < Tokens->Count))
                {
                    FormatString(KEA_TILE_NAME_LENGTH, Tile->Name, "%s", Tokens->Tokens[Index++]);
                }
                else if(StringsAreEqual(Part, "flags"))
                {
                    while((Index < Tokens->Count) && !IsTilesetKeyword(Tokens->Tokens[Index]))
                    {
                        u32 Bit = TileFlagBit(FlagNames, Tokens->Tokens[Index]);
                        if(!Bit)
                        {
                            LOG_ERROR("Tile set %s, line %d: no flag %s in tile_flags.txt", FileName, LineNumber,
                                      Tokens->Tokens[Index]);
                            ++Result.ErrorCount;
                        }
                        Tile->Flags |= Bit;
                        ++Index;
                    }
                }
                else if(StringsAreEqual(Part, "frames"))
                {
                    while(((Index + 2) < Tokens->Count) && IsTilesetNumber(Tokens->Tokens[Index]))
                    {
                        u32 FrameSheet = S32FromZ(Tokens->Tokens[Index]);
                        if((FrameSheet < Result.SheetCount) && (Tile->FrameCount < TILESET_MAX_TILE_FRAMES))
                        {
                            tileset_file_cell *Frame = Tile->Frames + Tile->FrameCount++;
                            Frame->Sheet = (u16)FrameSheet;
                            Frame->X = (u16)S32FromZ(Tokens->Tokens[Index + 1]);
                            Frame->Y = (u16)S32FromZ(Tokens->Tokens[Index + 2]);
                        }
                        else
                        {
                            LOG_ERROR("Tile set %s, line %d: a frame is \"<sheet> <cell x> <cell y>\", at most %d "
                                      "frames", FileName, LineNumber, TILESET_MAX_TILE_FRAMES);
                            ++Result.ErrorCount;
                        }
                        Index += 3;
                    }
                }
                else if(StringsAreEqual(Part, "fps") && (Index < Tokens->Count))
                {
                    Tile->FramesPerSecond = R32FromTilesetToken(Tokens->Tokens[Index++]);
                }
                else
                {
                    LOG_ERROR("Tile set %s, line %d: what is %s?", FileName, LineNumber, Part);
                    ++Result.ErrorCount;
                }
            }
            if((Tile->FrameCount > 1) && (Tile->FramesPerSecond <= 0.0f))
            {
                Tile->FramesPerSecond = 8.0f;
            }
        }
        else
        {
            LOG_ERROR("Tile set %s, line %d: a line is size, sheet or tile, not %s", FileName, LineNumber, Key);
            ++Result.ErrorCount;
        }
    }

    return(Result);
}

// NOTE(pvlso): Cuts the cell (X, Y in tiles from the sheet's top left) out of a sheet loaded bottom row first. An
// empty bitmap when the cell is outside the sheet.
internal loaded_bitmap
CutTilesetCell(loaded_bitmap *Sheet, u32 TileSize, u32 CellX, u32 CellY, memory_arena *Arena)
{
    loaded_bitmap Result = {};
    if(Sheet->Memory && ((CellX + 1)*TileSize <= (u32)Sheet->Width) && ((CellY + 1)*TileSize <= (u32)Sheet->Height))
    {
        Result.Width = TileSize;
        Result.Height = TileSize;
        Result.WidthOverHeight = 1.0f;
        Result.AlignPercentage = V2(0.5f, 0.5f);
        Result.Pitch = TileSize*BITMAP_BYTES_PER_PIXEL;
        Result.Memory = PushSize(Arena, Result.Height*Result.Pitch);

        u32 FirstRow = Sheet->Height - (CellY + 1)*TileSize;
        for(u32 Y = 0; Y < TileSize; ++Y)
        {
            u32 *Source = (u32 *)((u8 *)Sheet->Memory + (FirstRow + Y)*Sheet->Pitch) + CellX*TileSize;
            u32 *Dest = (u32 *)((u8 *)Result.Memory + Y*Result.Pitch);
            for(u32 X = 0; X < TileSize; ++X)
            {
                *Dest++ = *Source++;
            }
        }
    }

    return(Result);
}
