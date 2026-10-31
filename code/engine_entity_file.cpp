/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): The project's entity types and entity definitions, the text formats: see engine_entity_file.h. Lines
// are read with the tile sets' tokenizer (engine_tileset_file.cpp).

#define ENTITY_PROJECT_FILE_NAME "entity_types.txt"

// NOTE(pvlso): A new definition's GUID: the CPU's clock mixed (splitmix64), never 0
internal u64
NewEntityGUID(void)
{
    local_persist u64 Counter = 0;
    u64 Result = __rdtsc() + 0x9E3779B97F4A7C15ull*(++Counter);
    Result = (Result ^ (Result >> 30))*0xBF58476D1CE4E5B9ull;
    Result = (Result ^ (Result >> 27))*0x94D049BB133111EBull;
    Result ^= (Result >> 31);
    if(!Result) {Result = 1;}

    return(Result);
}

// NOTE(pvlso): 16 hex digits (FormatString only reads 32 bits for %x)
internal void
EntityGUIDToHex(u64 GUID, char *Dest)
{
    char *Digits = "0123456789abcdef";
    for(u32 Index = 0; Index < 16; ++Index)
    {
        Dest[Index] = Digits[(GUID >> (60 - 4*Index)) & 0xF];
    }
    Dest[16] = 0;
}

// NOTE(pvlso): False when Token isn't hex digits
internal b32
EntityGUIDFromHex(char *Token, u64 *GUID)
{
    u64 Value = 0;
    b32 Result = (*Token != 0);
    for(char *C = Token; Result && *C; ++C)
    {
        u32 Digit = 0;
        if((*C >= '0') && (*C <= '9')) {Digit = (u32)(*C - '0');}
        else if((*C >= 'a') && (*C <= 'f')) {Digit = (u32)(*C - 'a') + 10;}
        else if((*C >= 'A') && (*C <= 'F')) {Digit = (u32)(*C - 'A') + 10;}
        else {Result = false;}
        Value = (Value << 4) | Digit;
    }
    if(Result) {*GUID = Value;}

    return(Result);
}

internal s32
FindNameIn(char (*Names)[ENTITY_NAME_LENGTH], u32 Count, char *Name)
{
    s32 Result = -1;
    for(u32 Index = 0; (Result < 0) && (Index < Count); ++Index)
    {
        if(StringsAreEqual(Names[Index], Name)) {Result = (s32)Index;}
    }

    return(Result);
}

// NOTE(pvlso): none, circle x y radius, box x y width height from Tokens[First]
internal b32
ReadEntityShape(tileset_line_tokens *Tokens, u32 First, entity_shape *Shape)
{
    char *Kind = Tokens->Tokens[First];
    u32 Numbers = StringsAreEqual(Kind, "circle") ? 3 : (StringsAreEqual(Kind, "box") ? 4 : 0);
    b32 Result = (StringsAreEqual(Kind, "none") || Numbers) && (Tokens->Count >= First + 1 + Numbers);
    for(u32 Index = First + 1; Result && (Index < First + 1 + Numbers); ++Index)
    {
        Result = IsTilesetNumber(Tokens->Tokens[Index]);
    }
    if(Result)
    {
        *Shape = {};
        if(Numbers)
        {
            Shape->Shape = (Numbers == 3) ? EntityCollision_Circle : EntityCollision_Box;
            Shape->Center = V2(R32FromTilesetToken(Tokens->Tokens[First + 1]), R32FromTilesetToken(Tokens->Tokens[First + 2]));
            Shape->Size.x = R32FromTilesetToken(Tokens->Tokens[First + 3]);
            if(Numbers == 4) {Shape->Size.y = R32FromTilesetToken(Tokens->Tokens[First + 4]);}
        }
    }

    return(Result);
}

// NOTE(pvlso): Calls Line for every line of a text file with words in it, stops when it returns false
#define ENTITY_LINE_CALLBACK(name) b32 name(void *Context, tileset_line_tokens *Tokens, char *FileName, u32 LineNumber)
typedef ENTITY_LINE_CALLBACK(entity_line_callback);

internal b32
ReadEntityLines(char *FileName, platform_file_type Type, memory_arena *Arena, entity_line_callback *Line, void *Context)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    read_file_result File = Platform.ReadEntireFile(FileName, Type, Arena, true);
    b32 Result = (File.Contents != 0);
    tileset_line_tokens *Tokens = PushStruct(Arena, tileset_line_tokens);
    char *At = (char *)File.Contents;
    char *FileEnd = At ? At + StringLength(At) : 0; // NOTE(pvlso): a text file's Size counts its closing zero
    u32 LineNumber = 0;
    while(At && (At < FileEnd))
    {
        char *LineEnd = At;
        while((LineEnd < FileEnd) && (*LineEnd != '\n')) {++LineEnd;}
        ++LineNumber;
        TokenizeTilesetLine(At, LineEnd, Tokens);
        At = (LineEnd < FileEnd) ? LineEnd + 1 : LineEnd;
        if(Tokens->Count && !Line(Context, Tokens, FileName, LineNumber))
        {
            break;
        }
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}

internal
ENTITY_LINE_CALLBACK(ReadEntityProjectLine)
{
    entity_project *Project = (entity_project *)Context;
    char *Key = Tokens->Tokens[0];
    char *Name = (Tokens->Count >= 2) ? Tokens->Tokens[1] : (char *)"";
    if(StringsAreEqual(Key, "type") && Name[0])
    {
        if(FindNameIn(Project->Types, Project->TypeCount, Name) >= 0)
        {
            LOG_ERROR("%s, line %d: the type %s again", FileName, LineNumber, Name);
        }
        else if(Project->TypeCount < ENTITY_MAX_TYPES)
        {
            FormatString(ENTITY_NAME_LENGTH, Project->Types[Project->TypeCount++], "%s", Name);
        }
        else
        {
            LOG_ERROR("%s, line %d: more than %d types", FileName, LineNumber, ENTITY_MAX_TYPES);
        }
    }
    else if(StringsAreEqual(Key, "clip") && Name[0])
    {
        if(Project->ClipCount < ENTITY_MAX_CLIPS)
        {
            FormatString(ENTITY_NAME_LENGTH, Project->Clips[Project->ClipCount++], "%s", Name);
        }
        else
        {
            LOG_ERROR("%s, line %d: more than %d clips", FileName, LineNumber, ENTITY_MAX_CLIPS);
        }
    }
    else
    {
        LOG_ERROR("%s, line %d: can't read %s", FileName, LineNumber, Key);
    }

    return(true);
}

// NOTE(pvlso): The project's data/txts/entity_types.txt, on Arena. Without it the project has one clip, Idle, and no
// types.
internal entity_project *
ReadEntityProject(memory_arena *Arena)
{
    entity_project *Result = PushStruct(Arena, entity_project);
    if(Platform.FileExists(ENTITY_PROJECT_FILE_NAME, PlatformFileType_TXT))
    {
        ReadEntityLines(ENTITY_PROJECT_FILE_NAME, PlatformFileType_TXT, Arena, ReadEntityProjectLine, Result);
    }
    else
    {
        LOG_WARNING("No data/txts/%s, the project's entities have no types", ENTITY_PROJECT_FILE_NAME);
    }
    if(!Result->ClipCount)
    {
        FormatString(ENTITY_NAME_LENGTH, Result->Clips[Result->ClipCount++], "Idle");
    }
    Result->Loaded = true;
    LOG("Read %s: %u types, %u clips", ENTITY_PROJECT_FILE_NAME, Result->TypeCount, Result->ClipCount);

    return(Result);
}

// NOTE(pvlso): A new definition: a GUID of its own, nothing else
internal entity_file
NewEntityDefinition(void)
{
    entity_file Result = {};
    Result.Loaded = true;
    Result.GUID = NewEntityGUID();

    return(Result);
}

struct entity_file_reader
{
    entity_file *Entity;
    entity_project *Project;
};

internal
ENTITY_LINE_CALLBACK(ReadEntityFileLine)
{
    entity_file_reader *Reader = (entity_file_reader *)Context;
    entity_file *Entity = Reader->Entity;
    entity_project *Project = Reader->Project;
    char *Key = Tokens->Tokens[0];
    b32 Valid = (Tokens->Count >= 2);
    if(!Valid)
    {
    }
    else if(StringsAreEqual(Key, "guid"))
    {
        Valid = EntityGUIDFromHex(Tokens->Tokens[1], &Entity->GUID);
    }
    else if(StringsAreEqual(Key, "display"))
    {
        FormatString(ENTITY_DISPLAY_LENGTH, Entity->DisplayName, "%s", Tokens->Tokens[1]);
    }
    else if(StringsAreEqual(Key, "type"))
    {
        s32 Type = FindNameIn(Project->Types, Project->TypeCount, Tokens->Tokens[1]);
        Valid = (Type >= 0);
        if(Valid) {Entity->Type = (u32)Type + 1;}
    }
    else if(StringsAreEqual(Key, "clip"))
    {
        s32 Clip = FindNameIn(Project->Clips, Project->ClipCount, Tokens->Tokens[1]);
        entity_look_kind Kind = ((Tokens->Count < 4) ? EntityLook_None :
                                 (StringsAreEqual(Tokens->Tokens[2], "bitmap") ? EntityLook_Bitmap :
                                  (StringsAreEqual(Tokens->Tokens[2], "spritesheet") ? EntityLook_SpriteSheet :
                                   EntityLook_None)));
        Valid = (Clip >= 0) && (Kind != EntityLook_None);
        if(Valid)
        {
            entity_clip *Look = Entity->Clips + Clip;
            Look->Kind = Kind;
            FormatString(ENTITY_LOOK_NAME_LENGTH, Look->FileName, "%s", Tokens->Tokens[3]);
            Look->FramesPerSecond = 8.0f;
            Look->Loop = true;
            for(u32 Index = 4; Index < Tokens->Count; ++Index)
            {
                if(StringsAreEqual(Tokens->Tokens[Index], "fps") && (Index + 1 < Tokens->Count))
                {
                    Look->FramesPerSecond = R32FromTilesetToken(Tokens->Tokens[++Index]);
                }
                else if(StringsAreEqual(Tokens->Tokens[Index], "once")) {Look->Loop = false;}
                else if(StringsAreEqual(Tokens->Tokens[Index], "loop")) {Look->Loop = true;}
            }
        }
    }
    else if(StringsAreEqual(Key, "footprint"))
    {
        Valid = ReadEntityShape(Tokens, 1, &Entity->Footprint);
    }
    else if(StringsAreEqual(Key, "hitarea"))
    {
        Valid = ReadEntityShape(Tokens, 1, &Entity->HitArea);
    }
    else
    {
        Valid = false;
    }

    if(!Valid)
    {
        LOG_ERROR("Entity %s, line %d: can't read the %s line (not in data/txts/%s?)", FileName, LineNumber, Key,
                  ENTITY_PROJECT_FILE_NAME);
        ++Entity->ErrorCount;
    }

    return(true);
}

internal entity_file
ReadEntityFile(char *FileName, entity_project *Project, memory_arena *Arena)
{
    entity_file Result = {};
    entity_file_reader Reader = {&Result, Project};
    Result.Loaded = ReadEntityLines(FileName, PlatformFileType_Entity, Arena, ReadEntityFileLine, &Reader);
    if(!Result.Loaded)
    {
        LOG_ERROR("Entity %s: can't read the file", FileName);
    }

    return(Result);
}

// NOTE(pvlso): A text value in quotes, without the quotes it may hold
internal void
CopyWithoutQuotes(char *Source, char *Dest, u32 DestSize)
{
    u32 Length = 0;
    for(char *C = Source; *C && (Length + 1 < DestSize); ++C)
    {
        if(*C != '"') {Dest[Length++] = *C;}
    }
    Dest[Length] = 0;
}

internal umm
WriteEntityShape(char *Dest, umm DestSize, char *Key, entity_shape *Shape)
{
    umm Result = 0;
    switch(Shape->Shape)
    {
        case EntityCollision_None:
        {
            Result = FormatString(DestSize, Dest, "%s none\n", Key);
        } break;

        case EntityCollision_Circle:
        {
            Result = FormatString(DestSize, Dest, "%s circle %.2f %.2f %.2f\n", Key, Shape->Center.x, Shape->Center.y,
                                  Shape->Size.x);
        } break;

        case EntityCollision_Box:
        {
            Result = FormatString(DestSize, Dest, "%s box %.2f %.2f %.2f %.2f\n", Key, Shape->Center.x, Shape->Center.y,
                                  Shape->Size.x, Shape->Size.y);
        } break;

        InvalidDefaultCase;
    }

    return(Result);
}

internal b32
WriteEntityFile(char *FileName, entity_file *Entity, entity_project *Project, memory_arena *Arena)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    umm Capacity = 4096 + (umm)ENTITY_MAX_CLIPS*(ENTITY_LOOK_NAME_LENGTH + 96);
    char *Text = (char *)PushSize(Arena, Capacity);
    umm At = 0;
#define ENTITY_OUT(...) At += FormatString(Capacity - At, Text + At, __VA_ARGS__)
    char Hex[17];
    char Clean[ENTITY_LOOK_NAME_LENGTH];
    ENTITY_OUT("; An entity (Kaban engine), written by the entity editor. Format: engine_entity_file.h\n");
    EntityGUIDToHex(Entity->GUID, Hex);
    ENTITY_OUT("guid %s\n", Hex);
    CopyWithoutQuotes(Entity->DisplayName, Clean, sizeof(Clean));
    ENTITY_OUT("display \"%s\"\n", Clean);
    if(Entity->Type && (Entity->Type <= Project->TypeCount))
    {
        ENTITY_OUT("type %s\n", Project->Types[Entity->Type - 1]);
    }
    for(u32 ClipIndex = 0; ClipIndex < Project->ClipCount; ++ClipIndex)
    {
        entity_clip *Clip = Entity->Clips + ClipIndex;
        if(Clip->Kind == EntityLook_Bitmap)
        {
            CopyWithoutQuotes(Clip->FileName, Clean, sizeof(Clean));
            ENTITY_OUT("clip %s bitmap \"%s\"\n", Project->Clips[ClipIndex], Clean);
        }
        else if(Clip->Kind == EntityLook_SpriteSheet)
        {
            CopyWithoutQuotes(Clip->FileName, Clean, sizeof(Clean));
            ENTITY_OUT("clip %s spritesheet \"%s\" fps %.2f%s\n", Project->Clips[ClipIndex], Clean,
                       Clip->FramesPerSecond, Clip->Loop ? "" : " once");
        }
    }
    At += WriteEntityShape(Text + At, Capacity - At, "footprint", &Entity->Footprint);
    At += WriteEntityShape(Text + At, Capacity - At, "hitarea", &Entity->HitArea);
#undef ENTITY_OUT

    b32 Result = (Platform.WriteEntireFile(FileName, PlatformFileType_Entity, (u8 *)Text, (u32)At) != 0);
    if(!Result)
    {
        LOG_ERROR("Entity %s: can't write the file", FileName);
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}
