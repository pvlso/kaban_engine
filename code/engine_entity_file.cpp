/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): The project's entity blocks and entity definitions, the text formats: see engine_entity_file.h. Lines
// are read with the tile sets' tokenizer (engine_tileset_file.cpp).

#define ENTITY_PROJECT_FILE_NAME "entity_blocks.txt"

global_variable char *EntityFieldKindNames[EntityField_Count] = {"int", "number", "bool", "text", "choice", "entity"};

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

internal s32
FindEntityBlock(entity_project *Project, char *Name)
{
    s32 Result = -1;
    for(u32 BlockIndex = 0; (Result < 0) && (BlockIndex < Project->BlockCount); ++BlockIndex)
    {
        if(StringsAreEqual(Project->Blocks[BlockIndex].Name, Name)) {Result = (s32)BlockIndex;}
    }

    return(Result);
}

internal s32
FindEntityBlockField(entity_block_def *Block, char *Name)
{
    s32 Result = -1;
    for(u32 FieldIndex = 0; (Result < 0) && (FieldIndex < Block->FieldCount); ++FieldIndex)
    {
        if(StringsAreEqual(Block->Fields[FieldIndex].Name, Name)) {Result = (s32)FieldIndex;}
    }

    return(Result);
}

// NOTE(pvlso): False when Token isn't a value of that field
internal b32
ReadEntityFieldValue(entity_field_def *Field, char *Token, entity_field_value *Value)
{
    b32 Result = true;
    switch(Field->Kind)
    {
        case EntityField_Int:
        {
            Result = IsTilesetNumber(Token);
            if(Result) {Value->Int = RoundReal32ToInt32(R32FromTilesetToken(Token));}
        } break;

        case EntityField_Number:
        {
            Result = IsTilesetNumber(Token);
            if(Result) {Value->Number = R32FromTilesetToken(Token);}
        } break;

        case EntityField_Bool:
        {
            if(StringsAreEqual(Token, "yes") || StringsAreEqual(Token, "true") || StringsAreEqual(Token, "1"))
            {
                Value->Bool = true;
            }
            else if(StringsAreEqual(Token, "no") || StringsAreEqual(Token, "false") || StringsAreEqual(Token, "0"))
            {
                Value->Bool = false;
            }
            else
            {
                Result = false;
            }
        } break;

        case EntityField_Text:
        {
            FormatString(ENTITY_TEXT_LENGTH, Value->Text, "%s", Token);
        } break;

        case EntityField_Choice:
        {
            s32 Choice = FindNameIn(Field->Choices, Field->ChoiceCount, Token);
            Result = (Choice >= 0);
            if(Result) {Value->Int = Choice;}
        } break;

        case EntityField_Entity:
        {
            Result = EntityGUIDFromHex(Token, &Value->Entity);
        } break;

        InvalidDefaultCase;
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

struct entity_project_reader
{
    entity_project *Project;
    entity_block_def *Block;
};

internal
ENTITY_LINE_CALLBACK(ReadEntityProjectLine)
{
    entity_project_reader *Reader = (entity_project_reader *)Context;
    entity_project *Project = Reader->Project;
    char *Key = Tokens->Tokens[0];
    char *Name = (Tokens->Count >= 2) ? Tokens->Tokens[1] : (char *)"";
    if(StringsAreEqual(Key, "category") && Name[0])
    {
        if(Project->CategoryCount < ENTITY_MAX_CATEGORIES)
        {
            FormatString(ENTITY_NAME_LENGTH, Project->Categories[Project->CategoryCount++], "%s", Name);
        }
        else
        {
            LOG_ERROR("%s, line %d: more than %d categories", FileName, LineNumber, ENTITY_MAX_CATEGORIES);
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
    else if(StringsAreEqual(Key, "block") && Name[0])
    {
        Reader->Block = 0;
        if(FindEntityBlock(Project, Name) >= 0)
        {
            LOG_ERROR("%s, line %d: the block %s again", FileName, LineNumber, Name);
        }
        else if(Project->BlockCount < ENTITY_MAX_BLOCKS)
        {
            Reader->Block = Project->Blocks + Project->BlockCount++;
            FormatString(ENTITY_NAME_LENGTH, Reader->Block->Name, "%s", Name);
        }
        else
        {
            LOG_ERROR("%s, line %d: more than %d blocks", FileName, LineNumber, ENTITY_MAX_BLOCKS);
        }
    }
    else if(StringsAreEqual(Key, "field") && (Tokens->Count >= 3))
    {
        entity_block_def *Block = Reader->Block;
        s32 Kind = -1;
        for(u32 KindIndex = 0; KindIndex < EntityField_Count; ++KindIndex)
        {
            if(StringsAreEqual(Tokens->Tokens[2], EntityFieldKindNames[KindIndex])) {Kind = (s32)KindIndex;}
        }
        if(!Block)
        {
            LOG_ERROR("%s, line %d: the field %s isn't under a block", FileName, LineNumber, Name);
        }
        else if(Kind < 0)
        {
            LOG_ERROR("%s, line %d: %s isn't a field kind (int, number, bool, text, choice, entity)", FileName,
                      LineNumber, Tokens->Tokens[2]);
        }
        else if(Block->FieldCount >= ENTITY_MAX_BLOCK_FIELDS)
        {
            LOG_ERROR("%s, line %d: %s has more than %d fields", FileName, LineNumber, Block->Name,
                      ENTITY_MAX_BLOCK_FIELDS);
        }
        else
        {
            entity_field_def *Field = Block->Fields + Block->FieldCount++;
            FormatString(ENTITY_NAME_LENGTH, Field->Name, "%s", Name);
            Field->Kind = (entity_field_kind)Kind;
            if(Field->Kind == EntityField_Choice)
            {
                for(u32 Index = 3; Index < Tokens->Count; ++Index)
                {
                    if(Field->ChoiceCount < ENTITY_MAX_CHOICES)
                    {
                        FormatString(ENTITY_NAME_LENGTH, Field->Choices[Field->ChoiceCount++], "%s", Tokens->Tokens[Index]);
                    }
                }
                if(!Field->ChoiceCount)
                {
                    LOG_ERROR("%s, line %d: the choice %s lists no words", FileName, LineNumber, Name);
                    FormatString(ENTITY_NAME_LENGTH, Field->Choices[Field->ChoiceCount++], "None");
                }
            }
            else if((Field->Kind != EntityField_Entity) && (Tokens->Count >= 4) &&
                    !ReadEntityFieldValue(Field, Tokens->Tokens[3], &Field->Default))
            {
                LOG_ERROR("%s, line %d: %s isn't a %s value", FileName, LineNumber, Tokens->Tokens[3],
                          EntityFieldKindNames[Kind]);
            }
        }
    }
    else
    {
        LOG_ERROR("%s, line %d: can't read %s", FileName, LineNumber, Key);
    }

    return(true);
}

// NOTE(pvlso): The project's data/txts/entity_blocks.txt, on Arena. Without it the project has one clip, Idle, no
// categories and no blocks.
internal entity_project *
ReadEntityProject(memory_arena *Arena)
{
    entity_project *Result = PushStruct(Arena, entity_project);
    if(Platform.FileExists(ENTITY_PROJECT_FILE_NAME, PlatformFileType_TXT))
    {
        entity_project_reader Reader = {Result, 0};
        ReadEntityLines(ENTITY_PROJECT_FILE_NAME, PlatformFileType_TXT, Arena, ReadEntityProjectLine, &Reader);
    }
    else
    {
        LOG_WARNING("No data/txts/%s, the project's entities have no categories or blocks", ENTITY_PROJECT_FILE_NAME);
    }
    if(!Result->ClipCount)
    {
        FormatString(ENTITY_NAME_LENGTH, Result->Clips[Result->ClipCount++], "Idle");
    }
    Result->Loaded = true;
    LOG("Read %s: %u categories, %u clips, %u blocks", ENTITY_PROJECT_FILE_NAME, Result->CategoryCount,
        Result->ClipCount, Result->BlockCount);

    return(Result);
}

// NOTE(pvlso): A block's fields at their starting values
internal void
ResetEntityBlock(entity_file *Entity, entity_project *Project, u32 BlockIndex)
{
    entity_block_def *Block = Project->Blocks + BlockIndex;
    for(u32 FieldIndex = 0; FieldIndex < ENTITY_MAX_BLOCK_FIELDS; ++FieldIndex)
    {
        Entity->Fields[BlockIndex][FieldIndex] = (FieldIndex < Block->FieldCount) ?
            Block->Fields[FieldIndex].Default : entity_field_value{};
    }
}

// NOTE(pvlso): A new definition: a GUID of its own, nothing else
internal entity_file
NewEntityDefinition(entity_project *Project)
{
    entity_file Result = {};
    Result.Loaded = true;
    Result.GUID = NewEntityGUID();
    for(u32 BlockIndex = 0; BlockIndex < Project->BlockCount; ++BlockIndex)
    {
        ResetEntityBlock(&Result, Project, BlockIndex);
    }

    return(Result);
}

struct entity_file_reader
{
    entity_file *Entity;
    entity_project *Project;
    s32 Block;
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
    else if(StringsAreEqual(Key, "category"))
    {
        s32 Category = FindNameIn(Project->Categories, Project->CategoryCount, Tokens->Tokens[1]);
        Valid = (Category >= 0);
        if(Valid) {Entity->Category = (u32)Category + 1;}
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
    else if(StringsAreEqual(Key, "block"))
    {
        Reader->Block = FindEntityBlock(Project, Tokens->Tokens[1]);
        Valid = (Reader->Block >= 0);
        if(Valid) {Entity->Blocks |= (1u << Reader->Block);}
    }
    else if(StringsAreEqual(Key, "field") && (Tokens->Count >= 3) && (Reader->Block >= 0))
    {
        entity_block_def *Block = Project->Blocks + Reader->Block;
        s32 Field = FindEntityBlockField(Block, Tokens->Tokens[1]);
        Valid = (Field >= 0) && ReadEntityFieldValue(Block->Fields + Field, Tokens->Tokens[2],
                                                     &Entity->Fields[Reader->Block][Field]);
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
    for(u32 BlockIndex = 0; BlockIndex < Project->BlockCount; ++BlockIndex)
    {
        ResetEntityBlock(&Result, Project, BlockIndex);
    }

    entity_file_reader Reader = {&Result, Project, -1};
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

// NOTE(pvlso): A field's value as a file writes it (a text in quotes), for ReadEntityFieldValue to read back
internal umm
FormatEntityFieldValue(char *Dest, umm DestSize, entity_field_def *Field, entity_field_value *Value)
{
    umm Result = 0;
    switch(Field->Kind)
    {
        case EntityField_Int:    {Result = FormatString(DestSize, Dest, "%d", Value->Int);}                break;
        case EntityField_Number: {Result = FormatString(DestSize, Dest, "%.3f", Value->Number);}           break;
        case EntityField_Bool:   {Result = FormatString(DestSize, Dest, "%s", Value->Bool ? "yes" : "no");} break;
        case EntityField_Text:
        {
            char Clean[ENTITY_TEXT_LENGTH];
            CopyWithoutQuotes(Value->Text, Clean, sizeof(Clean));
            Result = FormatString(DestSize, Dest, "\"%s\"", Clean);
        } break;
        case EntityField_Choice:
        {
            u32 Choice = ((u32)Value->Int < Field->ChoiceCount) ? (u32)Value->Int : 0;
            Result = FormatString(DestSize, Dest, "%s", Field->Choices[Choice]);
        } break;
        case EntityField_Entity:
        {
            char Hex[17];
            EntityGUIDToHex(Value->Entity, Hex);
            Result = FormatString(DestSize, Dest, "%s", Hex);
        } break;
        InvalidDefaultCase;
    }

    return(Result);
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
    umm Capacity = 4096 + (umm)ENTITY_MAX_CLIPS*(ENTITY_LOOK_NAME_LENGTH + 96) +
        (umm)ENTITY_MAX_BLOCKS*(64 + ENTITY_MAX_BLOCK_FIELDS*(ENTITY_TEXT_LENGTH + 64));
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
    if(Entity->Category && (Entity->Category <= Project->CategoryCount))
    {
        ENTITY_OUT("category %s\n", Project->Categories[Entity->Category - 1]);
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
    for(u32 BlockIndex = 0; BlockIndex < Project->BlockCount; ++BlockIndex)
    {
        if(!(Entity->Blocks & (1u << BlockIndex)))
        {
            continue;
        }

        entity_block_def *Block = Project->Blocks + BlockIndex;
        ENTITY_OUT("block %s\n", Block->Name);
        for(u32 FieldIndex = 0; FieldIndex < Block->FieldCount; ++FieldIndex)
        {
            entity_field_def *Field = Block->Fields + FieldIndex;
            ENTITY_OUT("field %s ", Field->Name);
            At += FormatEntityFieldValue(Text + At, Capacity - At, Field, &Entity->Fields[BlockIndex][FieldIndex]);
            ENTITY_OUT("\n");
        }
    }
#undef ENTITY_OUT

    b32 Result = (Platform.WriteEntireFile(FileName, PlatformFileType_Entity, (u8 *)Text, (u32)At) != 0);
    if(!Result)
    {
        LOG_ERROR("Entity %s: can't write the file", FileName);
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}
