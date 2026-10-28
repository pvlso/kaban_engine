/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): Maps' text files: see engine_map_file.h

inline char *
SkipMapSpaces(char *At, char *End)
{
    while((At < End) && ((*At == ' ') || (*At == '\t') || (*At == '\r'))) {++At;}
    return(At);
}

inline char *
ReadMapWord(char *At, char *End, char *Dest, u32 DestCount)
{
    At = SkipMapSpaces(At, End);
    u32 Length = 0;
    if((At < End) && (*At == '"'))
    {
        ++At;
        while((At < End) && (*At != '"')) {if(Length < DestCount - 1) {Dest[Length++] = *At;} ++At;}
        if(At < End) {++At;}
    }
    else
    {
        while((At < End) && (*At != ' ') && (*At != '\t') && (*At != '\r'))
        {
            if(Length < DestCount - 1) {Dest[Length++] = *At;}
            ++At;
        }
    }
    Dest[Length] = 0;

    return(At);
}

inline u32
U32FromMapText(char **AtInit, char *End)
{
    char *At = *AtInit;
    u32 Result = 0;
    while((At < End) && IsDigit(*At)) {Result = 10*Result + (u32)(*At++ - '0');}
    *AtInit = At;

    return(Result);
}

// NOTE(pvlso): Reads data/maps/<FileName>; the layers' cells go in Arena. Mistakes are logged with their line.
internal map_file
ReadMapFile(char *FileName, memory_arena *Arena)
{
    map_file Result = {};
    read_file_result File = Platform.ReadEntireFile(FileName, PlatformFileType_Map, Arena, true);
    if(!File.Contents)
    {
        LOG_ERROR("Map %s: can't read the file", FileName);
        ++Result.ErrorCount;
        return(Result);
    }
    Result.Loaded = true;

    char *At = (char *)File.Contents;
    char *FileEnd = At + StringLength(At); // NOTE(pvlso): a text file's Size counts its closing zero

    // NOTE(pvlso): Room for as many entities as there are lines starting with "entity"
    u32 MaxEntities = 1;
    for(char *LineAt = At; LineAt < FileEnd;)
    {
        char *Word = SkipMapSpaces(LineAt, FileEnd);
        char *Entity = "entity";
        u32 Matched = 0;
        while((Word + Matched < FileEnd) && Entity[Matched] && (Word[Matched] == Entity[Matched])) {++Matched;}
        if(!Entity[Matched]) {++MaxEntities;}
        while((LineAt < FileEnd) && (*LineAt != '\n')) {++LineAt;}
        if(LineAt < FileEnd) {++LineAt;}
    }
    Result.Entities = PushArray(Arena, MaxEntities, map_file_entity);

    u32 LineNumber = 0;
    map_file_layer *Layer = 0;
    map_file_entity *Entity = 0;
    u32 Row = 0;
    char Word[256];
    while(At < FileEnd)
    {
        char *LineEnd = At;
        while((LineEnd < FileEnd) && (*LineEnd != '\n')) {++LineEnd;}
        ++LineNumber;
        char *Line = SkipMapSpaces(At, LineEnd);
        At = (LineEnd < FileEnd) ? LineEnd + 1 : LineEnd;
        if((Line >= LineEnd) || (*Line == ';'))
        {
            continue;
        }

        Line = ReadMapWord(Line, LineEnd, Word, sizeof(Word));
        if(StringsAreEqual(Word, "size"))
        {
            Line = SkipMapSpaces(Line, LineEnd);
            Result.Width = U32FromMapText(&Line, LineEnd);
            Line = SkipMapSpaces(Line, LineEnd);
            Result.Height = U32FromMapText(&Line, LineEnd);
            if(!Result.Width || !Result.Height || (Result.Width > MAP_FILE_MAX_SIDE) || (Result.Height > MAP_FILE_MAX_SIDE))
            {
                LOG_ERROR("Map %s, line %d: the size is 1 to %d cells a side", FileName, LineNumber, MAP_FILE_MAX_SIDE);
                ++Result.ErrorCount;
                Result.Loaded = false;
                break;
            }
        }
        else if(StringsAreEqual(Word, "set"))
        {
            if(Result.SetCount < MAP_FILE_MAX_SETS)
            {
                ReadMapWord(Line, LineEnd, Result.Sets[Result.SetCount++], MAP_FILE_SET_NAME_LENGTH);
            }
            else
            {
                LOG_ERROR("Map %s, line %d: more than %d tile sets", FileName, LineNumber, MAP_FILE_MAX_SETS);
                ++Result.ErrorCount;
            }
        }
        else if(StringsAreEqual(Word, "layer"))
        {
            if(!Result.Width)
            {
                LOG_ERROR("Map %s, line %d: the size comes before the layers", FileName, LineNumber);
                ++Result.ErrorCount;
                Result.Loaded = false;
                break;
            }
            if(Result.LayerCount >= MAP_FILE_MAX_LAYERS)
            {
                LOG_ERROR("Map %s, line %d: more than %d layers", FileName, LineNumber, MAP_FILE_MAX_LAYERS);
                ++Result.ErrorCount;
                Layer = 0;
                continue;
            }
            Layer = Result.Layers + Result.LayerCount++;
            Line = ReadMapWord(Line, LineEnd, Layer->Name, MAP_FILE_NAME_LENGTH);
            Line = SkipMapSpaces(Line, LineEnd);
            b32 Negative = ((Line < LineEnd) && (*Line == '-'));
            if(Negative) {++Line;}
            Layer->Z = (s32)U32FromMapText(&Line, LineEnd)*(Negative ? -1 : 1);
            ReadMapWord(Line, LineEnd, Word, sizeof(Word));
            Layer->Hidden = StringsAreEqual(Word, "hidden");
            umm Size = (umm)Result.Width*Result.Height*sizeof(map_cell);
            Layer->Cells = (map_cell *)PushSize(Arena, Size);
            ZeroSize(Size, Layer->Cells);
            Row = 0;
        }
        else if(StringsAreEqual(Word, "row"))
        {
            if(!Layer || (Row >= Result.Height))
            {
                if(Layer)
                {
                    LOG_ERROR("Map %s, line %d: more rows than the map's height", FileName, LineNumber);
                    ++Result.ErrorCount;
                }
                continue;
            }
            map_cell *Cells = Layer->Cells + Row*Result.Width;
            for(u32 X = 0; X < Result.Width; ++X)
            {
                Line = SkipMapSpaces(Line, LineEnd);
                if(Line >= LineEnd) {break;}
                if(*Line == '.')
                {
                    ++Line;
                    continue;
                }
                u32 Set = U32FromMapText(&Line, LineEnd);
                u32 TileX = 0, TileY = 0;
                b32 Valid = ((Line < LineEnd) && (*Line == ':'));
                if(Valid) {++Line; TileX = U32FromMapText(&Line, LineEnd);}
                Valid = Valid && (Line < LineEnd) && (*Line == ':');
                if(Valid) {++Line; TileY = U32FromMapText(&Line, LineEnd);}
                if(Valid && Set && (Set <= Result.SetCount))
                {
                    Cells[X].Set = (u16)Set;
                    Cells[X].X = (u16)TileX;
                    Cells[X].Y = (u16)TileY;
                }
                else
                {
                    LOG_ERROR("Map %s, line %d: cell %d is \".\" or set:x:y with one of the sets", FileName, LineNumber, X);
                    ++Result.ErrorCount;
                    while((Line < LineEnd) && (*Line != ' ') && (*Line != '\t')) {++Line;}
                }
            }
            ++Row;
        }
        else if(StringsAreEqual(Word, "entity"))
        {
            map_file_entity Read = {};
            char Numbers[2][32];
            char Definition[32];
            Line = ReadMapWord(Line, LineEnd, Word, sizeof(Word));
            b32 Valid = EntityGUIDFromHex(Word, &Read.GUID);
            Line = ReadMapWord(Line, LineEnd, Definition, sizeof(Definition));
            Valid = Valid && EntityGUIDFromHex(Definition, &Read.Definition);
            Line = ReadMapWord(Line, LineEnd, Numbers[0], sizeof(Numbers[0]));
            Line = ReadMapWord(Line, LineEnd, Numbers[1], sizeof(Numbers[1]));
            Valid = Valid && IsTilesetNumber(Numbers[0]) && IsTilesetNumber(Numbers[1]);
            ReadMapWord(Line, LineEnd, Word, sizeof(Word));
            Read.Flip = StringsAreEqual(Word, "flip");
            Entity = 0;
            if(Valid)
            {
                Read.P = V2(R32FromTilesetToken(Numbers[0]), R32FromTilesetToken(Numbers[1]));
                Entity = Result.Entities + Result.EntityCount++;
                *Entity = Read;
            }
            else
            {
                LOG_ERROR("Map %s, line %d: an entity is its GUID, its definition's GUID, x and y", FileName, LineNumber);
                ++Result.ErrorCount;
            }
        }
        else if(StringsAreEqual(Word, "override"))
        {
            map_file_override Read = {};
            Line = ReadMapWord(Line, LineEnd, Read.Block, sizeof(Read.Block));
            Line = ReadMapWord(Line, LineEnd, Read.Field, sizeof(Read.Field));
            ReadMapWord(Line, LineEnd, Read.Value, sizeof(Read.Value));
            if(!Entity)
            {
                LOG_ERROR("Map %s, line %d: an override goes under its entity", FileName, LineNumber);
                ++Result.ErrorCount;
            }
            else if(!Read.Field[0])
            {
                LOG_ERROR("Map %s, line %d: an override is a block, a field and a value", FileName, LineNumber);
                ++Result.ErrorCount;
            }
            else if(Entity->OverrideCount >= MAP_FILE_MAX_ENTITY_OVERRIDES)
            {
                LOG_ERROR("Map %s, line %d: more than %d overrides on an entity", FileName, LineNumber,
                          MAP_FILE_MAX_ENTITY_OVERRIDES);
                ++Result.ErrorCount;
            }
            else
            {
                Entity->Overrides[Entity->OverrideCount++] = Read;
            }
        }
        else
        {
            LOG_ERROR("Map %s, line %d: a line is size, set, layer, row, entity or override, not %s", FileName,
                      LineNumber, Word);
            ++Result.ErrorCount;
        }
    }

    return(Result);
}

// NOTE(pvlso): Writes data/maps/<FileName>. False when it can't be written.
internal b32
WriteMapFile(char *FileName, map_file *Map, memory_arena *Arena)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    umm Capacity = 4096 + (umm)Map->LayerCount*(Map->Height*(8 + Map->Width*16) + 128) +
        (umm)Map->EntityCount*(96 + MAP_FILE_MAX_ENTITY_OVERRIDES*(2*ENTITY_NAME_LENGTH + ENTITY_TEXT_LENGTH + 16));
    char *Text = (char *)PushSize(Arena, Capacity);
    umm At = 0;
#define MAP_OUT(...) At += FormatString(Capacity - At, Text + At, __VA_ARGS__)
    MAP_OUT("; A map (Kaban engine), written by the map editor. Format: engine_map_file.h\n");
    MAP_OUT("size %d %d\n", Map->Width, Map->Height);
    for(u32 SetIndex = 0; SetIndex < Map->SetCount; ++SetIndex)
    {
        MAP_OUT("set %s\n", Map->Sets[SetIndex]);
    }
    for(u32 LayerIndex = 0; LayerIndex < Map->LayerCount; ++LayerIndex)
    {
        map_file_layer *Layer = Map->Layers + LayerIndex;
        char Name[MAP_FILE_NAME_LENGTH];
        u32 Length = 0;
        for(char *C = Layer->Name; *C && (Length < MAP_FILE_NAME_LENGTH - 1); ++C)
        {
            if(*C != '"') {Name[Length++] = *C;}
        }
        Name[Length] = 0;
        MAP_OUT("\nlayer \"%s\" %d%s\n", Name, Layer->Z, Layer->Hidden ? " hidden" : "");
        for(u32 Y = 0; Y < Map->Height; ++Y)
        {
            MAP_OUT("row");
            map_cell *Cells = Layer->Cells + Y*Map->Width;
            for(u32 X = 0; X < Map->Width; ++X)
            {
                if(Cells[X].Set) {MAP_OUT(" %d:%d:%d", Cells[X].Set, Cells[X].X, Cells[X].Y);}
                else {MAP_OUT(" .");}
            }
            MAP_OUT("\n");
        }
    }
    if(Map->EntityCount)
    {
        MAP_OUT("\n");
    }
    for(u32 EntityIndex = 0; EntityIndex < Map->EntityCount; ++EntityIndex)
    {
        map_file_entity *Entity = Map->Entities + EntityIndex;
        char GUID[17];
        char Definition[17];
        EntityGUIDToHex(Entity->GUID, GUID);
        EntityGUIDToHex(Entity->Definition, Definition);
        MAP_OUT("entity %s %s %.3f %.3f%s\n", GUID, Definition, Entity->P.x, Entity->P.y, Entity->Flip ? " flip" : "");
        for(u32 OverrideIndex = 0; OverrideIndex < Entity->OverrideCount; ++OverrideIndex)
        {
            map_file_override *Override = Entity->Overrides + OverrideIndex;
            MAP_OUT("override %s %s %s\n", Override->Block, Override->Field, Override->Value);
        }
    }
#undef MAP_OUT

    b32 Result = (Platform.WriteEntireFile(FileName, PlatformFileType_Map, (u8 *)Text, (u32)At) != 0);
    if(!Result)
    {
        LOG_ERROR("Map %s: can't write the file", FileName);
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}
